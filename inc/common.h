#ifndef COMMON_H
#define COMMON_H

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <stddef.h>

// Macro to get parent structure from member pointer
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

#define UNUSED(x) (void)(x)
#define PTR2UINT(v)((uintptr_t)(const void*)(v))
#define UINT2PTR(v)((void*)(uintptr_t)(v))

#define RESERVE_VA(size) mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0)
#define FREE_VA(addr, size) munmap(addr, size)

#endif // COMMON_H