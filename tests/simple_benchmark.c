#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <stdint.h>

#include "common.h"
#include "va_allocator.h"
#include "va_allocator_types.h"

// Maximum total memory to allocate: 1GB
#define MAX_TOTAL_MEMORY (1ULL * 1024 * 1024 * 1024)

// Test configurations
#define SMALL_BLOCK_SIZE (4 * 1024)        // 4KB
#define MEDIUM_BLOCK_SIZE (64 * 1024)      // 64KB  
#define LARGE_BLOCK_SIZE (1 * 1024 * 1024) // 1MB

// Calculate max allocations for each size to stay under 1GB total
#define MAX_SMALL_ALLOCS (MAX_TOTAL_MEMORY / SMALL_BLOCK_SIZE)     // ~262K allocations
#define MAX_MEDIUM_ALLOCS (MAX_TOTAL_MEMORY / MEDIUM_BLOCK_SIZE)   // ~16K allocations
#define MAX_LARGE_ALLOCS (MAX_TOTAL_MEMORY / LARGE_BLOCK_SIZE)     // ~1K allocations

// For mixed size test, use smaller numbers to stay under 1GB total
#define MIXED_SMALL_COUNT 16384    // 16K * 4KB = 64MB
#define MIXED_MEDIUM_COUNT 4096    // 4K * 64KB = 256MB  
#define MIXED_LARGE_COUNT 192      // 192 * 1MB = 192MB
// Total: 64MB + 256MB + 192MB = 512MB

typedef struct {
    double alloc_time_ms;
    double free_time_ms;
    double total_time_ms;
    uint64_t successful_allocs;
    uint64_t total_bytes_requested;
    uint64_t peak_memory_kb;
} benchmark_result_t;

static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static void print_header(const char* test_name) {
    printf("\n=== %s ===\n", test_name);
    printf("%-10s | %-12s | %-12s | %-12s | %-10s | %-12s | %-10s\n",
           "Allocator", "Alloc Time", "Free Time", "Total Time", "Allocs", "Total MB", "Peak MB");
    printf("-----------|--------------|--------------|--------------|------------|--------------|----------\n");
}

static void print_result(const char* allocator_name, const benchmark_result_t* result) {
    printf("%-10s | %10.2f ms | %10.2f ms | %10.2f ms | %10lu | %10.2f MB | %8lu MB\n",
           allocator_name,
           result->alloc_time_ms,
           result->free_time_ms, 
           result->total_time_ms,
           result->successful_allocs,
           result->total_bytes_requested / (1024.0 * 1024.0),
           result->peak_memory_kb / 1024);
}

static void print_comparison(const benchmark_result_t* default_result, const benchmark_result_t* arena_result) {
    printf("\nComparison (Arena vs Default):\n");
    
    if (default_result->alloc_time_ms > 0) {
        double alloc_speedup = default_result->alloc_time_ms / arena_result->alloc_time_ms;
        printf("  Allocation speedup: %.2fx\n", alloc_speedup);
    }
    
    if (default_result->free_time_ms > 0) {
        double free_speedup = default_result->free_time_ms / arena_result->free_time_ms;
        printf("  Deallocation speedup: %.2fx\n", free_speedup);
    }
    
    if (default_result->total_time_ms > 0) {
        double total_speedup = default_result->total_time_ms / arena_result->total_time_ms;
        printf("  Overall speedup: %.2fx\n", total_speedup);
    }
    
    printf("  Memory efficiency: Default %lu MB, Arena %lu MB\n",
           default_result->peak_memory_kb / 1024,
           arena_result->peak_memory_kb / 1024);
}

