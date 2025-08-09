# Simple Allocator Benchmark

A straightforward benchmark comparing DEFAULT vs ARENA allocator performance with controlled memory usage.

## Overview

This benchmark tests two allocation strategies:
- **DEFAULT**: Standard allocator implementation
- **ARENA**: Arena-based allocator implementation

The benchmark stays within a **1GB total memory limit** and focuses on clear, simple comparisons.

## Test Cases

### 1. Same Size Allocations

Tests allocation/deallocation performance for uniform block sizes:

- **Small blocks**: 50,000 × 4KB = 195MB total
- **Medium blocks**: 8,000 × 64KB = 500MB total  
- **Large blocks**: 512 × 1MB = 512MB total

### 2. Mixed Size Allocations

Tests allocation performance with varied block sizes in a single test:

- 16,384 × 4KB = 64MB
- 4,096 × 64KB = 256MB
- 192 × 1MB = 192MB
- **Total: 512MB**

## Key Metrics

- **Allocation Time**: Time spent in `va_alloc()` calls
- **Deallocation Time**: Time spent in `va_free()` calls
- **Total Time**: Complete test execution time
- **Successful Allocations**: Number of allocations that succeeded
- **Peak Memory**: Maximum virtual memory used
- **Speedup Ratios**: Arena vs Default performance comparison

## Sample Results

```
=== Small Block Allocations (4KB each) ===
Allocator  | Alloc Time   | Free Time    | Total Time   | Allocs     | Total MB     | Peak MB   
-----------|--------------|--------------|--------------|------------|--------------|----------
DEFAULT    |    5906.69 ms |       1.26 ms |    5907.95 ms |      50000 |     195.31 MB |      195 MB
ARENA      |     198.31 ms |       1.61 ms |     199.92 ms |      50000 |     195.31 MB |      195 MB

Comparison (Arena vs Default):
  Allocation speedup: 29.79x
  Deallocation speedup: 0.78x
  Overall speedup: 29.55x
```

## Key Findings

1. **Arena allocator shows significant speedup** for allocation-heavy workloads
2. **Speedup is most pronounced for smaller blocks** (29x for 4KB blocks)
3. **Deallocation performance** is roughly comparable between implementations
4. **Memory usage** is similar between both allocators
5. **Mixed workloads** show good Arena performance (15x speedup)

## Building and Running

### Build
```bash
cd build
cmake ..
make simple_benchmark
```

### Run
```bash
./simple_benchmark
```

### Run as Test
```bash
make test
# Or specifically:
ctest -R simple_benchmark_test
```

## Implementation Details

- **Memory limit**: Hard-coded 1GB maximum total allocation
- **Test isolation**: Each test uses a fresh allocator instance
- **Timing**: Uses `clock_gettime(CLOCK_MONOTONIC)` for precise measurements
- **Error handling**: Gracefully handles allocation failures
- **Memory tracking**: Monitors peak virtual memory usage

## Files

- `tests/simple_benchmark.c` - Main benchmark implementation
- `CMakeLists.txt` - Build configuration with `simple_benchmark` target

## Performance Insights

### Why Arena is Faster

1. **Reduced allocation overhead** through pre-allocated memory pools
2. **Better memory locality** for allocation metadata
3. **Optimized for bulk allocation patterns**
4. **Less fragmentation** in allocation structures

### When Default Might Be Better

1. **Sparse allocation patterns** where Arena overhead isn't amortized
2. **Very large allocations** where direct allocation is more efficient
3. **Memory-constrained environments** where Arena pre-allocation is wasteful

## Extending the Benchmark

To add new test cases:

1. Define new test functions following the `test_*_allocations()` pattern
2. Add calls in `main()` with appropriate `print_header()` and `print_comparison()`
3. Ensure total memory stays under 1GB limit
4. Update this documentation

## Limitations

- Tests only allocation/deallocation, not actual memory usage patterns
- Fixed test sizes may not represent all real-world scenarios
- Single-threaded performance only
- No fragmentation analysis beyond peak memory measurement
