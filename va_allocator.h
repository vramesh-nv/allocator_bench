#ifndef VA_ALLOCATOR_H
#define VA_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque pointer type for the VA allocator
typedef struct va_allocator va_allocator_t;

// Function declarations
va_allocator_t* va_allocator_init(void);
void va_allocator_destroy(va_allocator_t *allocator);
uint64_t va_alloc(va_allocator_t *allocator, uint64_t size);
void va_free(va_allocator_t *allocator, uint64_t addr);

// Get total VA size
uint64_t va_allocator_get_total_size(va_allocator_t *allocator);

// Get used VA size
uint64_t va_allocator_get_used_size(va_allocator_t *allocator);

#ifdef __cplusplus
}
#endif

#endif // VA_ALLOCATOR_H