#define _GNU_SOURCE
#include "va_allocator.h"
#include <assert.h>
#include "radix.h"
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>

// Define container_of macro
#define container_of(ptr, type, member) ({ \
    const typeof(((type *)0)->member) *__mptr = (ptr); \
    (type *)((char *)__mptr - offsetof(type, member)); \
})

// Define physical memory size (for now hardcoded)
#define PHYSICAL_MEMORY_SIZE (2ULL * 1024 * 1024 * 1024)  // 2GB
#define VA_SIZE (2 * PHYSICAL_MEMORY_SIZE)  // Twice the physical memory

// Structure to represent a memory block
typedef struct va_block {
    uint64_t start_addr;    // Starting address of the block
    uint64_t size;          // Size of the block
    int is_free;            // Whether the block is free
    struct va_block *addr_next;  // Next block in address order
    struct va_block *addr_prev;  // Previous block in address order
    CUradixNode radix_node;      // Node in the radix tree for size-based tracking
} va_block_t;

// Structure to represent the VA allocator
struct va_allocator {
    va_block_t *addr_list;      // List ordered by address
    CUradixTree size_tree;      // Tree ordered by size
    uint64_t total_va_size;     // Total VA space size
    uint64_t used_va_size;      // Currently used VA space
};

// Helper function to insert block into address-ordered list
static void insert_addr_list(va_allocator_t *allocator, va_block_t *block) {
    va_block_t *current = allocator->addr_list;
    va_block_t *prev = NULL;

    // Find the correct position to insert
    while (current && current->start_addr < block->start_addr) {
        prev = current;
        current = current->addr_next;
    }

    // Insert the block
    block->addr_next = current;
    block->addr_prev = prev;
    if (prev) {
        prev->addr_next = block;
    } else {
        allocator->addr_list = block;
    }
    if (current) {
        current->addr_prev = block;
    }
}

// Helper function to remove block from address list
static void remove_addr_list(va_allocator_t *allocator, va_block_t *block) {
    if (block->addr_prev) {
        block->addr_prev->addr_next = block->addr_next;
    } else {
        allocator->addr_list = block->addr_next;
    }
    if (block->addr_next) {
        block->addr_next->addr_prev = block->addr_prev;
    }
}

// Initialize the VA allocator
va_allocator_t* va_allocator_init(void) {
    va_allocator_t *allocator = malloc(sizeof(struct va_allocator));
    if (!allocator) return NULL;

    // Initialize the allocator
    allocator->total_va_size = VA_SIZE;
    allocator->used_va_size = 0;
    allocator->addr_list = NULL;
    radixTreeInit(&allocator->size_tree, 64);  // Use 64 bits for size keys

    // Reserve VA space using mmap
    void *va_base = mmap(NULL, VA_SIZE, PROT_NONE, 
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (va_base == MAP_FAILED) {
        free(allocator);
        return NULL;
    }

    // Create initial free block
    va_block_t *initial_block = malloc(sizeof(va_block_t));
    if (!initial_block) {
        munmap(va_base, VA_SIZE);
        free(allocator);
        return NULL;
    }

    initial_block->start_addr = (uint64_t)va_base;
    initial_block->size = VA_SIZE;
    initial_block->is_free = 1;
    initial_block->addr_next = NULL;
    initial_block->addr_prev = NULL;
    memset(&initial_block->radix_node, 0, sizeof(CUradixNode));

    // Insert into both lists
    allocator->addr_list = initial_block;
    radixTreeInsert(&allocator->size_tree, &initial_block->radix_node, VA_SIZE);

    return allocator;
}

// Destroy the VA allocator
void va_allocator_destroy(va_allocator_t *allocator) {
    if (!allocator) return;

    // Free all blocks
    va_block_t *current = allocator->addr_list;
    while (current) {
        va_block_t *next = current->addr_next;
        free(current);
        current = next;
    }

    // Unmap the VA space
    if (allocator->addr_list) {
        munmap((void *)allocator->addr_list->start_addr, VA_SIZE);
    }

    free(allocator);
}

void print_all_blocks(va_allocator_t *allocator) {
    va_block_t *current = allocator->addr_list;
    while (current) {
        printf("Block: start_addr: %lu, size: %lu, is_free: %d\n", current->start_addr, current->size, current->is_free);
        current = current->addr_next;
    }

    printf("Radix tree is empty: %d\n", radixTreeEmpty(&allocator->size_tree));
}

// Allocate VA space
uint64_t va_alloc(va_allocator_t *allocator, uint64_t size) {
    if (!allocator || size == 0 || size > VA_SIZE) return 0;

    // Find best fit block using radix tree
    CUradixNode *node = radixTreeFindGEQ(&allocator->size_tree, size);
    if (!node) {
        return 0;  // No suitable block found
    }

    va_block_t *best_fit = container_of(node, va_block_t, radix_node);

    // Split block if necessary
    if (best_fit->size > size) {
        va_block_t *new_block = malloc(sizeof(va_block_t));
        if (!new_block) return 0;

        new_block->start_addr = best_fit->start_addr + size;
        new_block->size = best_fit->size - size;
        new_block->is_free = 1;
        new_block->addr_next = NULL;
        new_block->addr_prev = NULL;
        memset(&new_block->radix_node, 0, sizeof(CUradixNode));
        
        best_fit->size = size;
        
        // Insert new block into both lists
        insert_addr_list(allocator, new_block);
        radixTreeInsert(&allocator->size_tree, &new_block->radix_node, new_block->size);
    }

    // Mark block as allocated
    best_fit->is_free = 0;
    allocator->used_va_size += best_fit->size;

    // Remove from size tree
    radixTreeRemove(&best_fit->radix_node);

    return best_fit->start_addr;
}

// Free VA space
void va_free(va_allocator_t *allocator, uint64_t addr) {
    if (!allocator) return;

    // Find the block
    va_block_t *block = allocator->addr_list;
    while (block && block->start_addr != addr) {
        block = block->addr_next;
    }

    if (!block || block->is_free) {
        return;
    }

    // Mark as free
    block->is_free = 1;
    allocator->used_va_size -= block->size;

    // Coalesce with adjacent free blocks
    va_block_t *prev = block->addr_prev;
    va_block_t *next = block->addr_next;

    if (prev) {
        assert(block->start_addr == prev->start_addr + prev->size);
    }

    // Coalesce with previous block
    if (prev && prev->is_free) {
        prev->size += block->size;
        remove_addr_list(allocator, block);
        radixTreeRemove(&prev->radix_node);
        free(block);
        block = prev;
    }

    if (next) {
        assert(block->start_addr + block->size == next->start_addr);
    }

    // Coalesce with next block
    if (next && next->is_free) {
        block->size += next->size;
        remove_addr_list(allocator, next);
        radixTreeRemove(&next->radix_node);
        free(next);
    }

    // Insert the (possibly coalesced) block back into the size tree
    radixTreeInsert(&allocator->size_tree, &block->radix_node, block->size);
}

// Get total VA size
uint64_t va_allocator_get_total_size(va_allocator_t *allocator) {
    if (!allocator) return 0;
    return VA_SIZE;
}

// Get used VA size
uint64_t va_allocator_get_used_size(va_allocator_t *allocator) {
    if (!allocator) return 0;
    return ((struct va_allocator *)allocator)->used_va_size;
}