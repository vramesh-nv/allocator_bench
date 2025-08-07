#include "va_allocator.arenas.h"
#include "common.h"
#include "radix.h"
#include "bitvector.h"
#include "addrtracker.h"
#include "buddy.h"

#define NUM_ARENAS 8

// Forward declaration
typedef struct arena arena_t;
typedef struct arena_reservation arena_reservation_t;

typedef struct slab_allocator {
    uint64_t block_size;        // Size of each block in the slab
    uint64_t blocks_per_slab;   // Number of blocks in this slab
    uint64_t free_blocks;       // Number of free blocks
    CUbitvector *bitmap;
    arena_reservation_t *parent_reservation;
} slab_allocator_t;

typedef struct block_range {
    uint64_t low_idx;  // inclusive
    uint64_t high_idx; // inclusive
} block_range_t;

typedef struct va_block {
    uint64_t start_addr;         // Starting address of the block
    uint64_t size;               // Size of the block
    block_range_t block_range;  // Range of physical blocks that the block occupies
    int is_free;                 // Whether the block is free
    struct va_block *addr_next;  // Next block in address-ordered list
    struct va_block *addr_prev;  // Previous block in address-ordered list
    CUradixNode radix_node;      // Node in size-ordered radix tree
} va_block_t;

typedef struct {
    va_block_t *addr_list;      // List ordered by address
    CUradixTree size_tree;      // Tree ordered by size
    arena_reservation_t *parent_reservation;
} obj_allocator_t;

typedef struct arena_reservation {
    uint64_t size;
    uint64_t addr;
    CUIaddrTrackerNode node;
    arena_t *parent_arena;
    void *strategy;
    uint64_t block_size_log2;
    uint64_t num_blocks;
    buddy_alloc_block_t **buddy_blocks;
    uint32_t *ref_count;
    buddy_allocator_t *buddy_allocator;
    arena_reservation_t *next;
} arena_reservation_t;

/*typedef struct arena_object {
    uint64_t addr;
    uint64_t size;
    arena_reservation_t *reservation;
} arena_object_t;*/

typedef struct arena_info {
    uint64_t max_per_alloc_size;
    uint64_t reservation_size;
    uint64_t backing_memory_block_size;
} arena_info_t;

typedef struct arena {
    arena_info_t info; // Arena info: max_per_alloc_size, reservation_size
    uint64_t idx;      // Arena index
    int is_slab;       // Whether the arena is managed by a slab allocator strategy
    void *parent;      // Pointer to the parent allocator
    arena_reservation_t *reservation_head; // Head of the reservation list
} arena_t;

//
// Arena implementation structure
//
typedef struct {
    arena_t arenas[NUM_ARENAS];
    uint64_t total_va_size;       // Total VA space size
    uint64_t used_va_size;        // Currently used VA space
    CUIaddrTracker res_tracker;   // address to reservation tracker.
} va_allocator_arenas_t;

//
// Arbitrary arena sizes to reservation table:
// <= 512b -> 2MB
// <= 1KB -> 2MB
// <= 2KB -> 4MB
// <= 4KB -> 8MB
// <= 64KB -> 32MB
// <= 2MB -> 64MB
// <= 32MB -> 512MB
// > 32MB -> physical memory size
//
arena_info_t arena_info_table[NUM_ARENAS] = {
    {512UL, 2UL * 1024UL * 1024UL, 2UL * 1024UL * 1024UL},
    {1024UL, 2UL * 1024UL * 1024UL, 2UL * 1024UL * 1024UL},
    {2048UL, 4UL * 1024UL * 1024UL, 2UL * 1024UL * 1024UL},
    {4096UL, 8UL * 1024UL * 1024UL, 2UL * 1024UL * 1024UL},
    {64UL * 1024UL, 32UL * 1024UL * 1024UL, 32UL * 1024UL * 1024UL},
    {2UL * 1024UL * 1024UL, 64UL * 1024UL * 1024UL, 32UL * 1024UL * 1024UL},
    {32UL * 1024UL * 1024UL, 512UL * 1024UL * 1024UL, 32UL * 1024UL * 1024UL},
    {~0UL, PHYSICAL_MEMORY_SIZE, 32UL * 1024UL * 1024UL}
};

