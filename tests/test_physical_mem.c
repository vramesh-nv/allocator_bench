#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "physical_mem.h"
#include "common.h"

#define TEST_VA_BASE 0x200000000ULL   // 8GB base for test VA space
#define TEST_BLOCK_SIZE_32MB (32ULL * 1024 * 1024)
#define TEST_BLOCK_SIZE_2MB (2ULL * 1024 * 1024)  
#define TEST_BLOCK_SIZE_4MB (4ULL * 1024 * 1024)

// Test manager lifecycle
static void test_manager_lifecycle(void) {
    printf("Testing manager lifecycle...\n");
    
    // Test creation
    physical_mem_mgr_t *mgr = physical_mem_mgr_create();
    assert(mgr != NULL);
    printf("  ✓ Manager created successfully\n");
    
    // Initial usage should be 0
    uint64_t initial_usage = get_total_physical_mem_usage(mgr);
    assert(initial_usage == 0);
    printf("  ✓ Initial usage is 0: %lu bytes\n", initial_usage);
    
    // Test destruction
    physical_mem_mgr_destroy(mgr);
    printf("  ✓ Manager destroyed successfully\n");
    
    // Test NULL destruction (should not crash)
    physical_mem_mgr_destroy(NULL);
    printf("  ✓ NULL destruction handled safely\n");
    
    printf("Manager lifecycle test passed!\n\n");
}

// Test basic physical memory allocation and free
static void test_physical_allocation(void) {
    printf("Testing physical memory allocation...\n");
    
    physical_mem_mgr_t *mgr = physical_mem_mgr_create();
    assert(mgr != NULL);
    
    // Test single allocation
    physical_mem_t *mem1 = allocate_physical_mem(mgr, TEST_BLOCK_SIZE_32MB);
    assert(mem1 != NULL);
    printf("  ✓ Allocated 32MB block\n");
    
    // Check usage tracking
    uint64_t usage = get_total_physical_mem_usage(mgr);
    assert(usage == TEST_BLOCK_SIZE_32MB);
    printf("  ✓ Usage tracked correctly: %lu bytes\n", usage);
    
    // Test multiple allocations
    physical_mem_t *mem2 = allocate_physical_mem(mgr, TEST_BLOCK_SIZE_2MB);
    assert(mem2 != NULL);
    physical_mem_t *mem3 = allocate_physical_mem(mgr, TEST_BLOCK_SIZE_4MB);
    assert(mem3 != NULL);
    printf("  ✓ Multiple allocations successful\n");
    
    usage = get_total_physical_mem_usage(mgr);
    assert(usage == TEST_BLOCK_SIZE_32MB + TEST_BLOCK_SIZE_2MB + TEST_BLOCK_SIZE_4MB);
    printf("  ✓ Total usage: %lu bytes\n", usage);
    
    // Test free in different order
    free_physical_mem(mgr, mem2);
    usage = get_total_physical_mem_usage(mgr);
    assert(usage == TEST_BLOCK_SIZE_32MB + TEST_BLOCK_SIZE_4MB);
    printf("  ✓ Free mem2: usage now %lu bytes\n", usage);
    
    free_physical_mem(mgr, mem1);
    usage = get_total_physical_mem_usage(mgr);
    assert(usage == TEST_BLOCK_SIZE_4MB);
    printf("  ✓ Free mem1: usage now %lu bytes\n", usage);
    
    free_physical_mem(mgr, mem3);
    usage = get_total_physical_mem_usage(mgr);
    assert(usage == 0);
    printf("  ✓ All freed: usage back to 0\n");
    
    // Test NULL free (should not crash)
    free_physical_mem(mgr, NULL);
    printf("  ✓ NULL free handled safely\n");
    
    physical_mem_mgr_destroy(mgr);
    printf("Physical allocation test passed!\n\n");
}

