#ifndef VA_ALLOCATOR_DEFAULT_H
#define VA_ALLOCATOR_DEFAULT_H

#include "va_allocator_types.h"

#define PHYSICAL_MEMORY_SIZE (1ULL << 31)  // 2GB physical memory

// Forward declaration of ops getter and init for default allocator
va_allocator_ops_t *get_default_allocator_ops(void);
void *init_default_allocator(void);

void print_default_blocks(void* impl);

#endif // VA_ALLOCATOR_DEFAULT_H 