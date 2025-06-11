#include "radix.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Mock CU_ASSERT for testing
#define CU_ASSERT(expr) do { if (!(expr)) { printf("Assertion failed: %s\n", #expr); exit(1); } } while(0)

static inline CUradixNode * 
radixTreeGetSmallerChild(CUradixNode *node)
{
    return (node->child[0]? node->child[0] : node->child[1]);
}

static inline CUradixNode * 
radixTreeGetSmallerNode(CUradixNode *firstNode, CUradixNode *potentialSecondNode)
{
    return ((potentialSecondNode == NULL || firstNode->key < potentialSecondNode->key)? firstNode : potentialSecondNode);
}

static inline NvU32
radixTreeIsBitSet(NvU64 key, NvU32 key_bit)
{
    return ((key & (1ULL << key_bit)) != 0);
}

static void
radixListInit(CUradixNode *node)
{
    CU_ASSERT(node);

    node->next = node;
    node->prev = node;
}

static void
radixListInsert(CUradixNode *node, CUradixNode *head)
{
    CU_ASSERT(node);
    CU_ASSERT(head);

    CUradixNode *prev = head->prev;
    prev->next = node;
    node->prev = prev;
    head->prev = node;
    node->next = head;
}

static void
radixListRemove(CUradixNode *node)
{
    CU_ASSERT(node);

    CUradixNode *next = node->next;
    CUradixNode *prev = node->prev;

    next->prev = prev;
    prev->next = next;
}

static int
radixListEmpty(CUradixNode *node)
{
    CU_ASSERT(node);
    return node->next == node;
}

void
radixTreeInit(CUradixTree *tree, NvU32 key_bits)
{
    CU_ASSERT(tree);
    CU_ASSERT(key_bits > 0);

    memset(tree, 0, sizeof(*tree));
    tree->key_bits = key_bits;
}

NvBool
radixTreeEmpty(CUradixTree *tree)
{
    CU_ASSERT(tree);
    return (tree->root == NULL);
}

// This should be used only when repl is not currently connected to the tree
// using parent/child[]/*parent_to_self_ptr links. That is, only a detached
// node or a non-primary node in the linked-list of a node (not the first one)
// can be specified as the repl.
static void
radixTreeReplaceNode(CUradixNode *orig, CUradixNode *repl)
{
    int i;
    CU_ASSERT(repl->child[0] == NULL);
    CU_ASSERT(repl->child[1] == NULL);
    CU_ASSERT(repl->parent == NULL);
    CU_ASSERT(repl->parent_to_self_ptr == NULL);

    repl->parent_to_self_ptr = orig->parent_to_self_ptr;
    repl->parent = orig->parent;

    for (i = 0; i < 2; i++) {
        repl->child[i] = orig->child[i];
        if (repl->child[i]) {
            repl->child[i]->parent_to_self_ptr = &repl->child[i];
            repl->child[i]->parent = repl;
        }
    }

    *(repl->parent_to_self_ptr) = repl;
}

// There are (15) pointers that need to be changed to swap a parent with its specified child
static void
radixTreeSwapParentWithChild(CUradixNode *oldParent, CUradixNode *oldChild)
{
    CU_ASSERT(oldChild->parent == oldParent);

    unsigned i;
    unsigned childNumber = (oldParent->child[1] == oldChild);
    unsigned otherChildNumber = 1 - childNumber;
    CUradixNode *swap_parent;
    CUradixNode **swap_parent_to_self_ptr = oldParent->parent_to_self_ptr;
    CUradixNode *oldChild_child[2];
    for (i = 0; i < 2; i++) {
        oldChild_child[i] = oldChild->child[i];
    }

    // Set parent_to_self_ptr of oldParent (2)
    oldParent->parent_to_self_ptr = oldChild->child + childNumber;
    *(oldParent->parent_to_self_ptr) = oldParent;

    // Set the other child pointer of oldChild (3)
    oldChild->child[otherChildNumber] = oldParent->child[otherChildNumber];
    if (oldChild->child[otherChildNumber]) {
        oldChild->child[otherChildNumber]->parent = oldChild;
        oldChild->child[otherChildNumber]->parent_to_self_ptr = oldChild->child + otherChildNumber;
    }

    // Set parent_to_self_ptr of oldChild (2)
    oldChild->parent_to_self_ptr = swap_parent_to_self_ptr;
    *(oldChild->parent_to_self_ptr) = oldChild;

    // Swap parent pointers (2)
    swap_parent = oldParent->parent;
    oldParent->parent = oldChild;
    oldChild->parent = swap_parent;

    // Set child pointers of oldParent (6)
    for (i = 0; i < 2; i++) {
        oldParent->child[i] = oldChild_child[i];
        if (oldParent->child[i]) {
            oldParent->child[i]->parent_to_self_ptr = &oldParent->child[i];
            oldParent->child[i]->parent = oldParent;
        }
    }
}

