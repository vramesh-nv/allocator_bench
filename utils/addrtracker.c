#include "addrtracker.h"

static int cuiAddrTrackerCompare(CUavlTreeKey a, CUavlTreeKey b)
{
    NvS64 *addrA = (NvS64 *)a;
    NvS64 *addrB = (NvS64 *)b;

    NvS64 diff = *addrA - *addrB;
    if (diff < 0) {
        return -1;
    }
    else if (diff > 0) {
        return 1;
    }
    return 0;
}

static int cuiAddrTrackerNodeCompare(CUavlTreeKey a, CUavlTreeNode *nb)
{
    NvU64 addr = *(NvU64 *)a;
    CUIaddrTrackerNode *node = container_of(nb, CUIaddrTrackerNode, node);

    if (addr >= node->addr && addr < node->addr + node->size) {
        return 0;
    }
    return addr < node->addr ? -1 : 1;
}

void cuiAddrTrackerInit(CUIaddrTracker *tracker, NvU64 lo, NvU64 afterHi)
{
    CU_ASSERT(lo < afterHi);
    memset(tracker, '\0', sizeof(*tracker));
    cuAvlTreeInitialize(&tracker->tree, cuiAddrTrackerCompare, NULL);
    //cuiRWLockInitialize(&tracker->lock, CUI_MUTEX_ORDER_TERMINAL, CUI_MUTEX_DEFAULT);

    tracker->lo = lo;
    tracker->afterHi = afterHi;
}

void cuiAddrTrackerDeinit(CUIaddrTracker *tracker)
{
    //cuiRWLockDeinitialize(&tracker->lock);
    cuAvlTreeDeinitialize(&tracker->tree);
    memset(tracker, '\0', sizeof(*tracker));
}

static CUIaddrTrackerNode *toTracker(CUavlTreeNode *avlNode)
{
    if (avlNode == NULL) {
        return NULL;
    }

    return container_of(avlNode, CUIaddrTrackerNode, node);
}

CUIaddrTrackerNode *cuiAddrTrackerFindNode(CUIaddrTracker *tracker, NvU64 addr)
{
    //cuiRWLockReadLock(&tracker->lock);
    CUIaddrTrackerNode *node = toTracker(cuAvlTreeNodeFindWithNodeComparator(&tracker->tree,
                                                                             &addr,
                                                                             cuiAddrTrackerNodeCompare));
    //cuiRWLockReadUnlock(&tracker->lock);
    return node;
}

CUIaddrTrackerNode *cuiAddrTrackerFindFirstNodeInRange(CUIaddrTracker *tracker, NvU64 lo, NvU64 afterHi)
{
    CU_ASSERT(lo < afterHi);
    //cuiRWLockReadLock(&tracker->lock);
    CUIaddrTrackerNode *node = toTracker(cuAvlTreeNodeFindGEQ(&tracker->tree, &lo));

    if (node == NULL || (node->addr + node->size > afterHi)) {
        node = NULL;
    }

    //cuiRWLockReadUnlock(&tracker->lock);
    return node;
}

NvBool cuiAddrTrackerIsEmptyForRange(CUIaddrTracker *tracker, NvU64 lo, NvU64 afterHi)
{
    CU_ASSERT(lo < afterHi);
    NvU64 hi = afterHi - 1;
    //cuiRWLockReadLock(&tracker->lock);

    CUIaddrTrackerNode *node = toTracker(cuAvlTreeNodeFindLEQ(&tracker->tree, &hi));
    NvBool isEmpty = node == NULL || (node->addr + node->size <= lo);

    //cuiRWLockReadUnlock(&tracker->lock);
    return isEmpty;
}

CUIaddrTrackerNode *cuiAddrTrackerNodeNext(CUIaddrTrackerNode *node)
{
    CUIaddrTracker *tracker = node->tracker;
    //cuiRWLockReadLock(&tracker->lock);

    CUIaddrTrackerNode *next = toTracker(cuAvlTreeNodeInOrderSuccessor(&tracker->tree, &node->node));

    //cuiRWLockReadUnlock(&tracker->lock);
    return next;
}

CUIaddrTrackerNode *cuiAddrTrackerNodeGetNextWithLimit(CUIaddrTrackerNode *node, NvU64 startsBefore, NvBool adjacentOnly)
{
    CUIaddrTracker *tracker = node->tracker;
    //cuiRWLockReadLock(&tracker->lock);
   
    CUIaddrTrackerNode *next = toTracker(cuAvlTreeNodeInOrderSuccessor(&tracker->tree, &node->node));
    if (next == NULL ||
        next->addr >= startsBefore ||
        (adjacentOnly && next->addr != node->addr + node->size))
    {
        next = NULL;
    }

    //cuiRWLockReadUnlock(&tracker->lock);
    return next;
}

CUIaddrTrackerNode *cuiAddrTrackerRegisterNodeOrReturnExisting(CUIaddrTracker *tracker,
                                                               CUIaddrTrackerNode *node,
                                                               NvU64 addr,
                                                               NvU64 size,
                                                               void *value)
{
    CU_ASSERT(addr >= tracker->lo && addr + size <= tracker->afterHi);

    /*if (cuosBuildIsDebug() && !cuiAddrTrackerIsEmptyForRange(tracker, addr, addr + size)) {
        CUIaddrTrackerNode *debugNode = cuiAddrTrackerFindFirstNodeInRange(tracker, addr, addr + size);
        CU_ASSERT(debugNode != NULL && debugNode->addr == addr && debugNode->size == size);
    }*/

    node->value = value;
    node->size = size;
    node->addr = addr;
    node->tracker = tracker;

    //cuiRWLockWriteLock(&tracker->lock);
    CUIaddrTrackerNode *existing = toTracker(cuAvlTreeNodeInsertOrReturnExisting(&tracker->tree,
                                                                                 &node->node,
                                                                                 &node->addr,
                                                                                 NULL));
    //cuiRWLockWriteUnlock(&tracker->lock);

    if (existing != NULL) {
        memset(node, '\0', sizeof(*node));
    }

    return existing;
}

void cuiAddrTrackerRegisterNode(CUIaddrTracker *tracker, CUIaddrTrackerNode *node, NvU64 addr, NvU64 size, void *value)
{
    CUIaddrTrackerNode *existing = cuiAddrTrackerRegisterNodeOrReturnExisting(tracker, node, addr, size, value);
    CU_ASSERT(existing == NULL);
}

void cuiAddrTrackerUnregisterNode(CUIaddrTrackerNode *node)
{
    //cuiRWLockWriteLock(&node->tracker->lock);
    cuAvlTreeNodeRemove(&node->tracker->tree, &node->node);
    //cuiRWLockWriteUnlock(&node->tracker->lock);
}

NvBool cuiAddrTrackerIsInitialized(CUIaddrTracker *tracker)
{
    return tracker->afterHi != 0;
}