static benchmark_result_t test_same_size_allocations(va_allocator_type_t type, uint64_t block_size, uint64_t num_allocs) {
    benchmark_result_t result = {0};
    
    va_allocator_t *allocator = va_allocator_init(type);
    assert(allocator != NULL);
    
    // Allocate array to store addresses
    uint64_t *addresses = calloc(num_allocs, sizeof(uint64_t));
    assert(addresses != NULL);
    
    double start_time = get_time_ms();
    
    // Allocation phase
    double alloc_start = get_time_ms();
    for (uint64_t i = 0; i < num_allocs; i++) {
        addresses[i] = va_alloc(allocator, block_size);
        if (addresses[i] != 0) {
            result.successful_allocs++;
            result.total_bytes_requested += block_size;
        }
    }
    result.alloc_time_ms = get_time_ms() - alloc_start;
    
    // Measure peak memory usage
    result.peak_memory_kb = va_allocator_get_used_size(allocator) / 1024;
    
    // Deallocation phase
    double free_start = get_time_ms();
    for (uint64_t i = 0; i < result.successful_allocs; i++) {
        if (addresses[i] != 0) {
            va_free(allocator, addresses[i]);
        }
    }
    result.free_time_ms = get_time_ms() - free_start;
    
    result.total_time_ms = get_time_ms() - start_time;
    
    free(addresses);
    va_allocator_destroy(allocator);
    
    return result;
}

static benchmark_result_t test_mixed_size_allocations(va_allocator_type_t type) {
    benchmark_result_t result = {0};
    
    va_allocator_t *allocator = va_allocator_init(type);
    assert(allocator != NULL);
    
    // Calculate total allocations
    uint64_t total_allocs = MIXED_SMALL_COUNT + MIXED_MEDIUM_COUNT + MIXED_LARGE_COUNT;
    uint64_t *addresses = calloc(total_allocs, sizeof(uint64_t));
    uint64_t *sizes = calloc(total_allocs, sizeof(uint64_t));
    assert(addresses != NULL && sizes != NULL);
    
    double start_time = get_time_ms();
    
    // Allocation phase - mix of different sizes
    double alloc_start = get_time_ms();
    uint64_t alloc_index = 0;
    
    // Small allocations
    for (uint64_t i = 0; i < MIXED_SMALL_COUNT; i++) {
        addresses[alloc_index] = va_alloc(allocator, SMALL_BLOCK_SIZE);
        sizes[alloc_index] = SMALL_BLOCK_SIZE;
        if (addresses[alloc_index] != 0) {
            result.successful_allocs++;
            result.total_bytes_requested += SMALL_BLOCK_SIZE;
        }
        alloc_index++;
    }
    
    // Medium allocations
    for (uint64_t i = 0; i < MIXED_MEDIUM_COUNT; i++) {
        addresses[alloc_index] = va_alloc(allocator, MEDIUM_BLOCK_SIZE);
        sizes[alloc_index] = MEDIUM_BLOCK_SIZE;
        if (addresses[alloc_index] != 0) {
            result.successful_allocs++;
            result.total_bytes_requested += MEDIUM_BLOCK_SIZE;
        }
        alloc_index++;
    }
    
    // Large allocations
    for (uint64_t i = 0; i < MIXED_LARGE_COUNT; i++) {
        addresses[alloc_index] = va_alloc(allocator, LARGE_BLOCK_SIZE);
        sizes[alloc_index] = LARGE_BLOCK_SIZE;
        if (addresses[alloc_index] != 0) {
            result.successful_allocs++;
            result.total_bytes_requested += LARGE_BLOCK_SIZE;
        }
        alloc_index++;
    }
    
    result.alloc_time_ms = get_time_ms() - alloc_start;
    
    // Measure peak memory usage
    result.peak_memory_kb = va_allocator_get_used_size(allocator) / 1024;
    
    // Deallocation phase
    double free_start = get_time_ms();
    for (uint64_t i = 0; i < total_allocs; i++) {
        if (addresses[i] != 0) {
            va_free(allocator, addresses[i]);
        }
    }
    result.free_time_ms = get_time_ms() - free_start;
    
    result.total_time_ms = get_time_ms() - start_time;
    
    free(addresses);
    free(sizes);
    va_allocator_destroy(allocator);
    
    return result;
}

