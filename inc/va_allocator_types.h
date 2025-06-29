#ifndef VA_ALLOCATOR_TYPES_H
#define VA_ALLOCATOR_TYPES_H

#include <stdint.h>

// Allocator implementation types
typedef enum {
    VA_ALLOCATOR_TYPE_DEFAULT,  // Current implementation
    VA_ALLOCATOR_TYPE_ARENA,    // Arena allocator implementation
    VA_ALLOCATOR_TYPE_MAX
    // Add more types as needed
} va_allocator_type_t;

// Function pointer types for allocator operations
typedef uint64_t (*va_alloc_fn)(void* impl, uint64_t size);
typedef void (*va_free_fn)(void* impl, uint64_t addr);
typedef uint64_t (*va_get_total_size_fn)(void* impl);
typedef uint64_t (*va_get_used_size_fn)(void* impl);
typedef void (*va_print_fn)(void* impl);
typedef void (*va_destroy_fn)(void* impl);
typedef void (*va_flush_fn)(void* impl);
typedef uint64_t (*va_get_physical_mem_usage_fn)(void* impl);

// Structure containing function pointers for allocator operations
typedef struct {
    va_alloc_fn alloc;
    va_free_fn free;
    va_get_total_size_fn get_total_size;
    va_get_used_size_fn get_used_size;
    va_print_fn print;
    va_destroy_fn destroy;
    va_flush_fn flush;
    va_get_physical_mem_usage_fn get_physical_mem_usage;
    void* impl;  // Implementation-specific data
} va_allocator_ops_t;

#endif // VA_ALLOCATOR_TYPES_H 