//
// Again, this is arbitrary.
//
static inline int
is_arena_idx_slab(uint64_t arena_idx) { return (arena_idx < 3) ? 1 : 0; }

static uint64_t
get_arena_idx_for_size(uint64_t size)
{
    for (uint64_t i = 0; i < NUM_ARENAS; i++) {
        if (arena_info_table[i].max_per_alloc_size >= size) {
            return i;
        }
    }
    // There should be no case where the size is greater than the max size of the last arena.
    assert(0);
    return NUM_ARENAS;
}

//
// Slab allocator functions:
//
// initialize_slab
// deinitialize_slab
// allocate_from_slab
// free_to_slab
//
static void
deinitialize_slab(slab_allocator_t *sa)
{
    assert(sa);
    if (sa->bitmap) {
        assert(!cubitvectorIsAnyBitSet(sa->bitmap));
        cubitvectorDestroy(sa->bitmap);
        sa->bitmap = NULL;
    }
    free(sa);
    return;
}

static void *
initialize_slab(arena_reservation_t *reservation)
{
    assert(reservation && (reservation->strategy == NULL));

    slab_allocator_t *sa = (slab_allocator_t *)calloc(1, sizeof(*sa));
    if (!sa) {
        return NULL;
    }

    sa->block_size = reservation->parent_arena->info.max_per_alloc_size;
    sa->blocks_per_slab = reservation->parent_arena->info.reservation_size / sa->block_size;
    sa->free_blocks = sa->blocks_per_slab;
    sa->parent_reservation = reservation;

    CUbitvector *bitmap = NULL;
    cubitvectorCreate(&bitmap, sa->blocks_per_slab);
    if (!bitmap) {
        free(sa);
        return NULL;
    }

    sa->bitmap = bitmap;
    return sa;
}

static uint64_t
allocate_from_slab(slab_allocator_t *sa, block_range_t *block_range)
{
    assert(sa);
    if (sa->free_blocks == 0) {
        return 0;
    }

    NvU64 bit = 0;
    if (!cubitvectorFindLowestClearBitInRange(sa->bitmap, 0, sa->blocks_per_slab - 1, &bit)) {
        return 0;
    }
    cubitvectorSetBit(sa->bitmap, bit);
    sa->free_blocks--;


    uint64_t va = sa->parent_reservation->node.addr + (sa->block_size * bit);
    block_range->low_idx = (va - sa->parent_reservation->addr) >> sa->parent_reservation->block_size_log2;
    block_range->high_idx = block_range->low_idx;

    return va;
}

static void
free_to_slab(slab_allocator_t *sa, uint64_t addr, block_range_t *block_range)
{
    assert(sa);
    assert(addr >= sa->parent_reservation->addr);
    assert(addr < (sa->parent_reservation->addr + sa->parent_reservation->size));

    uint64_t bit = (addr - sa->parent_reservation->addr) / sa->block_size;
    cubitvectorClearBit(sa->bitmap, bit);
    sa->free_blocks++;

    if (!block_range) {
        return;
    }

    uint64_t va = sa->parent_reservation->node.addr + (sa->block_size * bit);
    block_range->low_idx = (va - sa->parent_reservation->addr) >> sa->parent_reservation->block_size_log2;
    block_range->high_idx = block_range->low_idx;

    return;
}

//
// Object allocator functions:
//
// initialize_obj_allocator
// deinitialize_obj_allocator
// allocate_from_obj_allocator
// free_to_obj_allocator
// insert_addr_list (Helper function to insert block into address-ordered list)
// remove_addr_list (Helper function to remove block from address-ordered list)
//
static void
insert_addr_list(obj_allocator_t *oa, va_block_t *block) {
    va_block_t *current = oa->addr_list;
    va_block_t *prev = NULL;
    while (current && current->start_addr < block->start_addr) {
        prev = current;
        current = current->addr_next;
    }
    block->addr_next = current;
    block->addr_prev = prev;
    if (prev) {
        prev->addr_next = block;
    } else {
        oa->addr_list = block;
    }
    if (current) {
        current->addr_prev = block;
    }
}

static void
remove_addr_list(obj_allocator_t *oa, va_block_t *block) {
    if (block->addr_prev) {
        block->addr_prev->addr_next = block->addr_next;
    } else {
        oa->addr_list = block->addr_next;
    }
    if (block->addr_next) {
        block->addr_next->addr_prev = block->addr_prev;
    }
}

