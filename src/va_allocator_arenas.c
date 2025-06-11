#include "va_allocator.arenas.h"
#include "common.h"
#include "radix.h"

// Arena implementation structure
typedef struct {
    uint64_t total_va_size;     // Total VA space size
    uint64_t used_va_size;      // Currently used VA space
} va_allocator_arenas_t;


// TODO: Fill up the dummy implementation
static uint64_t
arena_alloc(void *impl, uint64_t size)
{
    UNUSED(size);
    UNUSED(impl);
    //va_allocator_arenas_t *arena_impl = (va_allocator_arenas_t *)impl;


    return 0;
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

// Function to get the default implementation operations
va_allocator_ops_t *
get_arena_allocator_ops(void)
{
    static va_allocator_ops_t ops = {
        .alloc = arena_alloc,
        .free = arena_free,
        .get_total_size = arena_get_total_size,
        .get_used_size = arena_get_used_size,
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
    return arena_impl;
}