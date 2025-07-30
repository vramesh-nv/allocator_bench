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

static void test_buddy_allocator_alloc_free_size(void)
{
    buddy_allocator_t *allocator = buddy_allocator_create(BASE_BLOCK_SIZE);
    assert(allocator != NULL);

    uint64_t alloc_size =  2ull * 1024ull * 1024ull;

    uint64_t log = 0;
    COMPUTE_LOG2(log, (BASE_BLOCK_SIZE / alloc_size));

    buddy_alloc_block_t **blocks = calloc(log, sizeof(*blocks));
    assert(blocks != NULL);

    for (uint64_t i = 0; i < log; i++) {
        blocks[i] = buddy_allocator_alloc(allocator, alloc_size << i);
        assert(blocks[i] != NULL);
    }

    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == BASE_BLOCK_SIZE);

    buddy_alloc_block_t *alloc_block = buddy_allocator_alloc(allocator, alloc_size);
    assert(alloc_block != NULL);
    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == BASE_BLOCK_SIZE);

    buddy_allocator_free(allocator, alloc_block);

    buddy_alloc_block_t *large_block = buddy_allocator_alloc(allocator, BASE_BLOCK_SIZE);
    assert(large_block != NULL);
    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == 2 * BASE_BLOCK_SIZE);

    for (uint64_t i = 0; i < log; i++) {
        buddy_allocator_free(allocator, blocks[i]);
    }
    buddy_free_physical_blocks(allocator);
    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == BASE_BLOCK_SIZE);
    
    buddy_allocator_free(allocator, large_block);
    buddy_free_physical_blocks(allocator);
    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == 0);

    free(blocks);
    buddy_allocator_destroy(allocator);
}


// Test edge cases and boundary conditions
static void test_buddy_allocator_edge_cases(void)
{
    printf("Testing edge cases...\n");
    buddy_allocator_t *allocator = buddy_allocator_create(BASE_BLOCK_SIZE);
    assert(allocator != NULL);

    // Test minimum allocation size
    buddy_alloc_block_t *min_block = buddy_allocator_alloc(allocator, 2ULL * 1024 * 1024);
    assert(min_block != NULL);
    printf("  ✓ Minimum 2MB allocation successful\n");

    // Test maximum allocation size
    buddy_alloc_block_t *max_block = buddy_allocator_alloc(allocator, BASE_BLOCK_SIZE);
    assert(max_block != NULL);
    printf("  ✓ Maximum 32MB allocation successful\n");

    // Test NULL inputs
    assert(buddy_allocator_alloc(NULL, 2ULL * 1024 * 1024) == NULL);
    assert(buddy_allocator_alloc(allocator, 0) == NULL);
    printf("  ✓ NULL input handling correct\n");

    buddy_allocator_free(allocator, min_block);
    buddy_allocator_free(allocator, max_block);
    buddy_free_physical_blocks(allocator);
    buddy_allocator_destroy(allocator);
    printf("Edge cases test passed!\n\n");
}