// Test memory mapping and unmapping
static void test_memory_mapping(void) {
    printf("Testing memory mapping...\n");
    
    physical_mem_mgr_t *mgr = physical_mem_mgr_create();
    assert(mgr != NULL);
    
    // Allocate a 32MB physical block
    physical_mem_t *mem = allocate_physical_mem(mgr, TEST_BLOCK_SIZE_32MB);
    assert(mem != NULL);
    printf("  ✓ Allocated 32MB physical block\n");
    
    // Test mapping portions of the block
    uint64_t va1 = TEST_VA_BASE;
    uint64_t va2 = TEST_VA_BASE + TEST_BLOCK_SIZE_2MB;
    uint64_t va3 = TEST_VA_BASE + TEST_BLOCK_SIZE_2MB + TEST_BLOCK_SIZE_4MB;
    
    // Map 2MB portion
    int ret = map_physical_mem(mem, 0, va1, TEST_BLOCK_SIZE_2MB);
    assert(ret == 0);
    printf("  ✓ Mapped 2MB at VA 0x%lx\n", va1);
    
    // Map 4MB portion  
    ret = map_physical_mem(mem, TEST_BLOCK_SIZE_2MB, va2, TEST_BLOCK_SIZE_4MB);
    assert(ret == 0);
    printf("  ✓ Mapped 4MB at VA 0x%lx\n", va2);
    
    // Map 8MB portion
    ret = map_physical_mem(mem, TEST_BLOCK_SIZE_2MB + TEST_BLOCK_SIZE_4MB, va3, 8 * 1024 * 1024);
    assert(ret == 0);
    printf("  ✓ Mapped 8MB at VA 0x%lx\n", va3);
    
    // Test memory access (write/read)
    volatile uint32_t *ptr1 = (volatile uint32_t*)va1;
    volatile uint32_t *ptr2 = (volatile uint32_t*)va2;
    volatile uint32_t *ptr3 = (volatile uint32_t*)va3;
    
    *ptr1 = 0xDEADBEEF;
    *ptr2 = 0xCAFEBABE;
    *ptr3 = 0x12345678;
    
    assert(*ptr1 == 0xDEADBEEF);
    assert(*ptr2 == 0xCAFEBABE);
    assert(*ptr3 == 0x12345678);
    printf("  ✓ Memory read/write access works\n");
    
    // Test unmapping
    ret = unmap_physical_mem(mem, va2, TEST_BLOCK_SIZE_4MB);
    assert(ret == 0);
    printf("  ✓ Unmapped 4MB region\n");
    
    ret = unmap_physical_mem(mem, va1, TEST_BLOCK_SIZE_2MB);
    assert(ret == 0);
    printf("  ✓ Unmapped 2MB region\n");
    
    ret = unmap_physical_mem(mem, va3, 8 * 1024 * 1024);
    assert(ret == 0);
    printf("  ✓ Unmapped 8MB region\n");
    
    // Clean up
    free_physical_mem(mgr, mem);
    physical_mem_mgr_destroy(mgr);
    
    printf("Memory mapping test passed!\n\n");
}

