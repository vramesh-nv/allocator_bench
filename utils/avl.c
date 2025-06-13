#include "avl.h"

#define CU_AVLTREE_DEBUG 0

static inline int             cuAvlTreeNodeGetHeight(CUavlTree *tree, CUavlTreeNode *node);
static inline int             cuAvlTreeNodeGetBalance(CUavlTree *tree, CUavlTreeNode *node);
static inline void            cuAvlTreeNodeRecalculateHeight(CUavlTree *tree, CUavlTreeNode *node);
static inline CUavlTreeNode **cuAvlTreeNodeGetLinkPointer(CUavlTree *tree, CUavlTreeNode *node);
static inline CUavlTreeNode **cuAvlTreeNodeGetChildLinkPointer(CUavlTree *tree, CUavlTreeNode *node, int compare);
static           CUavlTreeNode  *cuAvlTreeNodeRotateRight(CUavlTree *tree, CUavlTreeNode *node);
static           CUavlTreeNode  *cuAvlTreeNodeRotateLeft(CUavlTree *tree, CUavlTreeNode *node);
static           void            cuAvlTreeNodeRebalance(CUavlTree *tree, CUavlTreeNode *node, int isDelete);

/* Debug functions */
#if CU_AVLTREE_DEBUG
static void cuAvlTreeNodePrintRecursive(CUavlTree *tree, CUavlTreeNode *node, int depth)
{
    UNUSED(tree);
    int i, height, balance;
    if (node) {
        cuAvlTreeNodePrintRecursive(tree, node->left, depth + 1);
        for (i = 0; i < depth; i++) {
            printf(" ");
        }
        height = cuAvlTreeNodeGetHeight(tree, node);
        balance = cuAvlTreeNodeGetBalance(tree, node);
        tree->print(node->key);
        printf(" (%d, %d)\n", height, balance);
        cuAvlTreeNodePrintRecursive(tree, node->right, depth + 1);
    }
}
#endif

static int cuAvlTreeAssertValidRecursive(CUavlTree *tree, CUavlTreeNode *node, int mark)
{
    int height;
    int lefth = 0;
    int righth = 0;
    if (node) {
        int balance = cuAvlTreeNodeGetBalance(tree, node);
        CU_ASSERT(-2 < balance && 2 > balance);
        if (node->left) {
            int compare = tree->compare(node->left->key, node->key);
            CU_ASSERT(node->left == cuAvlTreeNodeGetChild(tree, node, compare));
            CU_ASSERT(node == node->left->parent);
            lefth = cuAvlTreeAssertValidRecursive(tree, node->left, mark);
        }
        if (node->right) {
            int compare = tree->compare(node->right->key, node->key);
            CU_ASSERT(node->right == cuAvlTreeNodeGetChild(tree, node, compare));
            CU_ASSERT(node == node->right->parent);
            righth = cuAvlTreeAssertValidRecursive(tree, node->right, mark);
        }
    }
    height = MAX(lefth, righth) + 1;
    CU_ASSERT(!node || height == node->height);
    return height;
}

void cuAvlTreeAssertValid(CUavlTree *tree)
{
    cuAvlTreeAssertValidRecursive(tree, tree->root, 0);
}
/* end Debug functions */

static inline int cuAvlTreeNodeGetHeight(CUavlTree *tree, CUavlTreeNode *node)
{
    UNUSED(tree);
    if (node) {
        return node->height;
    }
    return 0;
}

static inline int cuAvlTreeNodeGetBalance(CUavlTree *tree, CUavlTreeNode *node)
{
    int balance = 0;
    if (node) {
        balance = cuAvlTreeNodeGetHeight(tree, node->left) - cuAvlTreeNodeGetHeight(tree, node->right);
    }
    CU_ASSERT(2 >= balance);
    return balance;
}

static inline void cuAvlTreeNodeRecalculateHeight(CUavlTree *tree, CUavlTreeNode *node)
{
    node->height = 1 + MAX(cuAvlTreeNodeGetHeight(tree, node->left), cuAvlTreeNodeGetHeight(tree, node->right));
}

static inline CUavlTreeNode **cuAvlTreeNodeGetLinkPointer(CUavlTree *tree, CUavlTreeNode *node)
{
    CUavlTreeNode *parent = node->parent;
    if (parent) {
        if (node == parent->left) {
            return &parent->left;
        }
        if (node == parent->right) {
            return &parent->right;
        }
        CU_ASSERT(0);
    }
    return &tree->root;
}

