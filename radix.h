#ifndef __RADIX_H__
#define __RADIX_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Type definitions
typedef uint64_t NvU64;
typedef uint32_t NvU32;
typedef int NvBool;

// Export macro
#define CUDA_TEST_EXPORT

typedef struct CUradixNode_st CUradixNode;
typedef struct CUradixTree_st CUradixTree;

// Generic radix tree node type
struct CUradixNode_st
{
    struct CUradixNode_st *next;
    struct CUradixNode_st *prev;

    struct CUradixNode_st *child[2];
    struct CUradixNode_st **parent_to_self_ptr;
    struct CUradixNode_st *parent;

    NvU64 key; // Bits of this key will determine the location of a node
};

// Generic radix tree type
struct CUradixTree_st
{
    struct CUradixNode_st *root;
    unsigned int key_bits;
};

CUDA_TEST_EXPORT void
radixTreeInit(CUradixTree *tree, NvU32 key_bits);

CUDA_TEST_EXPORT void
radixTreeInsert(CUradixTree *tree, CUradixNode *node, NvU64 key);

CUDA_TEST_EXPORT void
radixTreeRemove(CUradixNode *node);

CUDA_TEST_EXPORT NvBool
radixTreeEmpty(CUradixTree *tree);

CUDA_TEST_EXPORT CUradixNode *
radixTreeFindGEQ(CUradixTree *tree, NvU64 key);

#ifdef __cplusplus
}
#endif

#endif
