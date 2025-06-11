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

#endif // COMMON_H