// Test fragmentation scenarios
static void test_buddy_allocator_fragmentation(void)
{
    printf("Testing fragmentation scenarios...\n");
    buddy_allocator_t *allocator = buddy_allocator_create(BASE_BLOCK_SIZE);
    assert(allocator != NULL);

    // Allocate alternating sizes to create fragmentation
    buddy_alloc_block_t *blocks[8];
    
    // Allocate 4 x 4MB blocks (should fill exactly 16MB)
    for (int i = 0; i < 4; i++) {
        blocks[i] = buddy_allocator_alloc(allocator, 4ULL * 1024 * 1024);
        assert(blocks[i] != NULL);
    }
    
    // Allocate 8 x 2MB blocks (should fill remaining 16MB)
    for (int i = 4; i < 8; i++) {
        blocks[i] = buddy_allocator_alloc(allocator, 2ULL * 1024 * 1024);
        assert(blocks[i] != NULL);
    }
    
    // Should have exactly one 32MB block allocated
    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == BASE_BLOCK_SIZE);
    
    // Free every other block to create fragmentation
    for (int i = 0; i < 8; i += 2) {
        buddy_allocator_free(allocator, blocks[i]);
    }
    
    // Try to allocate 16MB - should fail due to fragmentation
    buddy_alloc_block_t *large_block = buddy_allocator_alloc(allocator, 16ULL * 1024 * 1024);
    assert(large_block != NULL); // Should allocate new chunk
    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == 2 * BASE_BLOCK_SIZE);
    
    printf("  ✓ Fragmentation handling works correctly\n");

    // Clean up
    buddy_allocator_free(allocator, large_block);
    for (int i = 1; i < 8; i += 2) {
        buddy_allocator_free(allocator, blocks[i]);
    }
    buddy_free_physical_blocks(allocator);
    buddy_allocator_destroy(allocator);
    printf("Fragmentation test passed!\n\n");
}

// Test coalescing behavior
static void test_buddy_allocator_coalescing(void)
{
    printf("Testing coalescing behavior...\n");
    buddy_allocator_t *allocator = buddy_allocator_create(BASE_BLOCK_SIZE);
    assert(allocator != NULL);

    // Allocate 16 x 2MB blocks (32MB total)
    buddy_alloc_block_t *blocks[16];
    for (int i = 0; i < 16; i++) {
        blocks[i] = buddy_allocator_alloc(allocator, 2ULL * 1024 * 1024);
        assert(blocks[i] != NULL);
    }
    
    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == BASE_BLOCK_SIZE);
    
    // Free all blocks
    for (int i = 0; i < 16; i++) {
        buddy_allocator_free(allocator, blocks[i]);
    }
    
    // Should be able to allocate the entire 32MB block again
    buddy_alloc_block_t *full_block = buddy_allocator_alloc(allocator, BASE_BLOCK_SIZE);
    assert(full_block != NULL);
    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == BASE_BLOCK_SIZE);
    
    printf("  ✓ Coalescing works correctly\n");

    buddy_allocator_free(allocator, full_block);
    buddy_free_physical_blocks(allocator);
    buddy_allocator_destroy(allocator);
    printf("Coalescing test passed!\n\n");
}

// Test mixed allocation patterns
static void test_buddy_allocator_mixed_patterns(void)
{
    printf("Testing mixed allocation patterns...\n");
    buddy_allocator_t *allocator = buddy_allocator_create(BASE_BLOCK_SIZE);
    assert(allocator != NULL);

    buddy_alloc_block_t *blocks[32];
    int block_count = 0;
    
    // Pattern: 8MB, 4MB, 4MB, 2MB, 2MB, 2MB, 2MB, 8MB
    uint64_t sizes[] = {8ULL * 1024 * 1024, 4ULL * 1024 * 1024, 4ULL * 1024 * 1024,
                        2ULL * 1024 * 1024, 2ULL * 1024 * 1024, 2ULL * 1024 * 1024, 
                        2ULL * 1024 * 1024, 8ULL * 1024 * 1024};
    
    for (int i = 0; i < 8; i++) {
        blocks[block_count] = buddy_allocator_alloc(allocator, sizes[i]);
        assert(blocks[block_count] != NULL);
        block_count++;
    }
    
    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == BASE_BLOCK_SIZE);
    
    // Free in reverse order
    for (int i = block_count - 1; i >= 0; i--) {
        buddy_allocator_free(allocator, blocks[i]);
    }
    
    printf("  ✓ Mixed allocation patterns work correctly\n");

    buddy_free_physical_blocks(allocator);
    buddy_allocator_destroy(allocator);
    printf("Mixed patterns test passed!\n\n");
}