static void
free_to_obj_allocator(obj_allocator_t *oa, uint64_t addr, block_range_t *block_range)
{
    assert(oa);
    assert(addr >= oa->parent_reservation->addr);
    assert(addr < (oa->parent_reservation->addr + oa->parent_reservation->size));

    va_block_t *block = oa->addr_list;
    while (block && block->start_addr != addr) {
        block = block->addr_next;
    }
    if (!block || block->is_free) {
        return;
    }

    if (block_range) {
        uint64_t bz = 1ULL << oa->parent_reservation->block_size_log2;
        uint64_t offset = block->start_addr - oa->parent_reservation->addr;
        block_range->low_idx = offset / bz;
        block_range->high_idx = (offset + block->size) % bz != 0 ? (offset + block->size) / bz : (offset + block->size) / bz - 1;
    }

    block->is_free = 1;
    va_block_t *prev = block->addr_prev;
    va_block_t *next = block->addr_next;

    assert(!prev || (prev && (block->start_addr == prev->start_addr + prev->size)));
    if (prev && prev->is_free) {
        prev->size += block->size;
        remove_addr_list(oa, block);
        radixTreeRemove(&prev->radix_node);
        free(block);
        block = prev;
    }

    assert(!next || (next && (block->start_addr + block->size == next->start_addr)));
    if (next && next->is_free) {
        block->size += next->size;
        remove_addr_list(oa, next);
        radixTreeRemove(&next->radix_node);
        free(next);
    }
    radixTreeInsert(&oa->size_tree, &block->radix_node, block->size);
}

static uint64_t
allocate_from_obj_allocator(obj_allocator_t *oa, uint64_t size, block_range_t *block_range)
{
    assert(oa);

    CUradixNode *node = radixTreeFindGEQ(&oa->size_tree, size);
    if (!node) {
        return 0;
    }

    va_block_t *best_fit = container_of(node, va_block_t, radix_node);
    // Split block if necessary
    if (best_fit->size > size) {
        va_block_t *new_block = calloc(1, sizeof(*new_block));
        if (!new_block) {
            return 0;
        }
        new_block->start_addr = best_fit->start_addr + size;
        new_block->size = best_fit->size - size;
        new_block->is_free = 1;
        new_block->addr_next = NULL;
        new_block->addr_prev = NULL;

        uint64_t bz = 1ULL << oa->parent_reservation->block_size_log2;
        uint64_t offset = new_block->start_addr - oa->parent_reservation->addr;
        new_block->block_range.low_idx = offset / bz;
        new_block->block_range.high_idx = best_fit->block_range.high_idx;

        // Update the best fit block's size to reflect this split
        best_fit->size = size;
        offset = best_fit->start_addr - oa->parent_reservation->addr;
        best_fit->block_range.high_idx = (offset + best_fit->size) % bz != 0 ? (offset + best_fit->size) / bz : (offset + best_fit->size) / bz - 1;

        insert_addr_list(oa, new_block);
        radixTreeInsert(&oa->size_tree, &new_block->radix_node, new_block->size);
    }

    // Mark the best fit block as in use
    best_fit->is_free = 0;
    radixTreeRemove(&best_fit->radix_node);
    *block_range = best_fit->block_range;
    return best_fit->start_addr;
}

static void
deinitialize_obj_allocator(obj_allocator_t *oa)
{
    assert(oa);

    va_block_t *block = oa->addr_list;
    while (block) {
        va_block_t *next = block->addr_next;
        free(block);
        block = next;
    }

    free(oa);
    return;
}

static obj_allocator_t *
initialize_obj_allocator(arena_reservation_t *reservation)
{
    assert(reservation && (reservation->strategy == NULL));

    obj_allocator_t *oa = (obj_allocator_t *)calloc(1, sizeof(*oa));
    if (!oa) {
        return NULL;
    }

    oa->parent_reservation = reservation;
    oa->addr_list = NULL;
    radixTreeInit(&oa->size_tree, 63);  

    va_block_t *block = (va_block_t *)calloc(1, sizeof(*block));
    if (!block) {
        free(oa);
        return NULL;
    }

    block->start_addr = reservation->addr;
    block->size = reservation->size;
    block->is_free = 1;
    block->addr_next = NULL;
    block->addr_prev = NULL;
    block->block_range.low_idx = 0;
    block->block_range.high_idx = reservation->num_blocks - 1;
    oa->addr_list = block;

    radixTreeInsert(&oa->size_tree, &block->radix_node, block->size);

    return oa;
}

