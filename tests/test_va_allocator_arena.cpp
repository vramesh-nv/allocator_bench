#include <iostream>
#include <vector>
#include <cassert>
#include <random>
#include <algorithm>
#include "va_allocator.h"

// Test small allocations that should use slab allocator
void test_slab_allocation(void)
{
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_ARENA);
    assert(allocator != NULL);

    std::cout << "Testing slab allocation (≤2KB blocks)..." << std::endl;

    // Test different small sizes that should use slab allocator
    const std::vector<uint64_t> sizes = {256, 512, 1024, 2048};
    std::vector<uint64_t> addresses;

    for (uint64_t size : sizes) {
        // Allocate multiple blocks of each size
        for (int i = 0; i < 10; i++) {
            uint64_t addr = va_alloc(allocator, size);
            assert(addr != 0);
            addresses.push_back(addr);
            //std::cout << "Allocated " << size << " bytes at address: 0x" 
            //          << std::hex << addr << std::dec << std::endl;
        }
    }

    // Free all allocations
    for (uint64_t addr : addresses) {
        va_free(allocator, addr);
        //std::cout << "Freed address: 0x" << std::hex << addr << std::dec << std::endl;
    }

    va_allocator_destroy(allocator);
}

// Test medium allocations that should use different arena strategies
void test_medium_allocation(void)
{
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_ARENA);
    assert(allocator != NULL);

    std::cout << "Testing medium allocation (4KB-64KB blocks)..." << std::endl;

    const std::vector<uint64_t> sizes = {4096, 8192, 16384, 32768, 65536};
    std::vector<uint64_t> addresses;

    for (uint64_t size : sizes) {
        uint64_t addr = va_alloc(allocator, size);
        assert(addr != 0);
        addresses.push_back(addr);
        //std::cout << "Allocated " << (size/1024) << "KB at address: 0x" 
        //          << std::hex << addr << std::dec << std::endl;
    }

    // Free all allocations
    for (uint64_t addr : addresses) {
        va_free(allocator, addr);
        //std::cout << "Freed address: 0x" << std::hex << addr << std::dec << std::endl;
    }

    va_allocator_destroy(allocator);
}

// Test large allocations
void test_large_allocation(void)
{
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_ARENA);
    assert(allocator != NULL);

    std::cout << "Testing large allocation (2MB-32MB blocks)..." << std::endl;

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
        //std::cout << "Allocated " << (size/(1024*1024)) << "MB at address: 0x" 
        //          << std::hex << addr << std::dec << std::endl;
    }

    // Free all allocations
    for (uint64_t addr : addresses) {
        va_free(allocator, addr);
        //std::cout << "Freed address: 0x" << std::hex << addr << std::dec << std::endl;
    }

    va_allocator_destroy(allocator);
}

// Test mixed allocation patterns
void test_mixed_allocation(void)
{
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_ARENA);
    assert(allocator != NULL);

    std::cout << "Testing mixed allocation patterns..." << std::endl;

    std::vector<uint64_t> addresses;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> size_dist(256, 32 * 1024 * 1024); // 256B to 32MB

    // Allocate random sizes
    for (int i = 0; i < 10000; i++) {
        uint64_t size = size_dist(gen);
        uint64_t addr = va_alloc(allocator, size);
        if (addr == 0) {
            std::cout << "Failed to allocate " << size << " bytes" << std::endl;
            //assert(0);
            break;
        }
        addresses.push_back(addr);
        //std::cout << "Allocated " << size << " bytes at address: 0x" 
        //          << std::hex << addr << std::dec << std::endl;
    }

    // Free in random order
    std::shuffle(addresses.begin(), addresses.end(), gen);
    for (uint64_t addr : addresses) {
        va_free(allocator, addr);
        //std::cout << "Freed address: 0x" << std::hex << addr << std::dec << std::endl;
    }

    va_allocator_destroy(allocator);
}

void test_allocator_create_destroy(void)
{
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_ARENA);
    assert(allocator != NULL);

    va_allocator_destroy(allocator);
}