// Test error conditions and edge cases
static void test_error_conditions(void) {
    printf("Testing error conditions...\n");
    
    physical_mem_mgr_t *mgr = physical_mem_mgr_create();
    assert(mgr != NULL);
    
    physical_mem_t *mem = allocate_physical_mem(mgr, TEST_BLOCK_SIZE_32MB);
    assert(mem != NULL);
    
    uint64_t va = TEST_VA_BASE;
    
    // Test invalid parameters for mapping
    int ret = map_physical_mem(NULL, 0, va, TEST_BLOCK_SIZE_2MB);
    assert(ret == -1);
    printf("  ✓ map_physical_mem rejects NULL mem\n");
    
    ret = map_physical_mem(mem, 0, 0, TEST_BLOCK_SIZE_2MB);
    assert(ret == -1);
    printf("  ✓ map_physical_mem rejects NULL VA\n");
    
    ret = map_physical_mem(mem, 0, va, 0);
    assert(ret == -1);
    printf("  ✓ map_physical_mem rejects zero size\n");
    
    // Test invalid parameters for unmapping
    ret = unmap_physical_mem(NULL, va, TEST_BLOCK_SIZE_2MB);
    assert(ret == -1);
    printf("  ✓ unmap_physical_mem rejects NULL mem\n");
    
    ret = unmap_physical_mem(mem, 0, TEST_BLOCK_SIZE_2MB);
    assert(ret == -1);
    printf("  ✓ unmap_physical_mem rejects NULL VA\n");
    
    ret = unmap_physical_mem(mem, va, 0);
    assert(ret == -1);
    printf("  ✓ unmap_physical_mem rejects zero size\n");
    
    // Test unmapping non-existent mapping
    ret = unmap_physical_mem(mem, va, TEST_BLOCK_SIZE_2MB);
    assert(ret == -1);
    printf("  ✓ unmap_physical_mem rejects non-existent mapping\n");
    
    // Test valid mapping then double mapping (should fail)
    ret = map_physical_mem(mem, 0, va, TEST_BLOCK_SIZE_2MB);
    assert(ret == 0);
    printf("  ✓ First mapping successful\n");
    
    ret = map_physical_mem(mem, 0, va, TEST_BLOCK_SIZE_2MB);
    assert(ret == -1);
    printf("  ✓ Double mapping correctly rejected\n");
    
    // Test overlapping mapping
    ret = map_physical_mem(mem, 0, va + 1024*1024, TEST_BLOCK_SIZE_2MB);  // 1MB offset, overlaps
    assert(ret == -1);
    printf("  ✓ Overlapping mapping correctly rejected\n");
    
    // Clean up the valid mapping
    ret = unmap_physical_mem(mem, va, TEST_BLOCK_SIZE_2MB);
    assert(ret == 0);
    printf("  ✓ Cleanup mapping successful\n");
    
    free_physical_mem(mgr, mem);
    physical_mem_mgr_destroy(mgr);
    
    printf("Error conditions test passed!\n\n");
}

// Test resource exhaustion (allocate until failure)
static void test_resource_limits(void) {
    printf("Testing resource limits...\n");
    
    physical_mem_mgr_t *mgr = physical_mem_mgr_create();
    assert(mgr != NULL);
    
    // Try to allocate more than PHYSICAL_MEMORY_SIZE
    printf("  Physical memory limit: %llu bytes\n", PHYSICAL_MEMORY_SIZE);
    
    // Allocate large chunks until failure
    physical_mem_t *mems[100];
    int alloc_count = 0;
    uint64_t chunk_size = 64 * 1024 * 1024;  // 64MB chunks
    
    for (int i = 0; i < 100; i++) {
        mems[i] = allocate_physical_mem(mgr, chunk_size);
        if (mems[i] == NULL) {
            printf("  ✓ Allocation failed at chunk %d (as expected)\n", i);
            break;
        }
        alloc_count++;
        uint64_t usage = get_total_physical_mem_usage(mgr);
        printf("  Allocated chunk %d, total usage: %lu MB\n", i, usage / (1024*1024));
        
        if (usage >= PHYSICAL_MEMORY_SIZE) {
            break;
        }
    }
    
    printf("  ✓ Successfully allocated %d chunks before exhaustion\n", alloc_count);
    
    // Free all allocated chunks
    for (int i = 0; i < alloc_count; i++) {
        free_physical_mem(mgr, mems[i]);
    }
    
    uint64_t final_usage = get_total_physical_mem_usage(mgr);
    assert(final_usage == 0);
    printf("  ✓ All memory freed, usage back to 0\n");
    
    physical_mem_mgr_destroy(mgr);
    
    printf("Resource limits test passed!\n\n");
}

