#ifndef __CUILOOKUPTREE_H__
#define __CUILOOKUPTREE_H__

#include "common.h"
#include "avl.h"
#include "utils_types.h"

typedef struct CUIaddrTracker_st
{
    CUavlTree tree;
    NvU64 lo;
    NvU64 afterHi;
    //CUIrwlock lock;
} CUIaddrTracker;

typedef struct CUIaddrTrackerNode_st
{
    CUavlTreeNode node;
    CUIaddrTracker *tracker;

    NvU64 addr;
    NvU64 size;
    void *value;
} CUIaddrTrackerNode;

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize a heap for which valid addresses are [lo, afterHi) */
CUDA_TEST_EXPORT void
cuiAddrTrackerInit(CUIaddrTracker *tracker, NvU64 lo, NvU64 afterHi);

/* Deinitialize a heap */
CUDA_TEST_EXPORT void
cuiAddrTrackerDeinit(CUIaddrTracker *tracker);

/* Find the node which contains addr or NULL if it doesn't exit */
CUDA_TEST_EXPORT CUIaddrTrackerNode *
cuiAddrTrackerFindNode(CUIaddrTracker *tracker, NvU64 addr);

/* Find the node with the lowest address which is completely contained in [lo, afterHi) */
CUDA_TEST_EXPORT CUIaddrTrackerNode *
cuiAddrTrackerFindFirstNodeInRange(CUIaddrTracker *tracker, NvU64 lo, NvU64 afterHi);

/* True if there are no nodes in [lo, afterHi) or false otherwise */
CUDA_TEST_EXPORT NvBool
cuiAddrTrackerIsEmptyForRange(CUIaddrTracker *tracker, NvU64 lo, NvU64 afterHi);

/* Find the next node in address order or NULL if one doesn't exist */
CUDA_TEST_EXPORT CUIaddrTrackerNode *
cuiAddrTrackerNodeNext(CUIaddrTrackerNode *node);

/* Find the next node whose addr is less than startsBefore
   with an optional adjacency requirement */
CUDA_TEST_EXPORT CUIaddrTrackerNode *
cuiAddrTrackerNodeGetNextWithLimit(CUIaddrTrackerNode *node, NvU64 startsBefore, NvBool adjacentOnly);

/* Insert a node into the tracker */
CUDA_TEST_EXPORT void 
cuiAddrTrackerRegisterNode(CUIaddrTracker *tracker, CUIaddrTrackerNode *node, NvU64 addr, NvU64 size, void *value);

/* Insert a node into the tracker or, if one exists at that address already, return that node instead */
CUDA_TEST_EXPORT CUIaddrTrackerNode *
cuiAddrTrackerRegisterNodeOrReturnExisting(CUIaddrTracker *tracker, CUIaddrTrackerNode *node, NvU64 addr, NvU64 size, void *value);

/* Remove a node from the tracker */
CUDA_TEST_EXPORT void 
cuiAddrTrackerUnregisterNode(CUIaddrTrackerNode *node);

/* True if the tracker is intialized, false otherwise */
CUDA_TEST_EXPORT NvBool
cuiAddrTrackerIsInitialized(CUIaddrTracker *tracker);

#ifdef __cplusplus
}
#endif

#endif
