#ifndef VA_ALLOCATOR_DEFAULT_H
#define VA_ALLOCATOR_DEFAULT_H

#include "va_allocator_types.h"

// Forward declaration of ops getter and init for default allocator
va_allocator_ops_t *get_default_allocator_ops(void);
void *init_default_allocator(void);

#endif // VA_ALLOCATOR_DEFAULT_H 