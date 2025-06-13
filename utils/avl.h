#ifndef _AVL_TREE_H
#define _AVL_TREE_H

#include "utils_types.h"

enum CUavlTreeStatus_enum
{
    CU_AVL_TREE_STATUS_SUCCESS    = 0,
    CU_AVL_TREE_STATUS_KEY_EXISTS = 1,
};

typedef void  *CUavlTreeKey;
typedef void  *CUavlTreeValue;
typedef struct CUavlTree_st CUavlTree;
typedef struct CUavlTreeNode_st CUavlTreeNode;
typedef enum   CUavlTreeStatus_enum CUavlTreeStatus;
typedef void (*CUavlTreePrint)(CUavlTreeKey key);
typedef int  (*CUavlTreeCompare)(CUavlTreeKey a, CUavlTreeKey b);
typedef int  (*CUavlTreeNodeCompare)(CUavlTreeKey ka, CUavlTreeNode *nb);

struct CUavlTreeNode_st
{
    CUavlTreeNode *left;
    CUavlTreeNode *right;
    CUavlTreeKey   key;
    CUavlTreeValue value;
    CUavlTreeNode *parent;
    int            height;
};

struct CUavlTree_st
{
    CUavlTreePrint   print;
    CUavlTreeCompare compare;
    CUavlTreeNode   *root;
};

void            cuAvlTreeAssertValid(CUavlTree *tree);

void            cuAvlTreeInitialize(CUavlTree *tree, CUavlTreeCompare compare, CUavlTreePrint print);
void            cuAvlTreeDeinitialize(CUavlTree *tree);
CUavlTreeNode  *cuAvlTreeNodeFind(CUavlTree *tree, CUavlTreeKey key);
CUavlTreeNode  *cuAvlTreeNodeFindGEQ(CUavlTree *tree, CUavlTreeKey key);
CUavlTreeNode  *cuAvlTreeNodeFindLEQ(CUavlTree *tree, CUavlTreeKey key);
CUavlTreeStatus cuAvlTreeNodeInsert(CUavlTree *tree, CUavlTreeNode *node, CUavlTreeKey key, CUavlTreeValue value);
CUavlTreeNode  *cuAvlTreeNodeInsertOrReturnExisting(CUavlTree *tree, CUavlTreeNode *node, CUavlTreeKey key, CUavlTreeValue value);
void            cuAvlTreeNodeRemove(CUavlTree *tree, CUavlTreeNode *node);
CUavlTreeNode  *cuAvlTreeNodeInOrderPredecessor(CUavlTree *tree, CUavlTreeNode *node);
CUavlTreeNode  *cuAvlTreeNodeInOrderSuccessor(CUavlTree *tree, CUavlTreeNode *node);

static inline CUavlTreeNode *cuAvlTreeNodeGetChild(CUavlTree *tree, CUavlTreeNode *node, int compare)
{
    UNUSED(tree);
    CU_ASSERT(0 != compare);
    return (0 > compare) ? node->left : node->right;
}

#if CU_AVLTREE_DEBUG
static CUavlTreeNode *cuAvlTreeNodeFindWithNodeComparator(CUavlTree *tree, CUavlTreeKey key, CUavlTreeNodeCompare comparator)
{
    UNUSED(tree);
    CUavlTreeNode *node = tree->root;
    while (node) {
        int compare = comparator(key, node);
        if (0 == compare) {
            return node;
        }
        node = cuAvlTreeNodeGetChild(tree, node, compare);
    }
    return NULL;
}
#endif

static CUavlTreeNode *cuAvlTreeNodeFindWithComparator(CUavlTree *tree, CUavlTreeKey key, CUavlTreeCompare comparator)
{
    CUavlTreeNode *node = tree->root;
    while (node) {
        int compare = comparator(key, node->key);
        if (0 == compare) {
            return node;
        }
        node = cuAvlTreeNodeGetChild(tree, node, compare);
    }
    return NULL;
}

#endif // AVL_TREE_H