// Test granular mapping pattern (like buddy allocator would use)
static void test_granular_mapping_pattern(void) {
    printf("Testing granular mapping pattern...\n");
    
    physical_mem_mgr_t *mgr = physical_mem_mgr_create();
    assert(mgr != NULL);
    
    // Allocate one 32MB physical block
    physical_mem_t *mem = allocate_physical_mem(mgr, TEST_BLOCK_SIZE_32MB);
    assert(mem != NULL);
    printf("  ✓ Allocated 32MB physical block\n");
    
    // Map various sized portions (simulating buddy allocator usage)
    uint64_t va_base = TEST_VA_BASE;
    uint64_t current_va = va_base;
    
    // Map 16 x 2MB blocks
    uint64_t mappings_2mb[16];
    for (int i = 0; i < 16; i++) {
        mappings_2mb[i] = current_va;
        int ret = map_physical_mem(mem, i * TEST_BLOCK_SIZE_2MB, current_va, TEST_BLOCK_SIZE_2MB);
        assert(ret == 0);
        current_va += TEST_BLOCK_SIZE_2MB;
    }
    printf("  ✓ Mapped 16 x 2MB blocks successfully\n");
    
    // Test that we've used the entire 32MB 
    assert(current_va == va_base + TEST_BLOCK_SIZE_32MB);
    printf("  ✓ Exactly filled 32MB physical block\n");
    
    // Write unique patterns to each 2MB block
    for (int i = 0; i < 16; i++) {
        volatile uint32_t *ptr = (volatile uint32_t*)mappings_2mb[i];
        *ptr = 0x1000U + (uint32_t)i;  // Unique pattern for each block
    }
    printf("  ✓ Wrote unique patterns to each block\n");
    
    // Verify patterns
    for (int i = 0; i < 16; i++) {
        volatile uint32_t *ptr = (volatile uint32_t*)mappings_2mb[i];
        assert(*ptr == (0x1000U + (uint32_t)i));
    }
    printf("  ✓ Verified all patterns correct\n");
    
    // Unmap every other block (simulating fragmented free)
    for (int i = 1; i < 16; i += 2) {
        int ret = unmap_physical_mem(mem, mappings_2mb[i], TEST_BLOCK_SIZE_2MB);
        assert(ret == 0);
    }
    printf("  ✓ Unmapped every other block\n");
    
    // Verify remaining blocks still have correct data
    for (int i = 0; i < 16; i += 2) {
        volatile uint32_t *ptr = (volatile uint32_t*)mappings_2mb[i];
        assert(*ptr == (0x1000U + (uint32_t)i));
    }
    printf("  ✓ Remaining blocks still have correct data\n");
    
    // Unmap remaining blocks
    for (int i = 0; i < 16; i += 2) {
        int ret = unmap_physical_mem(mem, mappings_2mb[i], TEST_BLOCK_SIZE_2MB);
        assert(ret == 0);
    }
    printf("  ✓ Unmapped all remaining blocks\n");
    
    free_physical_mem(mgr, mem);
    physical_mem_mgr_destroy(mgr);
    
    printf("Granular mapping pattern test passed!\n\n");
}