//
// Reservation functions:
//
// destroy_reservation
// create_reservation
// allocate_from_reservation
//
static void
destroy_reservation(arena_reservation_t *reservation)
{
    if (!reservation) {
        return;
    }
    cuiAddrTrackerUnregisterNode(&reservation->node);
    if (reservation->parent_arena->is_slab) {
        // Slab allocation
        deinitialize_slab((slab_allocator_t *)reservation->strategy);
    } else {
        // Object allocation
        deinitialize_obj_allocator((obj_allocator_t *)reservation->strategy);
    }

    FREE_VA(UINT2PTR(reservation->addr), reservation->size);
    memset(reservation, 0, sizeof(*reservation));
    free(reservation);
    return;
}

static arena_reservation_t *
create_reservation(arena_t *arena)
{
    assert(arena);
    arena_reservation_t *reservation = (arena_reservation_t *)calloc(1, sizeof(*reservation));
    if (!reservation) {
        return NULL;
    }

    uint64_t addr = PTR2UINT(RESERVE_VA(arena->info.reservation_size));
    if (!addr) {
        free(reservation);
        return NULL;
    }

    reservation->addr = addr;
    reservation->size = arena->info.reservation_size;
    reservation->parent_arena = arena;

    COMPUTE_LOG2(reservation->block_size_log2, arena->info.backing_memory_block_size);


    reservation->num_blocks = arena->info.reservation_size / arena->info.backing_memory_block_size;

    reservation->buddy_allocator = buddy_allocator_create(arena->info.backing_memory_block_size);
    if (!reservation->buddy_allocator) {
        FREE_VA(UINT2PTR(addr), arena->info.reservation_size);
        free(reservation);
        return NULL;
    }

    reservation->buddy_blocks = (buddy_alloc_block_t **)calloc(reservation->num_blocks, sizeof(*reservation->buddy_blocks));
    if (!reservation->buddy_blocks) {
        buddy_allocator_destroy(reservation->buddy_allocator);
        FREE_VA(UINT2PTR(addr), arena->info.reservation_size);
        free(reservation);
        return NULL;
    }

    reservation->ref_count = (uint32_t *)calloc(reservation->num_blocks, sizeof(*reservation->ref_count));
    if (!reservation->ref_count) {
        buddy_allocator_destroy(reservation->buddy_allocator);
        free(reservation->buddy_blocks);
        FREE_VA(UINT2PTR(addr), arena->info.reservation_size);
        free(reservation);
        return NULL;
    }

    void *strategy = (arena->is_slab) ? (void *)initialize_slab(reservation) 
                                      : (void *)initialize_obj_allocator(reservation);
    if (!strategy) {
        buddy_allocator_destroy(reservation->buddy_allocator);
        free(reservation->buddy_blocks);
        free(reservation->ref_count);
        FREE_VA(UINT2PTR(addr), arena->info.reservation_size);
        free(reservation);
        return NULL;
    }

    reservation->strategy = strategy;
    va_allocator_arenas_t *arena_impl = (va_allocator_arenas_t *)arena->parent;
    cuiAddrTrackerRegisterNode(&arena_impl->res_tracker, &reservation->node, addr, arena->info.reservation_size, reservation);
    arena_impl->total_va_size += arena->info.reservation_size;
    return reservation;
}

