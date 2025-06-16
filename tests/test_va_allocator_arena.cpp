#include <iostream>
#include <vector>
#include <cassert>
#include <random>
#include <algorithm>
#include "va_allocator.h"

/*// Test small allocations that should use slab allocator
void test_slab_allocation(void) {
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_ARENA);
    assert(allocator != NULL);

    std::cout << "\nTesting slab allocation (≤2KB blocks)..." << std::endl;

    // Test different small sizes that should use slab allocator
    const std::vector<uint64_t> sizes = {256, 512, 1024, 2048};
    std::vector<uint64_t> addresses;

    for (uint64_t size : sizes) {
        // Allocate multiple blocks of each size
        for (int i = 0; i < 10; i++) {
            uint64_t addr = va_alloc(allocator, size);
            assert(addr != 0);
            addresses.push_back(addr);
            std::cout << "Allocated " << size << " bytes at address: 0x" 
                      << std::hex << addr << std::dec << std::endl;
        }
    }

    // Free all allocations
    for (uint64_t addr : addresses) {
        va_free(allocator, addr);
        std::cout << "Freed address: 0x" << std::hex << addr << std::dec << std::endl;
    }

    va_allocator_destroy(allocator);
}

// Test medium allocations that should use different arena strategies
void test_medium_allocation(void) {
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_ARENA);
    assert(allocator != NULL);

    std::cout << "\nTesting medium allocation (4KB-64KB blocks)..." << std::endl;

    const std::vector<uint64_t> sizes = {4096, 8192, 16384, 32768, 65536};
    std::vector<uint64_t> addresses;

    for (uint64_t size : sizes) {
        uint64_t addr = va_alloc(allocator, size);
        assert(addr != 0);
        addresses.push_back(addr);
        std::cout << "Allocated " << (size/1024) << "KB at address: 0x" 
                  << std::hex << addr << std::dec << std::endl;
    }

    // Free all allocations
    for (uint64_t addr : addresses) {
        va_free(allocator, addr);
        std::cout << "Freed address: 0x" << std::hex << addr << std::dec << std::endl;
    }

    va_allocator_destroy(allocator);
}

// Test large allocations
void test_large_allocation(void) {
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_ARENA);
    assert(allocator != NULL);

    std::cout << "\nTesting large allocation (2MB-32MB blocks)..." << std::endl;

    const std::vector<uint64_t> sizes = {
        2 * 1024 * 1024,    // 2MB
        4 * 1024 * 1024,    // 4MB
        8 * 1024 * 1024,    // 8MB
        16 * 1024 * 1024,   // 16MB
        32 * 1024 * 1024    // 32MB
    };
    std::vector<uint64_t> addresses;

    for (uint64_t size : sizes) {
        uint64_t addr = va_alloc(allocator, size);
        assert(addr != 0);
        addresses.push_back(addr);
        std::cout << "Allocated " << (size/(1024*1024)) << "MB at address: 0x" 
                  << std::hex << addr << std::dec << std::endl;
    }

    // Free all allocations
    for (uint64_t addr : addresses) {
        va_free(allocator, addr);
        std::cout << "Freed address: 0x" << std::hex << addr << std::dec << std::endl;
    }

    va_allocator_destroy(allocator);
}

// Test mixed allocation patterns
void test_mixed_allocation(void) {
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_ARENA);
    assert(allocator != NULL);

    std::cout << "\nTesting mixed allocation patterns..." << std::endl;

    std::vector<uint64_t> addresses;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> size_dist(256, 32 * 1024 * 1024); // 256B to 32MB

    // Allocate random sizes
    for (int i = 0; i < 100; i++) {
        uint64_t size = size_dist(gen);
        uint64_t addr = va_alloc(allocator, size);
        if (addr == 0) {
            std::cout << "Failed to allocate " << size << " bytes" << std::endl;
            break;
        }
        addresses.push_back(addr);
        std::cout << "Allocated " << size << " bytes at address: 0x" 
                  << std::hex << addr << std::dec << std::endl;
    }

    // Free in random order
    std::shuffle(addresses.begin(), addresses.end(), gen);
    for (uint64_t addr : addresses) {
        va_free(allocator, addr);
        std::cout << "Freed address: 0x" << std::hex << addr << std::dec << std::endl;
    }

    va_allocator_destroy(allocator);
}

// Test arena allocator statistics
void test_arena_stats(void) {
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_ARENA);
    assert(allocator != NULL);

    std::cout << "\nTesting arena allocator statistics..." << std::endl;

    uint64_t total_size = va_allocator_get_total_size(allocator);
    uint64_t initial_used = va_allocator_get_used_size(allocator);

    std::cout << "Total VA space: " << (total_size/(1024*1024)) << "MB" << std::endl;
    std::cout << "Initial used space: " << (initial_used/(1024*1024)) << "MB" << std::endl;

    // Allocate some memory
    std::vector<uint64_t> addresses;
    for (int i = 0; i < 10; i++) {
        uint64_t size = 1024 * 1024; // 1MB
        uint64_t addr = va_alloc(allocator, size);
        assert(addr != 0);
        addresses.push_back(addr);
    }

    uint64_t after_alloc_used = va_allocator_get_used_size(allocator);
    std::cout << "Used space after allocation: " << (after_alloc_used/(1024*1024)) << "MB" << std::endl;

    // Free all allocations
    for (uint64_t addr : addresses) {
        va_free(allocator, addr);
    }

    uint64_t final_used = va_allocator_get_used_size(allocator);
    std::cout << "Final used space: " << (final_used/(1024*1024)) << "MB" << std::endl;

    va_allocator_destroy(allocator);
}*/

void test_basic_alloc_free_small(void)
{
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_ARENA);
    assert(allocator != NULL);

    size_t num_allocs = 100000;
    size_t alloc_size = 256;
    for (size_t i = 0; i < num_allocs; i++) {
        uint64_t addr = va_alloc(allocator, alloc_size);
        if (addr == 0) {
            std::cerr << "Failed to allocate " << alloc_size << " bytes, iteration: " << i << std::endl;
            assert(0);
        }
        va_free(allocator, addr);
    }

    va_allocator_destroy(allocator);
}

void test_alloc_free_multiple_small(void)
{
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_ARENA);
    assert(allocator != NULL);

    size_t num_allocs = 100000;
    size_t alloc_size = 256;
    std::vector<uint64_t> addresses(num_allocs);
    for (size_t i = 0; i < num_allocs; i++) {
        addresses[i] = va_alloc(allocator, alloc_size);
        if (addresses[i] == 0) {
            std::cerr << "Failed to allocate " << alloc_size << " bytes, iteration: " << i << std::endl;
            assert(0);
            break;
        }
    }

    for (size_t i = 0; i < num_allocs; i++) {
        va_free(allocator, addresses[i]);
    }

    va_allocator_destroy(allocator);
}

void test_allocator_create_destroy(void)
{
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_ARENA);
    assert(allocator != NULL);

    va_allocator_destroy(allocator);
}

int main(void) {
    std::cout << "Starting arena allocator tests..." << std::endl;

    test_allocator_create_destroy();
    test_basic_alloc_free_small();
    test_alloc_free_multiple_small();

    std::cout << "All arena allocator tests completed successfully!" << std::endl;
    return 0;
} 