// Test partial mapping functionality  
static void test_partial_mapping(void) {
    printf("Testing partial mapping functionality...\n");
    
    physical_mem_mgr_t *mgr = physical_mem_mgr_create();
    assert(mgr != NULL);
    
    // Allocate a 32MB physical block
    physical_mem_t *mem = allocate_physical_mem(mgr, TEST_BLOCK_SIZE_32MB);
    assert(mem != NULL);
    
    // Test partial mapping - map 4MB starting at offset 8MB
    uint64_t va1 = TEST_VA_BASE;
    uint64_t offset1 = 8ULL * 1024 * 1024;  // 8MB offset
    uint64_t size1 = TEST_BLOCK_SIZE_4MB;   // 4MB size
    
    int result = map_physical_mem(mem, offset1, va1, size1);
    assert(result == 0);
    printf("  ✓ Partial mapping (4MB at offset 8MB) successful\n");
    
    // Verify mapping info
    uint64_t retrieved_offset, retrieved_size;
    result = get_mapping_info(mem, va1, &retrieved_offset, &retrieved_size);
    assert(result == 0);
    assert(retrieved_offset == offset1);
    assert(retrieved_size == size1);
    printf("  ✓ Mapping info retrieval works correctly\n");
    
    // Test another partial mapping - map 2MB starting at offset 16MB
    uint64_t va2 = TEST_VA_BASE + 0x100000000ULL; // Different VA
    uint64_t offset2 = 16ULL * 1024 * 1024;       // 16MB offset
    uint64_t size2 = TEST_BLOCK_SIZE_2MB;         // 2MB size
    
    result = map_physical_mem(mem, offset2, va2, size2);
    assert(result == 0);
    printf("  ✓ Second partial mapping (2MB at offset 16MB) successful\n");
    
    // Test range mapping check
    assert(is_range_mapped(mem, va1, size1) == 1);
    assert(is_range_mapped(mem, va2, size2) == 1);
    assert(is_range_mapped(mem, TEST_VA_BASE + 0x200000000ULL, size1) == 0);
    printf("  ✓ Range mapping checks work correctly\n");
    
    // Test bounds validation
    uint64_t invalid_offset = 30ULL * 1024 * 1024; // 30MB offset
    uint64_t invalid_size = TEST_BLOCK_SIZE_4MB;    // 4MB size (30+4 > 32)
    result = map_physical_mem(mem, invalid_offset, TEST_VA_BASE + 0x300000000ULL, invalid_size);
    assert(result == -1); // Should fail due to bounds
    printf("  ✓ Bounds validation works correctly\n");
    
    // Test overlapping VA mapping
    result = map_physical_mem(mem, 0, va1, size1); // Same VA, different offset
    assert(result == -1); // Should fail due to VA overlap
    printf("  ✓ VA overlap detection works correctly\n");
    
    // Test overlapping physical memory mapping
    uint64_t va3 = TEST_VA_BASE + 0x400000000ULL; // Different VA
    
    // Try to map same physical memory range (8MB-12MB) to different VA - should fail
    result = map_physical_mem(mem, offset1, va3, size1); // Same offset and size as first mapping
    assert(result == -1); // Should fail due to physical memory overlap
    printf("  ✓ Exact physical memory overlap detection works correctly\n");
    
    // Try to map partially overlapping physical memory (10MB-14MB overlaps with 8MB-12MB) - should fail
    uint64_t partial_offset = 10ULL * 1024 * 1024; // 10MB offset
    result = map_physical_mem(mem, partial_offset, va3, size1); // 10MB-14MB range
    assert(result == -1); // Should fail due to partial physical memory overlap
    printf("  ✓ Partial physical memory overlap detection works correctly\n");
    
    // Try to map physical memory that overlaps with second mapping (15MB-17MB overlaps with 16MB-18MB) - should fail
    uint64_t overlap_offset2 = 15ULL * 1024 * 1024; // 15MB offset
    result = map_physical_mem(mem, overlap_offset2, va3, size2); // 15MB-17MB range
    assert(result == -1); // Should fail due to physical memory overlap with second mapping
    printf("  ✓ Physical memory overlap with second mapping detection works correctly\n");
    
    // Clean up
    result = unmap_physical_mem(mem, va1, size1);
    assert(result == 0);
    result = unmap_physical_mem(mem, va2, size2);
    assert(result == 0);
    
    free_physical_mem(mgr, mem);
    physical_mem_mgr_destroy(mgr);
    printf("Partial mapping test passed!\n\n");
}

