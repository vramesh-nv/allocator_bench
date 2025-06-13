#ifndef __UTILS_TYPES_H__
#define __UTILS_TYPES_H__

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Type definitions
typedef uint64_t NvU64;
typedef uint32_t NvU32;
typedef int NvBool;
#define NV_TRUE           ((NvBool)(0 == 0))
#define NV_FALSE          ((NvBool)(0 == 1))

#define MAX(x, y)     (((x)>(y))?(x):(y))

#define UNUSED(x) (void)(x)

// Export macro
#define CUDA_TEST_EXPORT

// Mock CU_ASSERT for testing
#define CU_ASSERT(expr) do { if (!(expr)) { printf("Assertion failed: %s\n", #expr); exit(1); } } while(0)

#endif