static int
back_with_buddy_allocator(arena_reservation_t *reservation, block_range_t block_range)
{
    uint64_t ret = 0;
    uint64_t first_unbacked_idx = UINT64_MAX;
    uint64_t num_unbacked = 0;
    uint64_t bz = 1ULL << reservation->block_size_log2;

    for (uint64_t i = block_range.low_idx; i <= block_range.high_idx; i++) {
        if (reservation->buddy_blocks[i] == NULL) {
            if (first_unbacked_idx == UINT64_MAX) {
                first_unbacked_idx = i;
            }
            num_unbacked++;
        }
    }

    // No unbacked blocks.
    /*if (num_unbacked == 0) {
        assert(first_unbacked_idx == UINT64_MAX);
        return 0;
    }

    assert(first_unbacked_idx >= block_range.low_idx);
    assert(first_unbacked_idx <= block_range.high_idx);*/

    uint64_t unbacked_idx = first_unbacked_idx;

    for (uint64_t i = 0; i < reservation->num_blocks && num_unbacked > 0; i++) {
        if (i >= block_range.low_idx && i <= block_range.high_idx) {
            continue;
        }

        if (!reservation->buddy_blocks[i] || reservation->ref_count[i] > 0) {
            continue;
        }

        uint64_t old_va = reservation->addr + i * bz;
        ret = buddy_unmap(reservation->buddy_blocks[i], old_va, bz);
        assert(ret == 0);

        reservation->buddy_blocks[unbacked_idx] = reservation->buddy_blocks[i];
        reservation->buddy_blocks[i] = NULL;
        reservation->ref_count[i] = 0;

        uint64_t new_va = reservation->addr + unbacked_idx * bz;
        ret = buddy_map(reservation->buddy_blocks[unbacked_idx], new_va, bz);
        assert(ret == 0);

        num_unbacked--;

        if (num_unbacked == 0) {
            break;
        }

        uint64_t j = unbacked_idx + 1;
        for (; j <= block_range.high_idx; j++) {
            if (reservation->buddy_blocks[j] == NULL) {
                break;
            }
        }
        
        if (j > block_range.high_idx) {
            break;
        }
        unbacked_idx = j;
    }

    for (uint64_t i = block_range.low_idx; i <= block_range.high_idx; i++) {
        if (reservation->buddy_blocks[i] == NULL) {
            assert(reservation->ref_count[i] == 0);

            reservation->buddy_blocks[i] = buddy_allocator_alloc(reservation->buddy_allocator, bz);
            if (!reservation->buddy_blocks[i]) {
                return -1;
            }

            uint64_t va = reservation->addr + i * bz;
            ret = buddy_map(reservation->buddy_blocks[i], va, bz);
            assert(ret == 0);
        }
        reservation->ref_count[i]++;
    }

    return 0;
}

static uint64_t
allocate_from_reservation(arena_reservation_t *reservation, uint64_t size)
{
    uint64_t addr = 0;
    if (!reservation) {
        return 0;
    }
    block_range_t block_range;
    if (reservation->parent_arena->is_slab) {
        // Slab allocation
        addr = allocate_from_slab((slab_allocator_t *)reservation->strategy, &block_range);
    } else {
        // Object allocation
        addr = allocate_from_obj_allocator((obj_allocator_t *)reservation->strategy, size, &block_range);
    }

    if (!addr) {
        return 0;
    }

    // Try to back with buddy allocator.
    if (back_with_buddy_allocator(reservation, block_range) != 0) {
        if (reservation->parent_arena->is_slab) {
            free_to_slab((slab_allocator_t *)reservation->strategy, addr, NULL);
        } else {
            free_to_obj_allocator((obj_allocator_t *)reservation->strategy, addr, NULL);
        }
        return 0;
    }

    return addr;
}

//
// Arena functions:
//
// allocate_from_arena
//
static uint64_t
allocate_from_arena(arena_t *arena, uint64_t size)
{
    UNUSED(size);
    uint64_t addr = 0;
    arena_reservation_t *reservation = arena->reservation_head;
    while (reservation) {
        addr = allocate_from_reservation(reservation, size);
        if (addr) {
            goto Done;
        }
        reservation = reservation->next;
    }

    reservation = create_reservation(arena);
    if (!reservation) {
        goto Done;
    }

    addr = allocate_from_reservation(reservation, size);
    reservation->next = arena->reservation_head;
    arena->reservation_head = reservation;

Done:
    return addr;
}

