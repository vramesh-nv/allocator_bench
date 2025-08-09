#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "va_allocator.h"
#include "va_allocator_types.h"
static void test_arena_alloc(uint64_t alloc_size, uint64_t num_allocs)
{
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_ARENA);
    assert(allocator != NULL);
    
    uint64_t *addrs = (uint64_t *)calloc(num_allocs, sizeof(uint64_t));
    assert(addrs != NULL);

    for (uint64_t i = 0; i < num_allocs; i++) {
        addrs[i] = va_alloc(allocator, alloc_size);
        assert(addrs[i] != 0);
    }

    for (uint64_t i = 0; i < num_allocs; i++) {
        va_free(allocator, addrs[num_allocs - i - 1]);
    }

    free(addrs);
    va_allocator_destroy(allocator);
}

static void test_arena_alloc_sizes()
{
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_ARENA);
    assert(allocator != NULL);
    
    uint64_t min_alloc_size = 1ULL << 10;
    uint64_t max_alloc_size = 1ULL << 26;
    uint64_t num_sizes = 0;
    COMPUTE_LOG2(num_sizes, max_alloc_size/min_alloc_size);
    uint64_t num_allocs_per_size = 32;

    uint64_t *addrs = (uint64_t *)calloc(num_sizes * num_allocs_per_size, sizeof(uint64_t));
    assert(addrs != NULL);

    for (uint64_t i = 0; i < num_sizes; i++) {
        uint64_t alloc_size = min_alloc_size << i;
        for (uint64_t j = 0; j < num_allocs_per_size; j++) {
            addrs[i * num_allocs_per_size + j] = va_alloc(allocator, alloc_size);
            assert(addrs[i * num_allocs_per_size + j] != 0);
        }
    }
    
    for (uint64_t i = 0; i < num_sizes * num_allocs_per_size; i++) {
        va_free(allocator, addrs[i]);
    }

    free(addrs);
    va_allocator_destroy(allocator);
}

int main(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

    test_arena_alloc(1024, 1000000);
    test_arena_alloc(1024 * 1024, 1000);
    test_arena_alloc(2 * 1024 * 1024, 100);

    test_arena_alloc_sizes();

    return 0;
}