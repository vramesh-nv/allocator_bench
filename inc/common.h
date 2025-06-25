#ifndef COMMON_H
#define COMMON_H

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

// These are required mostly for linter 
// Ensure MAP_ANONYMOUS is defined
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif

// Ensure MAP_NORESERVE is defined
#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0x4000
#endif

// Macro to get parent structure from member pointer
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define UNUSED(x) (void)(x)
#define PTR2UINT(v)((uintptr_t)(const void*)(v))
#define UINT2PTR(v)((void*)(uintptr_t)(v))

#define PHYSICAL_MEMORY_SIZE (1ULL << 31)  // 2GB physical memory

#define RESERVE_VA(size) mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0)
#define FREE_VA(addr, size) munmap(addr, size)

#endif // COMMON_H