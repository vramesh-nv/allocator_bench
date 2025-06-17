#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <algorithm>
#include <cassert>
#include "va_allocator.h"

// Helper function to get current time in microseconds
uint64_t get_time_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

// Structure to hold benchmark results
struct BenchmarkResult {
    uint64_t alloc_time_us;
    uint64_t free_time_us;
    uint64_t total_ops;
    uint64_t total_size;
};

// Run benchmark for a specific allocator type
BenchmarkResult run_benchmark(va_allocator_type_t type, 
                            size_t num_ops,
                            size_t min_size,
                            size_t max_size,
                            float alloc_ratio = 0.7) {
    va_allocator_t *allocator = va_allocator_init(type);
    assert(allocator != NULL);

    BenchmarkResult result = {0, 0, 0, 0};  // Initialize all fields to 0
    std::vector<uint64_t> addresses;
    std::vector<uint64_t> sizes;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> size_dist(min_size, max_size);
    std::uniform_real_distribution<> op_dist(0, 1);

    uint64_t total_alloc_time = 0;
    uint64_t total_free_time = 0;

    for (size_t i = 0; i < num_ops; i++) {
        if (op_dist(gen) < alloc_ratio || addresses.empty()) {
            // Allocation operation
            uint64_t size = size_dist(gen);
            uint64_t alloc_start = get_time_us();
            uint64_t addr = va_alloc(allocator, size);
            total_alloc_time += get_time_us() - alloc_start;

            if (addr != 0) {
                addresses.push_back(addr);
                sizes.push_back(size);
                result.total_size += size;
            }
        } else {
            // Free operation
            size_t idx = std::uniform_int_distribution<>(0, addresses.size() - 1)(gen);
            uint64_t free_start = get_time_us();
            va_free(allocator, addresses[idx]);
            total_free_time += get_time_us() - free_start;

            addresses.erase(addresses.begin() + idx);
            sizes.erase(sizes.begin() + idx);
        }
        result.total_ops++;
    }

    // Free remaining allocations
    for (uint64_t addr : addresses) {
        va_free(allocator, addr);
    }

    result.alloc_time_us = total_alloc_time;
    result.free_time_us = total_free_time;

    va_allocator_destroy(allocator);
    return result;
}

// Print benchmark results
void print_results(const char* allocator_name, const BenchmarkResult& result) {
    std::cout << "\nResults for " << allocator_name << ":" << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Total operations: " << result.total_ops << std::endl;
    std::cout << "Total allocated size: " << (result.total_size / (1024*1024)) << " MB" << std::endl;
    std::cout << "Average allocation time: " << (result.alloc_time_us / (float)result.total_ops) << " us" << std::endl;
    std::cout << "Average free time: " << (result.free_time_us / (float)result.total_ops) << " us" << std::endl;
}

// Run different benchmark scenarios
void run_benchmark_scenarios() {
    const size_t NUM_OPERATIONS = 100000;
    
    // Scenario 1: Small allocations (256B - 4KB)
    std::cout << "\nScenario 1: Small allocations (256B - 4KB)" << std::endl;
    auto default_small = run_benchmark(VA_ALLOCATOR_TYPE_DEFAULT, NUM_OPERATIONS, 256, 4096);
    auto arena_small = run_benchmark(VA_ALLOCATOR_TYPE_ARENA, NUM_OPERATIONS, 256, 4096);
    print_results("Default Allocator", default_small);
    print_results("Arena Allocator", arena_small);

    // Scenario 2: Medium allocations (4KB - 1MB)
    std::cout << "\nScenario 2: Medium allocations (4KB - 1MB)" << std::endl;
    auto default_medium = run_benchmark(VA_ALLOCATOR_TYPE_DEFAULT, NUM_OPERATIONS, 4096, 1024*1024);
    auto arena_medium = run_benchmark(VA_ALLOCATOR_TYPE_ARENA, NUM_OPERATIONS, 4096, 1024*1024);
    print_results("Default Allocator", default_medium);
    print_results("Arena Allocator", arena_medium);

    // Scenario 3: Large allocations (1MB - 32MB)
    std::cout << "\nScenario 3: Large allocations (1MB - 32MB)" << std::endl;
    auto default_large = run_benchmark(VA_ALLOCATOR_TYPE_DEFAULT, NUM_OPERATIONS, 1024*1024, 32*1024*1024);
    auto arena_large = run_benchmark(VA_ALLOCATOR_TYPE_ARENA, NUM_OPERATIONS, 1024*1024, 32*1024*1024);
    print_results("Default Allocator", default_large);
    print_results("Arena Allocator", arena_large);

    // Scenario 4: Mixed allocations with high fragmentation
    std::cout << "\nScenario 4: Mixed allocations with high fragmentation" << std::endl;
    auto default_mixed = run_benchmark(VA_ALLOCATOR_TYPE_DEFAULT, NUM_OPERATIONS, 256, 32*1024*1024, 0.5);
    auto arena_mixed = run_benchmark(VA_ALLOCATOR_TYPE_ARENA, NUM_OPERATIONS, 256, 32*1024*1024, 0.5);
    print_results("Default Allocator", default_mixed);
    print_results("Arena Allocator", arena_mixed);
}

int main(void) {
    std::cout << "Starting allocator benchmark comparison..." << std::endl;
    run_benchmark_scenarios();
    std::cout << "\nBenchmark completed!" << std::endl;
    return 0;
} 