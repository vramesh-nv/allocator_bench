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

static void test_buddy_allocator_alloc(void)
{
    buddy_allocator_t *allocator = buddy_allocator_create(BASE_BLOCK_SIZE);
    assert(allocator != NULL);

    uint64_t alloc_size =  2ull * 1024ull * 1024ull;

    uint64_t num_allocs = BASE_BLOCK_SIZE / alloc_size;

    buddy_alloc_block_t **blocks = calloc(num_allocs, sizeof(*blocks));
    assert(blocks != NULL);

    uint64_t total_physical_mem_usage = buddy_allocator_get_total_physical_mem_usage(allocator);
    assert(total_physical_mem_usage == 0);

    for (uint64_t i = 0; i < num_allocs; i++) {
        blocks[i] = buddy_allocator_alloc(allocator, alloc_size);
        assert(blocks[i] != NULL);
    }

    total_physical_mem_usage = buddy_allocator_get_total_physical_mem_usage(allocator);
    assert(total_physical_mem_usage == BASE_BLOCK_SIZE);

    for (uint64_t i = 0; i < num_allocs; i++) {
        //buddy_allocator_free(allocator, blocks[i]);
    }
    free(blocks);

    buddy_allocator_destroy(allocator);
}

int main(void) {
    printf("Buddy Allocator Test Suite\n");
    printf("===================================\n\n");
    
    test_buddy_allocator_create();
    test_buddy_allocator_alloc();
    return 0;
} 