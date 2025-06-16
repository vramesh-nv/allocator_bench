#include "va_allocator.arenas.h"
#include "common.h"
#include "radix.h"
#include "bitvector.h"
#include "addrtracker.h"

#define NUM_ARENAS 8

// Forward declaration
typedef struct arena arena_t;
typedef struct arena_reservation arena_reservation_t;
//typedef struct va_allocator_arenas va_allocator_arenas_t;

typedef struct slab_allocator {
    uint64_t block_size;        // Size of each block in the slab
    uint64_t blocks_per_slab;   // Number of blocks in this slab
    uint64_t free_blocks;       // Number of free blocks
    CUbitvector *bitmap;
    arena_reservation_t *parent_reservation;
} slab_allocator_t;

typedef struct va_block {
    uint64_t start_addr;     // Starting address of the block
    uint64_t size;          // Size of the block
    int is_free;            // Whether the block is free
    struct va_block *addr_next;  // Next block in address-ordered list
    struct va_block *addr_prev;  // Previous block in address-ordered list
    CUradixNode radix_node;     // Node in size-ordered radix tree
} va_block_t;

typedef struct {
    va_block_t *addr_list;      // List ordered by address
    CUradixTree size_tree;      // Tree ordered by size
    uint64_t total_va_size;     // Total VA space size
    uint64_t used_va_size;      // Currently used VA space
    arena_reservation_t *parent_reservation;
} obj_allocator_t;

typedef struct arena_reservation {
    uint64_t size;
    uint64_t addr;
    CUIaddrTrackerNode node;
    arena_t *parent_arena;
    void *strategy;
    arena_reservation_t *next;
} arena_reservation_t;

typedef struct arena_object {
    uint64_t addr;
    uint64_t size;
    arena_reservation_t *reservation;
    CUIaddrTrackerNode addrtracker_node;
} arena_object_t;

typedef struct arena_info {
    uint64_t max_per_alloc_size;
    uint64_t reservation_size;
} arena_info_t;

typedef struct arena {
    arena_info_t info;
    uint64_t idx;
    int is_slab;
    void *parent;
    arena_reservation_t *reservation_head;
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
    {512UL, 2UL * 1024UL * 1024UL},
    {1024UL, 2UL * 1024UL * 1024UL},
    {2048UL, 4UL * 1024UL * 1024UL},
    {4096UL, 8UL * 1024UL * 1024UL},
    {64UL * 1024UL, 32UL * 1024UL * 1024UL},
    {2UL * 1024UL * 1024UL, 64UL * 1024UL * 1024UL},
    {32UL * 1024UL * 1024UL, 512UL * 1024UL * 1024UL},
    {~0UL, PHYSICAL_MEMORY_SIZE}
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
allocate_from_slab(slab_allocator_t *sa)
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

    return (sa->parent_reservation->node.addr + (sa->block_size * bit));
}

static void
free_to_slab(slab_allocator_t *sa, uint64_t addr)
{
    assert(sa);
    assert(addr >= sa->parent_reservation->addr);
    assert(addr < (sa->parent_reservation->addr + sa->parent_reservation->size));

    uint64_t bit = (addr - sa->parent_reservation->addr) / sa->block_size;
    cubitvectorClearBit(sa->bitmap, bit);
    sa->free_blocks++;
}

// Helper function to insert block into address-ordered list
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

// Helper function to remove block from address-ordered list
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
free_to_obj_allocator(obj_allocator_t *oa, uint64_t addr)
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

    block->is_free = 1;
    //oa->used_va_size -= block->size;
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
allocate_from_obj_allocator(obj_allocator_t *oa, uint64_t size)
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

        // Update the best fit block's size to reflect this split
        best_fit->size = size;
        insert_addr_list(oa, new_block);
        radixTreeInsert(&oa->size_tree, &new_block->radix_node, new_block->size);
    }

    // Mark the best fit block as in use
    best_fit->is_free = 0;
    //oa->used_va_size += best_fit->size;
    radixTreeRemove(&best_fit->radix_node);
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
    oa->used_va_size = 0;
    oa->total_va_size = reservation->size;
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
    oa->addr_list = block;

    radixTreeInsert(&oa->size_tree, &block->radix_node, block->size);

    return oa;
}

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

    void *strategy = (arena->is_slab) ? (void *)initialize_slab(reservation) 
                                      : (void *)initialize_obj_allocator(reservation);
    if (!strategy) {
        FREE_VA(UINT2PTR(addr), arena->info.reservation_size);
        free(reservation);
        return NULL;
    }

    reservation->strategy = strategy;
    va_allocator_arenas_t *arena_impl = (va_allocator_arenas_t *)arena->parent;
    cuiAddrTrackerRegisterNode(&arena_impl->res_tracker, &reservation->node, addr, arena->info.reservation_size, reservation);
    return reservation;
}

static uint64_t
allocate_from_reservation(arena_reservation_t *reservation, uint64_t size)
{
    uint64_t addr = 0;
    if (!reservation) {
        return 0;
    }
    if (reservation->parent_arena->is_slab) {
        // Slab allocation
        addr = allocate_from_slab((slab_allocator_t *)reservation->strategy);
    } else {
        // Object allocation
        addr = allocate_from_obj_allocator((obj_allocator_t *)reservation->strategy, size);
    }

    /*if (addr) {

    }*/
    return addr;
}

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

    return allocate_from_arena(&arena_impl->arenas[arena_idx], size);
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

    arena_reservation_t *reservation = (arena_reservation_t *)node->value;
    if (reservation->parent_arena->is_slab) {
        // Slab allocation
        free_to_slab((slab_allocator_t *)reservation->strategy, addr);
    } else {
        // Object allocation
        free_to_obj_allocator((obj_allocator_t *)reservation->strategy, addr);
    }
    return;
}

static uint64_t
arena_get_total_size(void *impl)
{
    UNUSED(impl);
    //va_allocator_arenas_t *arena_impl = (va_allocator_arenas_t *)impl;

    return 0;
}

static uint64_t
arena_get_used_size(void *impl)
{
    UNUSED(impl);
    //va_allocator_arenas_t *arena_impl = (va_allocator_arenas_t *)impl;

    return 0;
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