static inline CUavlTreeNode **cuAvlTreeNodeGetChildLinkPointer(CUavlTree *tree, CUavlTreeNode *node, int compare)
{
    UNUSED(tree);
    CU_ASSERT(0 != compare);
    return (0 > compare) ? &node->left : &node->right;
}

static CUavlTreeNode *cuAvlTreeNodeRotateRight(CUavlTree *tree, CUavlTreeNode *node)
{
    CUavlTreeNode *child = node->left;
    CUavlTreeNode **link = cuAvlTreeNodeGetLinkPointer(tree, node);

    *link = child;
    child->parent = node->parent;

    node->left = child->right;
    if (node->left) {
        node->left->parent = node;
    }

    child->right = node;
    node->parent = child;

    cuAvlTreeNodeRecalculateHeight(tree, node);
    cuAvlTreeNodeRecalculateHeight(tree, child);

    return child;
}

static CUavlTreeNode *cuAvlTreeNodeRotateLeft(CUavlTree *tree, CUavlTreeNode *node)
{
    CUavlTreeNode *child = node->right;
    CUavlTreeNode **link = cuAvlTreeNodeGetLinkPointer(tree, node);

    *link = child;
    child->parent = node->parent;

    node->right = child->left;
    if (node->right) {
        node->right->parent = node;
    }

    child->left = node;
    node->parent = child;

    cuAvlTreeNodeRecalculateHeight(tree, node);
    cuAvlTreeNodeRecalculateHeight(tree, child);

    return child;
}

static void cuAvlTreeNodeRebalance(CUavlTree *tree, CUavlTreeNode *node, int isDelete)
{
    while (node) {
        int balance = cuAvlTreeNodeGetBalance(tree, node);
        // Right subtree is too tall
        if (-2 == balance) {
            int rightBalance = cuAvlTreeNodeGetBalance(tree, node->right);
            if (-1 == rightBalance) {
                node = cuAvlTreeNodeRotateLeft(tree, node);
            }
            else if (0 == rightBalance && isDelete) {
                node = cuAvlTreeNodeRotateLeft(tree, node);
            }
            else if (1 == rightBalance) {
                cuAvlTreeNodeRotateRight(tree, node->right);
                node = cuAvlTreeNodeRotateLeft(tree, node);
            }
        }
        // Left subtree is too tall
        else if (2 == balance) {
            int leftBalance = cuAvlTreeNodeGetBalance(tree, node->left);
            if (1 == leftBalance) {
                node = cuAvlTreeNodeRotateRight(tree, node);
            }
            else if (0 == leftBalance && isDelete) {
                node = cuAvlTreeNodeRotateRight(tree, node);
            }
            else if (-1 == leftBalance) {
                cuAvlTreeNodeRotateLeft(tree, node->left);
                node = cuAvlTreeNodeRotateRight(tree, node);
            }
        }
        cuAvlTreeNodeRecalculateHeight(tree, node);
        node = node->parent;
    }
}

void cuAvlTreeInitialize(CUavlTree *tree, CUavlTreeCompare compare, CUavlTreePrint print)
{
    memset(tree, 0, sizeof(*tree));
    tree->print = print;
    tree->compare = compare;
}

void cuAvlTreeDeinitialize(CUavlTree *tree)
{
    memset(tree, 0, sizeof(*tree));
}

CUavlTreeNode *cuAvlTreeNodeFind(CUavlTree *tree, CUavlTreeKey key)
{
    return cuAvlTreeNodeFindWithComparator(tree, key, tree->compare);
}

CUavlTreeNode *cuAvlTreeNodeFindGEQ(CUavlTree *tree, CUavlTreeKey key)
{
    CUavlTreeNode *node = tree->root;
    CUavlTreeNode *best = NULL;
    while (node) {
        int compare = tree->compare(key, node->key);
        if (0 >= compare) {
            best = node;
        }
        if (0 == compare) {
            return node;
        }
        node = cuAvlTreeNodeGetChild(tree, node, compare);
    }
    return best;
}

CUavlTreeNode *cuAvlTreeNodeFindLEQ(CUavlTree *tree, CUavlTreeKey key)
{
    CUavlTreeNode *node = tree->root;
    CUavlTreeNode *best = NULL;
    while (node) {
        int compare = tree->compare(key, node->key);
        if (0 <= compare) {
            best = node;
        }
        if (0 == compare) {
            return node;
        }
        node = cuAvlTreeNodeGetChild(tree, node, compare);
    }
    return best;
}

