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

CUavlTreeNode *cuAvlTreeNodeFindWithNodeComparator(CUavlTree *tree, CUavlTreeKey key, CUavlTreeNodeCompare comparator);
CUavlTreeNode *cuAvlTreeNodeFindWithComparator(CUavlTree *tree, CUavlTreeKey key, CUavlTreeCompare comparator);

#endif // AVL_TREE_H