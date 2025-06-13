#include "va_allocator.arenas.h"
#include "common.h"
#include "radix.h"
#include "bitvector.h"

#define NUM_ARENAS 8

// Forward declaration
typedef struct arena arena_t;
typedef struct arena_reservation arena_reservation_t;

typedef struct arena_info {
    uint64_t max_per_alloc_size;
    uint64_t reservation_size;
} arena_info_t;

typedef struct slab_info {
    uint64_t block_size;        // Size of each block in the slab
    uint64_t blocks_per_slab;   // Number of blocks in this slab
    uint64_t free_blocks;       // Number of free blocks
    CUbitvector *bitmap;
    arena_reservation_t *parent_reservation;
} slab_info_t;

typedef struct arena_reservation {
    uint64_t size;
    uint64_t addr;
    arena_t *parent_arena;
    void *strategy;
    arena_reservation_t *next;
} arena_reservation_t;

typedef struct arena_object {
    uint64_t addr;
    uint64_t size;
    arena_reservation_t *reservation;
} arena_object_t;

typedef struct arena {
    arena_info_t info;
    uint64_t idx;
    int is_slab;
    arena_reservation_t *reservation_head;
} arena_t;

//
// Arena implementation structure
//
typedef struct {
    arena_t arenas[NUM_ARENAS];
    uint64_t total_va_size;     // Total VA space size
    uint64_t used_va_size;      // Currently used VA space
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



#if 0
static void
destroy_reservation(arena_reservation_t *reservation)
{
    if (!reservation) {
        return;
    }
    if (reservation->parent_arena->is_slab) {
        // Slab allocation
    } else {
        // Arena allocation
    }
}
#endif

static void *
initialize_slab(arena_reservation_t *reservation)
{
    assert(reservation && (reservation->strategy == NULL));

    slab_info_t *slab_info = (slab_info_t *)calloc(1, sizeof(*slab_info));
    if (!slab_info) {
        return NULL;
    }

    slab_info->block_size = reservation->parent_arena->info.max_per_alloc_size;
    slab_info->blocks_per_slab = reservation->parent_arena->info.reservation_size / slab_info->block_size;
    slab_info->free_blocks = slab_info->blocks_per_slab;
    slab_info->parent_reservation = reservation;

    CUbitvector *bitmap = NULL;
    cubitvectorCreate(&bitmap, slab_info->blocks_per_slab);
    if (!bitmap) {
        free(slab_info);
        return NULL;
    }

    slab_info->bitmap = bitmap;
    return slab_info;
}

static uint64_t
allocate_from_slab(slab_info_t *slab_info)
{
    assert(slab_info);
    if (slab_info->free_blocks == 0) {
        return 0;
    }

    NvU64 bit = 0;
    if (!cubitvectorFindLowestClearBitInRange(slab_info->bitmap, 0, slab_info->blocks_per_slab - 1, &bit)) {
        return 0;
    }
    cubitvectorSetBit(slab_info->bitmap, bit);
    slab_info->free_blocks--;

    return (slab_info->parent_reservation->addr + (slab_info->block_size * bit));
}

#if 0
static void
free_to_slab(slab_info_t *slab_info, uint64_t addr)
{
    assert(slab_info);
    assert(addr >= slab_info->parent_reservation->addr);
    assert(addr < (slab_info->parent_reservation->addr + slab_info->parent_reservation->size));

    uint64_t bit = (addr - slab_info->parent_reservation->addr) / slab_info->block_size;
    cubitvectorClearBit(slab_info->bitmap, bit);
    slab_info->free_blocks++;
}
#endif

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

    void *strategy = (arena->is_slab) ? (void *)initialize_slab(reservation) : NULL;
    if (!strategy) {
        FREE_VA(UINT2PTR(addr), arena->info.reservation_size);
        free(reservation);
        return NULL;
    }

    reservation->strategy = strategy;
    return reservation;
}

static uint64_t
allocate_from_reservation(arena_reservation_t *reservation)
{
    if (!reservation) {
        return 0;
    }
    if (reservation->parent_arena->is_slab) {
        // Slab allocation
        return allocate_from_slab((slab_info_t *)reservation->strategy);
    } else {
        // Arena allocation
        assert(0);
        return 0;
    }
}

static uint64_t
allocate_from_arena(arena_t *arena)
{
    uint64_t addr = 0;
    arena_reservation_t *reservation = arena->reservation_head;
    while (reservation) {
        addr = allocate_from_reservation(reservation);
        if (addr) {
            return addr;
        }
        reservation = reservation->next;
    }

    reservation = create_reservation(arena);
    if (!reservation) {
        return 0;
    }
    
    addr = allocate_from_reservation(reservation);
    reservation->next = arena->reservation_head;
    arena->reservation_head = reservation;

    return addr;
}

static uint64_t
arena_alloc(void *impl, uint64_t size)
{
    va_allocator_arenas_t *arena_impl = (va_allocator_arenas_t *)impl;
    if (!arena_impl) {
        return 0;
    }

    uint64_t arena_idx = get_arena_idx_for_size(size);
    assert(arena_idx < NUM_ARENAS);

    return allocate_from_arena(&arena_impl->arenas[arena_idx]);
}

static void
arena_free(void *impl, uint64_t addr)
{
    UNUSED(addr);
    UNUSED(impl);
    //va_allocator_arenas_t *arena_impl = (va_allocator_arenas_t *)impl;

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
        arena_impl->arenas[i].reservation_head = NULL;
    }

    return arena_impl;
}