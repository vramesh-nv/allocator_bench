# VA Allocator

A high-performance virtual address (VA) allocator implementation with multiple allocation strategies for memory management.

## Overview

This repository implements and compares two memory allocation strategies:

- **DEFAULT**: Standard allocator using radix trees and physical memory blocks
- **ARENA**: Arena-based allocator optimized for bulk allocation patterns

The allocator manages virtual address space with backing physical memory, supporting efficient allocation, deallocation, and memory mapping operations.

## Key Features

- **Buddy allocator** for physical memory management (2MB-32MB blocks)
- **Radix tree** for fast size-based allocation lookup
- **Virtual address tracking** with physical memory backing
- **Multiple allocation strategies** with pluggable interface
- **Memory pool management** with reference counting
- **Comprehensive test suite** with performance benchmarks

## Architecture

```
VA Allocator Interface
├── DEFAULT Implementation
│   ├── Radix tree for size ordering
│   ├── Address-ordered block lists
│   └── Physical memory manager
└── ARENA Implementation
    ├── Size-based arena selection
    ├── Slab allocators for small blocks
    └── Buddy allocator backing
```

## Performance

Benchmark results show significant performance advantages for the ARENA allocator:

- **29x faster** allocation for small blocks (4KB)
- **25x faster** allocation for medium blocks (64KB)
- **5x faster** allocation for large blocks (1MB)
- **15x faster** for mixed-size workloads

## Building

```bash
mkdir build && cd build
cmake ..
make
```

## Testing

```bash
# Run all tests
make test

# Run specific benchmark
./simple_benchmark

# Run individual tests
./test_buddy
./test_arena_alloc
./sanity
```

## Usage

```c
#include "va_allocator.h"

// Create allocator
va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_ARENA);

// Allocate memory
uint64_t addr = va_alloc(allocator, size);

// Free memory
va_free(allocator, addr);

// Cleanup
va_allocator_destroy(allocator);
```

## Components

- **`src/`** - Core allocator implementations
- **`tests/`** - Test suite and benchmarks
- **`utils/`** - Utility libraries (radix tree, bit vectors, AVL tree)
- **`inc/`** - Public header files

## Documentation

- [Simple Benchmark](SIMPLE_BENCHMARK.md) - Performance comparison guide
- Source code comments provide implementation details

## License

This is a prototype implementation for research and development purposes.