// The tree is maintained so that at each time, the key of a parent node is
// smaller than the key of its children. This allows us to make sure if we find
// a node with a greater key on our path traversing the tree, it is the best
// fit.
//
// If we don't keep this property, when a traverse to the left subtree of a
// parent doesn't find a GEQ node, we pick the parent as the answer, which
// could be larger than some of its right subtree nodes that are better answers.
// Keep in mind that in a radix tree, every node in a subtree is qualified
// to be the parent node (has the correct bits for that location).
void
radixTreeInsert(CUradixTree *tree, CUradixNode *node, NvU64 key)
{
    CU_ASSERT(tree);
    CU_ASSERT(tree->key_bits == 64 || ((~((1ULL << tree->key_bits) - 1)) & key) == 0);
    
    CUradixNode *parent = NULL;
    CUradixNode **parent_to_self_ptr = &tree->root;
    NvU32 cur_key_bit = tree->key_bits;
    NvU32 child_to_take = 0;

    memset(node, 0, sizeof(*node));
    node->key = key;
    radixListInit(node);

    while (*parent_to_self_ptr && (*parent_to_self_ptr)->key != key) {
        CUradixNode *cur = *parent_to_self_ptr;

        // If the node has a smaller key than the cur node, swap
        // node<-->cur and try to insert node (now changed) again
        if (node->key < cur->key) {
            CUradixNode *swapNode;

            radixTreeReplaceNode(cur, node);

            swapNode = cur;
            cur = node;
            node = swapNode;

            node->child[0] = NULL;
            node->child[1] = NULL;
            node->parent = NULL;
            node->parent_to_self_ptr = NULL;
        }

        parent = cur;
        CU_ASSERT(cur_key_bit > 0);
        cur_key_bit--;
        child_to_take = radixTreeIsBitSet(node->key, cur_key_bit);
        parent_to_self_ptr = &cur->child[child_to_take];
    }

    if (*parent_to_self_ptr) {
        radixListInsert(node, *parent_to_self_ptr);
    }
    else {
        node->parent_to_self_ptr = parent_to_self_ptr;
        *(node->parent_to_self_ptr) = node;
        node->parent = parent; 
    }
}

CUradixNode *
radixTreeFindGEQ(CUradixTree *tree, NvU64 key)
{
    CUradixNode *node = tree->root;
    CUradixNode *found = NULL;
    CUradixNode *gt_tree = NULL;
    unsigned int cur_key_bit = tree->key_bits;
    unsigned int child_to_take = 0;

    // We use key's bit representation to traverse the tree to
    // get to the GEQ node
    while (node) {
        if (node->key == key) {
            return node;
        }

        if (node->key > key) {
            found = radixTreeGetSmallerNode(node, found);
        }

        cur_key_bit--;
        child_to_take = radixTreeIsBitSet(key, cur_key_bit);

        // Record the right-subtree only if it exists but we're going left
        if (child_to_take == 0 && node->child[1]) {
            gt_tree = node->child[1];
        }
        node = node->child[child_to_take];
    }
    
    // If we didn't find anything in our traversal, the latest
    // gt_tree node (which is the latest child[1] to which we
    // didn't go) will have the GEQ.
    if (!found) {
        found = gt_tree;
    }

    return found;
}

void
radixTreeRemove(CUradixNode *node)
{
    CU_ASSERT(node);

    if (radixListEmpty(node)) {
        CUradixNode *leastChild = radixTreeGetSmallerChild(node);
        // 1. if it is a leaf, simply cut it out
        if (leastChild == NULL) {
            *(node->parent_to_self_ptr) = NULL;
        }
        // 2. if not, swap it with its smallest child and then 
        // try to remove it again (i.e., push it down until it's a leaf).
        // Note that remove cannot simply cut out a node from the middle of
        // the tree, because it will impact the height of other nodes.
        else {
            radixTreeSwapParentWithChild(node, leastChild);
            radixTreeRemove(node);
        }
    }
    else {
        // If this is the first element of the node (i.e., primary element),
        // another element in the linked-list should step up and become
        // primary.
        if (node->parent_to_self_ptr) {
            radixTreeReplaceNode(node, node->next);
            node->parent_to_self_ptr = NULL;
        }
        radixListRemove(node);
    }
}