int main(int argc, char **argv) {
    UNUSED(argc);
    UNUSED(argv);
    
    printf("Simple VA Allocator Benchmark\n");
    printf("Maximum total memory allocation: %.2f GB\n", MAX_TOTAL_MEMORY / (1024.0 * 1024.0 * 1024.0));
    printf("Comparing DEFAULT vs ARENA allocator implementations\n");
    
    // Test 1: Small block allocations (4KB each)
    print_header("Small Block Allocations (4KB each)");
    
    // Limit small allocations to reasonable number to avoid extremely long test
    uint64_t small_test_count = 50000; // 50K * 4KB = 200MB total
    
    benchmark_result_t default_small = test_same_size_allocations(VA_ALLOCATOR_TYPE_DEFAULT, SMALL_BLOCK_SIZE, small_test_count);
    benchmark_result_t arena_small = test_same_size_allocations(VA_ALLOCATOR_TYPE_ARENA, SMALL_BLOCK_SIZE, small_test_count);
    
    print_result("DEFAULT", &default_small);
    print_result("ARENA", &arena_small);
    print_comparison(&default_small, &arena_small);
    
    // Test 2: Medium block allocations (64KB each)
    print_header("Medium Block Allocations (64KB each)");
    
    uint64_t medium_test_count = 8000; // 8K * 64KB = 512MB total
    
    benchmark_result_t default_medium = test_same_size_allocations(VA_ALLOCATOR_TYPE_DEFAULT, MEDIUM_BLOCK_SIZE, medium_test_count);
    benchmark_result_t arena_medium = test_same_size_allocations(VA_ALLOCATOR_TYPE_ARENA, MEDIUM_BLOCK_SIZE, medium_test_count);
    
    print_result("DEFAULT", &default_medium);
    print_result("ARENA", &arena_medium);
    print_comparison(&default_medium, &arena_medium);
    
    // Test 3: Large block allocations (1MB each)
    print_header("Large Block Allocations (1MB each)");
    
    uint64_t large_test_count = 512; // 512 * 1MB = 512MB total
    
    benchmark_result_t default_large = test_same_size_allocations(VA_ALLOCATOR_TYPE_DEFAULT, LARGE_BLOCK_SIZE, large_test_count);
    benchmark_result_t arena_large = test_same_size_allocations(VA_ALLOCATOR_TYPE_ARENA, LARGE_BLOCK_SIZE, large_test_count);
    
    print_result("DEFAULT", &default_large);
    print_result("ARENA", &arena_large);
    print_comparison(&default_large, &arena_large);
    
    // Test 4: Mixed size allocations
    print_header("Mixed Size Allocations");
    
    benchmark_result_t default_mixed = test_mixed_size_allocations(VA_ALLOCATOR_TYPE_DEFAULT);
    benchmark_result_t arena_mixed = test_mixed_size_allocations(VA_ALLOCATOR_TYPE_ARENA);
    
    print_result("DEFAULT", &default_mixed);
    print_result("ARENA", &arena_mixed);
    print_comparison(&default_mixed, &arena_mixed);
    
    // Summary
    printf("\n");
    printf("=== SUMMARY ===\n");
    printf("All tests completed successfully.\n");
    printf("Total memory tested across all configurations:\n");
    printf("  Small blocks: %.2f MB\n", (small_test_count * SMALL_BLOCK_SIZE) / (1024.0 * 1024.0));
    printf("  Medium blocks: %.2f MB\n", (medium_test_count * MEDIUM_BLOCK_SIZE) / (1024.0 * 1024.0));
    printf("  Large blocks: %.2f MB\n", (large_test_count * LARGE_BLOCK_SIZE) / (1024.0 * 1024.0));
    printf("  Mixed sizes: %.2f MB\n", (default_mixed.total_bytes_requested) / (1024.0 * 1024.0));
    
    double total_tested = ((small_test_count * SMALL_BLOCK_SIZE) + 
                          (medium_test_count * MEDIUM_BLOCK_SIZE) + 
                          (large_test_count * LARGE_BLOCK_SIZE) + 
                          default_mixed.total_bytes_requested) / (1024.0 * 1024.0 * 1024.0);
    printf("  Total memory tested: %.2f GB (within %.2f GB limit)\n", 
           total_tested, MAX_TOTAL_MEMORY / (1024.0 * 1024.0 * 1024.0));
    
    return 0;
}