CUavlTreeNode *cuAvlTreeNodeInsertOrReturnExisting(CUavlTree *tree, CUavlTreeNode *node, CUavlTreeKey key, CUavlTreeValue value)
{
    CUavlTreeNode *parent = tree->root;
    CUavlTreeNode **childLink = &tree->root;

    memset(node, 0, sizeof(*node));
    node->key = key;
    node->value = value;
    node->height = 1;

    // Find the node to insert below
    while (parent) {
        int compare = tree->compare(key, parent->key);
        if (0 == compare) {
            return parent;
        }
        childLink = cuAvlTreeNodeGetChildLinkPointer(tree, parent, compare);
        if (NULL == *childLink) {
            break;
        }
        parent = *childLink;
    }

    // Link up the node
    *childLink = node;
    node->parent = parent;
    cuAvlTreeNodeRebalance(tree, parent, 0);

#if CU_AVLTREE_DEBUG
        cuAvlTreeAssertValid(tree);
#endif
    return NULL;
}

CUavlTreeStatus cuAvlTreeNodeInsert(CUavlTree *tree, CUavlTreeNode *node, CUavlTreeKey key, CUavlTreeValue value)
{
    if (cuAvlTreeNodeInsertOrReturnExisting(tree, node, key, value) != NULL) {
        return CU_AVL_TREE_STATUS_KEY_EXISTS;
    }
    return CU_AVL_TREE_STATUS_SUCCESS;
}

void cuAvlTreeNodeRemove(CUavlTree *tree, CUavlTreeNode *node)
{
    CUavlTreeNode *rebalance = node->parent;
    CUavlTreeNode **link = cuAvlTreeNodeGetLinkPointer(tree, node);
    // Interior node with two children
    if (node->left && node->right) {
        CUavlTreeNode *successor = node->right;
        CUavlTreeNode **successorLink = NULL;

        // Find the left-most node in the right subtree
        while (successor->left) {
            successor = successor->left;
        }
        // Temporarily remove that node
        successorLink = cuAvlTreeNodeGetLinkPointer(tree, successor);
        *successorLink = successor->right;
        if (successor->right) {
            successor->right->parent = successor->parent;
        }
        // We need to rebalance starting directly above successor.
        // However, don't do this if successor is a child of node
        if (node != successor->parent) {
            rebalance = successor->parent;
        }
        else {
            rebalance = successor;
        }
        // Replace node with successor
        *link = successor;
        successor->parent = node->parent;
        successor->left = node->left;
        successor->left->parent = successor;
        successor->right = node->right;
        // If successor is a child of parent, successor->right will be NULL
        if (successor->right) {
            successor->right->parent = successor;
        }
    }
    // Interior node with one child
    else if (node->left) {
        *link = node->left;
        node->left->parent = node->parent;
        rebalance = node->left;
    }
    // Interior node with one child
    else if (node->right) {
        *link = node->right;
        node->right->parent = node->parent;
        rebalance = node->right;
    }
    // Leaf node
    else {
        *link = NULL;
    }

    // We don't want to clobber the key/value, however we should zero out our pointers
    node->parent = NULL;
    node->left = NULL;
    node->right = NULL;
    cuAvlTreeNodeRebalance(tree, rebalance, 1);

#if CU_AVLTREE_DEBUG
        cuAvlTreeAssertValid(tree);
#endif
}

CUavlTreeNode *cuAvlTreeNodeInOrderPredecessor(CUavlTree *tree, CUavlTreeNode *node)
{
    UNUSED(tree);
    // Left-most node in left subtree
    if (node->left) {
        CUavlTreeNode *predecessor = node->left;
        while (predecessor->right) {
            predecessor = predecessor->right;
        }
        return predecessor;
    }
    // Otherwise, first parent with this node in its right subtree
    else if (node->parent) {
        CUavlTreeNode *predecessor = node;
        while (predecessor) {
            if (predecessor->parent) {
                if (predecessor->parent->right == predecessor) {
                    return predecessor->parent;
                }
            }
            predecessor = predecessor->parent;
        }
    }
    return NULL;
}

CUavlTreeNode *cuAvlTreeNodeInOrderSuccessor(CUavlTree *tree, CUavlTreeNode *node)
{
    UNUSED(tree);
    // Left-most node in right subtree
    if (node->right) {
        CUavlTreeNode *successor = node->right;
        while (successor->left) {
            successor = successor->left;
        }
        return successor;
    }
    // Otherwise, first parent with this node in its left subtree
    else if (node->parent) {
        CUavlTreeNode *successor = node;
        while (successor) {
            if (successor->parent) {
                if (successor->parent->left == successor) {
                    return successor->parent;
                }
            }
            successor = successor->parent;
        }
    }
    return NULL;
}