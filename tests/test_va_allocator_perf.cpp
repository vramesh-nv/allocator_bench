#include <iostream>
#include <vector>
#include <cassert>
#include <chrono>
#include <iomanip>
#include <numeric>
#include <algorithm>
#include <cmath>
#include "va_allocator.h"

// Helper function to get current timestamp in nanoseconds
uint64_t get_timestamp() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

// Structure to hold timing statistics
struct TimingStats {
    double min;
    double max;
    double mean;
    double median;
    double stddev;
};

// Calculate timing statistics
TimingStats calculate_stats(const std::vector<double>& times) {
    TimingStats stats;
    if (times.empty()) {
        stats.min = stats.max = stats.mean = stats.median = stats.stddev = 0.0;
        return stats;
    }

    // Sort for median calculation
    std::vector<double> sorted_times = times;
    std::sort(sorted_times.begin(), sorted_times.end());

    stats.min = sorted_times.front();
    stats.max = sorted_times.back();
    stats.mean = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
    stats.median = sorted_times[times.size() / 2];

    // Calculate standard deviation
    double sq_sum = std::inner_product(times.begin(), times.end(), times.begin(), 0.0);
    stats.stddev = std::sqrt(sq_sum / times.size() - stats.mean * stats.mean);

    return stats;
}

void test_same_size_allocation_performance(uint64_t block_size, size_t num_blocks) {
    std::cout << "\nTesting performance for " << num_blocks << " allocations of " 
              << (block_size / 1024.0) << " KB blocks\n";
    std::cout << "--------------------------------------------------------\n";

    va_allocator_t* allocator = va_allocator_init(VA_ALLOCATOR_TYPE_DEFAULT);
    assert(allocator != NULL);

    std::vector<uint64_t> addresses;
    std::vector<double> alloc_times;
    std::vector<double> free_times;
    addresses.reserve(num_blocks);
    alloc_times.reserve(num_blocks);
    free_times.reserve(num_blocks);

    // Measure allocation performance
    for (size_t i = 0; i < num_blocks; i++) {
        uint64_t start_time = get_timestamp();
        uint64_t addr = va_alloc(allocator, block_size);
        uint64_t end_time = get_timestamp();
        
        assert(addr != 0);
        addresses.push_back(addr);
        alloc_times.push_back((end_time - start_time) / 1000.0); // Convert to microseconds
    }

    // Measure deallocation performance
    for (size_t i = 0; i < num_blocks; i++) {
        uint64_t start_time = get_timestamp();
        va_free(allocator, addresses[i]);
        uint64_t end_time = get_timestamp();
        
        free_times.push_back((end_time - start_time) / 1000.0); // Convert to microseconds
    }

    // Calculate and display statistics
    TimingStats alloc_stats = calculate_stats(alloc_times);
    TimingStats free_stats = calculate_stats(free_times);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Allocation Statistics (μs):\n";
    std::cout << "  Min: " << alloc_stats.min << "\n";
    std::cout << "  Max: " << alloc_stats.max << "\n";
    std::cout << "  Mean: " << alloc_stats.mean << "\n";
    std::cout << "  Median: " << alloc_stats.median << "\n";
    std::cout << "  StdDev: " << alloc_stats.stddev << "\n\n";

    std::cout << "Deallocation Statistics (μs):\n";
    std::cout << "  Min: " << free_stats.min << "\n";
    std::cout << "  Max: " << free_stats.max << "\n";
    std::cout << "  Mean: " << free_stats.mean << "\n";
    std::cout << "  Median: " << free_stats.median << "\n";
    std::cout << "  StdDev: " << free_stats.stddev << "\n";

    va_allocator_destroy(allocator);
}

int main() {
    // Test different block sizes
    const size_t num_blocks = 1000;  // Number of blocks to allocate

    // Test with 4KB blocks
    test_same_size_allocation_performance(4 * 1024, num_blocks);

    // Test with 1MB blocks
    test_same_size_allocation_performance(1024 * 1024, num_blocks);

    // Test with 16MB blocks
    //test_same_size_allocation_performance(16 * 1024 * 1024, num_blocks);

    return 0;
} 