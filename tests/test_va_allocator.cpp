#include <iostream>
#include <vector>
#include <cassert>
#include "va_allocator.h"

void test_basic_allocation(void) {
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_DEFAULT);
    assert(allocator != NULL);

    // Test basic allocation
    uint64_t addr = va_alloc(allocator, 1024);
    assert(addr != 0);
    std::cout << "Allocated 1KB at address: 0x" << std::hex << addr << std::dec << std::endl;

    // Test freeing
    va_free(allocator, addr);
    std::cout << "Freed 1KB at address: 0x" << std::hex << addr << std::dec << std::endl;

    va_allocator_destroy(allocator);
}

void test_fragmentation(void) {
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_DEFAULT);
    assert(allocator != NULL);

    // Allocate several blocks
    std::vector<uint64_t> addresses;
    for (int i = 0; i < 5; i++) {
        uint64_t addr = va_alloc(allocator, 1024 * 1024);  // 1MB each
        assert(addr != 0);
        addresses.push_back(addr);
        std::cout << "Allocated 1MB at address: 0x" << std::hex << addr << std::dec << std::endl;
    }

    // Free every other block to create fragmentation
    for (int i = 0; i < 5; i += 2) {
        va_free(allocator, addresses[i]);
        std::cout << "Freed 1MB at address: 0x" << std::hex << addresses[i] << std::dec << std::endl;
    }

    // Try to allocate a large block
    uint64_t large_addr = va_alloc(allocator, 2 * 1024 * 1024);  // 2MB
    if (large_addr == 0) {
        std::cout << "Successfully detected fragmentation: Could not allocate 2MB block" << std::endl;
    } else {
        std::cout << "Warning: Allocated 2MB block despite fragmentation at address: 0x" 
                  << std::hex << large_addr << std::dec << std::endl;
        va_free(allocator, large_addr);
    }

    va_allocator_destroy(allocator);
}

void test_severe_fragmentation(void) {
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_DEFAULT);
    assert(allocator != NULL);

    const uint64_t total_va = va_allocator_get_total_size(allocator);

    // Start with 1KB and 1MB
    uint64_t small_size = 1024;        // 1KB
    uint64_t medium_size = 1024 * 1024; // 1MB
    uint64_t iteration = 0;
    uint64_t total_allocated = 0;

    std::vector<uint64_t> small_blocks;
    std::vector<uint64_t> medium_blocks;

    while (true) {
        iteration++;

        // Allocate new blocks first
        uint64_t small_addr = va_alloc(allocator, small_size);
        if (small_addr == 0) {
            std::cout << "Failed to allocate small block of size " << (small_size / 1024) << " KB" << std::endl;
            break;
        }
        small_blocks.push_back(small_addr);
        total_allocated += small_size;

        uint64_t medium_addr = va_alloc(allocator, medium_size);
        if (medium_addr == 0) {
            std::cout << "Failed to allocate medium block of size " << (medium_size / (1024 * 1024)) << " MB" << std::endl;
            break;
        }
        medium_blocks.push_back(medium_addr);
        total_allocated += medium_size;

        // Then free previous blocks if they exist
        if (small_blocks.size() > 1) {
            va_free(allocator, small_blocks[small_blocks.size()-2]);
            small_blocks.erase(small_blocks.begin() + small_blocks.size()-2);
            total_allocated -= small_size - 1024;
        }
        if (medium_blocks.size() > 1) {
            va_free(allocator, medium_blocks[medium_blocks.size()-2]);
            medium_blocks.erase(medium_blocks.begin() + medium_blocks.size()-2);
            total_allocated -= medium_size - 1024 * 1024;
        }

        // bump the sizes for next iteration
        small_size += 1024;
        medium_size += 1024 * 1024;
    }

    // Calculate fragmentation metrics
    uint64_t total_free = total_va - va_allocator_get_used_size(allocator);
    float fragmentation_ratio = (float)medium_size / (total_free) * 100;
    
    std::cout << "\nFragmentation Analysis:" << std::endl;
    std::cout << "Used VA space: " << (va_allocator_get_used_size(allocator) / (1024 * 1024)) << " MB" << std::endl;
    std::cout << "Total iterations completed: " << iteration << std::endl;
    std::cout << "Final small block size: " << (small_size - 1024) << " bytes" << std::endl;
    std::cout << "Final medium block size: " << (medium_size - 1024 * 1024) << " bytes" << std::endl;
    std::cout << "Total allocated: " << (total_allocated / (1024 * 1024)) << " MB" << std::endl;
    std::cout << "Total free space: " << (total_free / (1024 * 1024)) << " MB" << std::endl;
    std::cout << "Fragmentation ratio: " << fragmentation_ratio << "%" << std::endl;
    std::cout << "Number of allocated blocks: " << (small_blocks.size() + medium_blocks.size()) << std::endl;

    // Free blocks in reverse order of allocation
    while (!medium_blocks.empty()) {
        uint64_t addr = medium_blocks.back();
        va_free(allocator, addr);
        medium_blocks.pop_back();
    }

    while (!small_blocks.empty()) {
        uint64_t addr = small_blocks.back();
        va_free(allocator, addr);
        small_blocks.pop_back();
    }

    va_allocator_destroy(allocator);
}

int main(void) {
    std::cout << "Testing basic allocation..." << std::endl;
    test_basic_allocation();
    std::cout << "\nTesting fragmentation..." << std::endl;
    test_fragmentation();
    std::cout << "\nTesting severe fragmentation..." << std::endl;
    test_severe_fragmentation();
    return 0;
} 