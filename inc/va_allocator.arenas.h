#ifndef VA_ALLOCATOR_ARENAS_H
#define VA_ALLOCATOR_ARENAS_H

#include "va_allocator_types.h"

// Forward declaration of ops getter for arena allocator
va_allocator_ops_t* get_arena_allocator_ops(void);
void *init_arena_allocator(void);

#endif // VA_ALLOCATOR_ARENAS_H 