#ifndef __UTILS_TYPES_H__
#define __UTILS_TYPES_H__

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Type definitions
typedef unsigned char      NvU8;  /**< 0 to 255 */
typedef unsigned short     NvU16; /**< 0 to 65535 */
typedef unsigned int       NvU32; /**< 0 to 4294967295 */
typedef unsigned long long NvU64; /**< 0 to 18446744073709551615 */
typedef signed char        NvS8;  /**< -128 to 127 */
typedef signed short       NvS16; /**< -32768 to 32767 */
typedef signed int         NvS32; /**< -2147483648 to 2147483647 */
typedef signed long long   NvS64; /**< 2^-63 to 2^63-1 */

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