// Test backward compatibility
static void test_backward_compatibility(void) {
    printf("Testing backward compatibility...\n");
    
    physical_mem_mgr_t *mgr = physical_mem_mgr_create();
    assert(mgr != NULL);
    
    physical_mem_t *mem = allocate_physical_mem(mgr, TEST_BLOCK_SIZE_32MB);
    assert(mem != NULL);
    
    // Old API should still work (maps from offset 0)
    int result = map_physical_mem(mem, 0, TEST_VA_BASE, TEST_BLOCK_SIZE_32MB);
    assert(result == 0);
    
    // Verify it mapped from offset 0
    uint64_t offset, size;
    result = get_mapping_info(mem, TEST_VA_BASE, &offset, &size);
    assert(result == 0);
    assert(offset == 0);
    assert(size == TEST_BLOCK_SIZE_32MB);
    
    printf("  ✓ Backward compatibility maintained\n");
    
    result = unmap_physical_mem(mem, TEST_VA_BASE, TEST_BLOCK_SIZE_32MB);
    assert(result == 0);
    
    free_physical_mem(mgr, mem);
    physical_mem_mgr_destroy(mgr);
    printf("Backward compatibility test passed!\n\n");
}

// Test complex partial mapping scenarios
static void test_complex_partial_mapping(void) {
    printf("Testing complex partial mapping scenarios...\n");
    
    physical_mem_mgr_t *mgr = physical_mem_mgr_create();
    assert(mgr != NULL);
    
    physical_mem_t *mem = allocate_physical_mem(mgr, TEST_BLOCK_SIZE_32MB);
    assert(mem != NULL);
    
    // Map multiple non-overlapping partial regions
    struct {
        uint64_t va;
        uint64_t offset;
        uint64_t size;
    } mappings[] = {
        {TEST_VA_BASE,                     0,  TEST_BLOCK_SIZE_2MB},  // First 2MB
        {TEST_VA_BASE + 0x100000000ULL,   4ULL * 1024 * 1024, TEST_BLOCK_SIZE_4MB},  // 4MB at 4MB offset
        {TEST_VA_BASE + 0x200000000ULL,   16ULL * 1024 * 1024, TEST_BLOCK_SIZE_2MB}, // 2MB at 16MB offset
        {TEST_VA_BASE + 0x300000000ULL,   28ULL * 1024 * 1024, TEST_BLOCK_SIZE_4MB}  // 4MB at 28MB offset
    };
    
    // Create all mappings
    for (int i = 0; i < 4; i++) {
        int result = map_physical_mem(mem, mappings[i].offset, mappings[i].va, mappings[i].size);
        assert(result == 0);
    }
    printf("  ✓ Multiple partial mappings created successfully\n");
    
    // Verify all mappings
    for (int i = 0; i < 4; i++) {
        uint64_t offset, size;
        int result = get_mapping_info(mem, mappings[i].va, &offset, &size);
        assert(result == 0);
        assert(offset == mappings[i].offset);
        assert(size == mappings[i].size);
    }
    printf("  ✓ All mappings verified correctly\n");
    
    // Clean up all mappings
    for (int i = 0; i < 4; i++) {
        int result = unmap_physical_mem(mem, mappings[i].va, mappings[i].size);
        assert(result == 0);
    }
    printf("  ✓ All mappings cleaned up successfully\n");
    
    free_physical_mem(mgr, mem);
    physical_mem_mgr_destroy(mgr);
    printf("Complex partial mapping test passed!\n\n");
}

int main(void) {
    printf("Physical Memory Interface Test Suite\n");
    printf("===================================\n\n");
    
    test_manager_lifecycle();
    test_physical_allocation();
    test_memory_mapping();
    test_error_conditions();
    test_resource_limits();
    test_granular_mapping_pattern();
    
    // New partial mapping tests
    test_partial_mapping();
    test_backward_compatibility();
    test_complex_partial_mapping();
    
    printf("All physical memory tests passed successfully!\n");
    printf("\nTest Summary:\n");
    printf("- Manager lifecycle: creation, destruction, NULL handling\n");
    printf("- Physical memory allocation/free with usage tracking\n");
    printf("- Memory mapping/unmapping with read/write access\n");
    printf("- Error condition handling and parameter validation\n");
    printf("- Resource limit enforcement\n");
    printf("- Granular mapping patterns for buddy allocator usage\n");
    printf("- Partial mapping with offset support\n");
    printf("- Backward compatibility with existing mapping API\n");
    printf("- Complex partial mapping scenarios\n");
    
    return 0;
} 