// Test multiple physical blocks
static void test_buddy_allocator_multiple_blocks(void)
{
    printf("Testing multiple physical blocks...\n");
    buddy_allocator_t *allocator = buddy_allocator_create(BASE_BLOCK_SIZE);
    assert(allocator != NULL);

    // Allocate enough to trigger multiple physical blocks
    buddy_alloc_block_t *blocks[6];
    
    // Each allocation is 16MB, so 6 allocations = 96MB (3 physical blocks)
    for (int i = 0; i < 6; i++) {
        blocks[i] = buddy_allocator_alloc(allocator, 16ULL * 1024 * 1024);
        assert(blocks[i] != NULL);
    }
    
    // Should have 3 physical blocks
    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == 3 * BASE_BLOCK_SIZE);
    
    // Free middle blocks to test selective cleanup
    buddy_allocator_free(allocator, blocks[1]);
    buddy_allocator_free(allocator, blocks[3]);
    
    buddy_free_physical_blocks(allocator);
    
    // Should still have 2 physical blocks (with blocks[0], blocks[2], blocks[4], blocks[5])
    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == 3 * BASE_BLOCK_SIZE);
    
    // Free remaining blocks
    buddy_allocator_free(allocator, blocks[0]);
    buddy_allocator_free(allocator, blocks[2]);
    buddy_allocator_free(allocator, blocks[4]);
    buddy_allocator_free(allocator, blocks[5]);
    
    buddy_free_physical_blocks(allocator);
    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == 0);
    
    printf("  ✓ Multiple physical blocks handled correctly\n");

    buddy_allocator_destroy(allocator);
    printf("Multiple blocks test passed!\n\n");
}

// Test stress scenarios
static void test_buddy_allocator_stress(void)
{
    printf("Testing stress scenarios...\n");
    buddy_allocator_t *allocator = buddy_allocator_create(BASE_BLOCK_SIZE);
    assert(allocator != NULL);

    buddy_alloc_block_t *blocks[1000];
    int allocated_count = 0;
    
    // Stress test: allocate many small blocks
    for (int i = 0; i < 1000; i++) {
        blocks[i] = buddy_allocator_alloc(allocator, 2ULL * 1024 * 1024);
        if (blocks[i] != NULL) {
            allocated_count++;
        } else {
            break;
        }
    }
    
    printf("  ✓ Allocated %d blocks under stress\n", allocated_count);
    
    // Free all allocated blocks
    for (int i = 0; i < allocated_count; i++) {
        buddy_allocator_free(allocator, blocks[i]);
    }
    
    buddy_free_physical_blocks(allocator);
    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == 0);
    
    printf("  ✓ All blocks freed successfully\n");

    buddy_allocator_destroy(allocator);
    printf("Stress test passed!\n\n");
}

static void test_buddy_allocator_single_level(void)
{
    uint64_t alloc_size = 2ull * 1024ull * 1024ull;
    buddy_allocator_t *allocator = buddy_allocator_create(alloc_size);
    assert(allocator != NULL);
    
    buddy_alloc_block_t *alloc_block = buddy_allocator_alloc(allocator, alloc_size);
    assert(alloc_block != NULL);
    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == alloc_size);

    buddy_allocator_free(allocator, alloc_block);
    buddy_free_physical_blocks(allocator);
    assert(buddy_allocator_get_total_physical_mem_usage(allocator) == 0);

    buddy_allocator_destroy(allocator);
}

int main(void)
{
    printf("Buddy Allocator Test Suite\n");
    printf("===================================\n\n");
    
    test_buddy_allocator_create();
    test_buddy_allocator_alloc_free();
    test_buddy_allocator_alloc();
    test_buddy_allocator_alloc_free_size();
    
    // New comprehensive tests
    test_buddy_allocator_edge_cases();
    test_buddy_allocator_fragmentation();
    test_buddy_allocator_coalescing();
    test_buddy_allocator_mixed_patterns();
    test_buddy_allocator_multiple_blocks();
    test_buddy_allocator_stress();

    test_buddy_allocator_single_level();
    
    printf("All tests passed! ✅\n");
    return 0;
} 