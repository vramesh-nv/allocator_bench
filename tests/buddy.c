#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "physical_mem.h"
#include "buddy.h"
#include "common.h"

#define BASE_BLOCK_SIZE (32ull * 1024ull * 1024ull)

static void test_buddy_allocator_create(void)
{
    buddy_allocator_t *allocator = buddy_allocator_create(BASE_BLOCK_SIZE);
    assert(allocator != NULL);

    buddy_allocator_destroy(allocator);
}

static void test_buddy_allocator_alloc_free(void)
{
    buddy_allocator_t *allocator = buddy_allocator_create(BASE_BLOCK_SIZE);
    assert(allocator != NULL);

    uint64_t alloc_size =  2ull * 1024ull * 1024ull;

    // No physical mem usage to start with.
    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == 0);

    buddy_alloc_block_t *alloc_block = buddy_allocator_alloc(allocator, alloc_size);
    assert(alloc_block != NULL);

    // Allocated 1 base block
    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == BASE_BLOCK_SIZE);

    // Freeing physical blocks should not change the total physical mem usage as there are active allocations.
    buddy_free_physical_blocks(allocator);
    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == BASE_BLOCK_SIZE);

    // Free the allocation but usage should not change yet.
    buddy_allocator_free(allocator, alloc_block);
    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == BASE_BLOCK_SIZE);

    // Free the physical blocks and usage should be 0.
    buddy_free_physical_blocks(allocator);
    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == 0);

    buddy_allocator_destroy(allocator);
}

static void test_buddy_allocator_alloc(void)
{
    buddy_allocator_t *allocator = buddy_allocator_create(BASE_BLOCK_SIZE);
    assert(allocator != NULL);

    uint64_t alloc_size =  2ull * 1024ull * 1024ull;

    uint64_t num_allocs = BASE_BLOCK_SIZE / alloc_size;

    buddy_alloc_block_t **blocks = calloc(num_allocs, sizeof(*blocks));
    assert(blocks != NULL);

    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == 0);

    for (uint64_t i = 0; i < num_allocs; i++) {
        blocks[i] = buddy_allocator_alloc(allocator, alloc_size);
        assert(blocks[i] != NULL);
    }

    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == BASE_BLOCK_SIZE);

    for (uint64_t i = 0; i < num_allocs; i++) {
        if (i & 1) {
            buddy_allocator_free(allocator, blocks[i]);
        }
    }

    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == BASE_BLOCK_SIZE);

    buddy_free_physical_blocks(allocator);
    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == BASE_BLOCK_SIZE);

    for (uint64_t i = 0; i < num_allocs; i++) {
        if (!(i & 1)) {
            buddy_allocator_free(allocator, blocks[i]);
        }
    }
    
    buddy_free_physical_blocks(allocator);
    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == 0);

    free(blocks);
    buddy_allocator_destroy(allocator);
}

int main(void) {
    //printf("Buddy Allocator Test Suite\n");
    //printf("===================================\n\n");
    
    test_buddy_allocator_create();
    test_buddy_allocator_alloc_free();
    test_buddy_allocator_alloc();
    return 0;
} 