//
// Interface functions for the arena allocator
//
static uint64_t
arena_alloc(void *impl, uint64_t size)
{
    va_allocator_arenas_t *arena_impl = (va_allocator_arenas_t *)impl;
    if (!arena_impl) {
        return 0;
    }

    uint64_t arena_idx = get_arena_idx_for_size(size);
    assert(arena_idx < NUM_ARENAS);

    uint64_t addr = allocate_from_arena(&arena_impl->arenas[arena_idx], size);
    if (addr) {
        // Note: This is not quite right because the actual size of the block may be different
        // from the requested size. For example the slab allocator may return a block of size 4KB
        // even if the requested size is 1KB. But this is good enough for a prototype.
        arena_impl->used_va_size += size;
    }
    return addr;
}

static void
arena_free(void *impl, uint64_t addr)
{
    va_allocator_arenas_t *arena_impl = (va_allocator_arenas_t *)impl;
    if (!arena_impl) {
        return;
    }

    CUIaddrTrackerNode *node = cuiAddrTrackerFindNode(&arena_impl->res_tracker, addr);
    if (!node) {
        assert(0);
        return;
    }

    block_range_t block_range;
    arena_reservation_t *reservation = (arena_reservation_t *)node->value;
    if (reservation->parent_arena->is_slab) {
        // Slab allocation
        free_to_slab((slab_allocator_t *)reservation->strategy, addr, &block_range);
    } else {
        // Object allocation
        free_to_obj_allocator((obj_allocator_t *)reservation->strategy, addr, &block_range);
    }

    for (uint64_t i = block_range.low_idx; i <= block_range.high_idx; i++) {
        assert(reservation->ref_count[i] > 0);
        reservation->ref_count[i]--;
    }

    arena_impl->used_va_size -= node->size;
    return;
}

static uint64_t
arena_get_total_size(void *impl)
{
    UNUSED(impl);
    va_allocator_arenas_t *arena_impl = (va_allocator_arenas_t *)impl;
    if (!arena_impl) {
        return 0;
    }

    return arena_impl->total_va_size;
}

static uint64_t
arena_get_used_size(void *impl)
{
    va_allocator_arenas_t *arena_impl = (va_allocator_arenas_t *)impl;
    if (!arena_impl) {
        return 0;
    }

    return arena_impl->used_va_size;
}

static void
arena_destroy(void *impl)
{
    va_allocator_arenas_t *arena_impl = (va_allocator_arenas_t *)impl;
    if (!arena_impl) {
        return;
    }

    for (uint64_t i = 0; i < NUM_ARENAS; i++) {
        arena_t *arena = &arena_impl->arenas[i];
        arena_reservation_t *reservation = arena->reservation_head;
        while (reservation) {
            arena_reservation_t *next = reservation->next;
            destroy_reservation(reservation);
            reservation = next;
        }
        arena->reservation_head = NULL;
    }

    cuiAddrTrackerDeinit(&arena_impl->res_tracker);
    free(arena_impl);
    return;
}

static void
arena_allocator_print(void *impl)
{
    UNUSED(impl);
    //va_allocator_arenas_t *arena_impl = (va_allocator_arenas_t *)impl;
    return;
}

static void
arena_flush(void *impl)
{
    UNUSED(impl);
}

// Function to get the default implementation operations
va_allocator_ops_t *
get_arena_allocator_ops(void)
{
    static va_allocator_ops_t ops = {
        .alloc = arena_alloc,
        .free = arena_free,
        .get_total_size = arena_get_total_size,
        .get_used_size = arena_get_used_size,
        .print = arena_allocator_print,
        .destroy = arena_destroy,
        .flush = arena_flush,
        .impl = NULL
    };
    return &ops;
}

void *
init_arena_allocator(void)
{
    va_allocator_arenas_t *arena_impl = (va_allocator_arenas_t *)calloc(1, sizeof(*arena_impl));
    if (!arena_impl) {
        return NULL;
    }

    arena_impl->total_va_size = 0;
    arena_impl->used_va_size = 0;

    for (uint64_t i = 0; i < NUM_ARENAS; i++) {
        arena_impl->arenas[i].info = arena_info_table[i];
        arena_impl->arenas[i].idx = i;
        arena_impl->arenas[i].is_slab = is_arena_idx_slab(i);
        arena_impl->arenas[i].parent = arena_impl;
        arena_impl->arenas[i].reservation_head = NULL;
    }

    // Max VA width
    cuiAddrTrackerInit(&arena_impl->res_tracker, 0, 1ULL << 57);

    return arena_impl;
}