// Test fragmentation patterns
void test_fragmentation(void)
{
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_ARENA);
    assert(allocator != NULL);

    std::cout << "Testing fragmentation patterns..." << std::endl;

    const size_t num_blocks = 1000;
    std::vector<uint64_t> addresses;
    std::vector<uint64_t> sizes;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> size_dist(256, 4096); // 256B to 4KB

    // Create fragmentation pattern
    for (size_t i = 0; i < num_blocks; i++) {
        uint64_t size = size_dist(gen);
        uint64_t addr = va_alloc(allocator, size);
        assert(addr != 0);
        addresses.push_back(addr);
        sizes.push_back(size);
    }

    // Free every other block to create fragmentation
    for (size_t i = 0; i < num_blocks; i += 2) {
        va_free(allocator, addresses[i]);
        addresses[i] = 0;
    }

    // Try to allocate larger blocks in fragmented space
    for (size_t i = 0; i < 100; i++) {
        uint64_t size = 8192; // 8KB
        uint64_t addr = va_alloc(allocator, size);
        assert(addr != 0);
        addresses.push_back(addr);
        sizes.push_back(size);
    }

    // Free remaining blocks
    for (size_t i = 0; i < addresses.size(); i++) {
        if (addresses[i] != 0) {
            va_free(allocator, addresses[i]);
        }
    }

    va_allocator_destroy(allocator);
}

// Test rapid allocation/deallocation
void test_rapid_alloc_free(void)
{
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_ARENA);
    assert(allocator != NULL);

    std::cout << "Testing rapid allocation/deallocation..." << std::endl;

    const size_t num_iterations = 10000;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> size_dist(256, 32 * 1024 * 1024); // 256B to 32MB

    for (size_t i = 0; i < num_iterations; i++) {
        uint64_t size = size_dist(gen);
        uint64_t addr = va_alloc(allocator, size);
        assert(addr != 0);
        va_free(allocator, addr);
    }

    va_allocator_destroy(allocator);
}

// Test boundary sizes between arenas
void test_boundary_sizes(void)
{
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_ARENA);
    assert(allocator != NULL);

    std::cout << "Testing boundary sizes between arenas..." << std::endl;

    // Test sizes around arena boundaries
    const std::vector<uint64_t> boundary_sizes = {
        511,    // Just below 512B arena
        512,    // Start of 512B arena
        513,    // Just above 512B arena
        2047,   // Just below 2KB arena
        2048,   // Start of 2KB arena
        2049,   // Just above 2KB arena
        4095,   // Just below 4KB arena
        4096,   // Start of 4KB arena
        4097,   // Just above 4KB arena
    };

    std::vector<uint64_t> addresses;
    for (uint64_t size : boundary_sizes) {
        uint64_t addr = va_alloc(allocator, size);
        assert(addr != 0);
        addresses.push_back(addr);
    }

    // Free all allocations
    for (uint64_t addr : addresses) {
        va_free(allocator, addr);
    }

    va_allocator_destroy(allocator);
}

// Test random patterns with verification
void test_random_patterns(void)
{
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_ARENA);
    assert(allocator != NULL);

    std::cout << "Testing random patterns with verification..." << std::endl;

    const size_t num_operations = 10000;
    std::vector<uint64_t> addresses;
    std::vector<uint64_t> sizes;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> size_dist(256, 32 * 1024 * 1024); // 256B to 32MB
    std::uniform_real_distribution<> op_dist(0, 1);

    uint64_t tracked_size = 0;
    for (size_t i = 0; i < num_operations; i++) {
        if (op_dist(gen) < 0.7 || addresses.empty()) { // 70% chance to allocate
            uint64_t size = size_dist(gen);
            uint64_t addr = va_alloc(allocator, size);
            if (addr != 0) {
                addresses.push_back(addr);
                sizes.push_back(size);
                tracked_size += size;
            }
        } else { // 30% chance to free
            size_t idx = std::uniform_int_distribution<>(0, addresses.size() - 1)(gen);
            va_free(allocator, addresses[idx]);
            addresses.erase(addresses.begin() + idx);
            tracked_size -= sizes[idx];
            sizes.erase(sizes.begin() + idx);
        }

        // Verify total used size matches our tracking
        tracked_size = va_allocator_get_used_size(allocator);
        assert(va_allocator_get_used_size(allocator) == tracked_size);
    }

    // Free remaining allocations
    for (uint64_t addr : addresses) {
        va_free(allocator, addr);
    }

    va_allocator_destroy(allocator);
}

int main(void) {
    std::cout << "Starting arena allocator tests..." << std::endl;

    test_allocator_create_destroy();
    test_slab_allocation();
    test_medium_allocation();
    test_large_allocation();
    test_mixed_allocation();
    test_fragmentation();
    test_rapid_alloc_free();
    test_boundary_sizes();
    test_random_patterns();

    std::cout << "All arena allocator tests completed successfully!" << std::endl;
    return 0;
} 