#include "va_allocator_default.h"
#include "common.h"
#include "radix.h"
#include "physical_mem.h"

#define VA_RESERVATION_SIZE (2 * PHYSICAL_MEMORY_SIZE)
#define PHYSICAL_BLOCK_SIZE (32ull * 1024ull * 1024ull) // 32MB
static_assert(PHYSICAL_MEMORY_SIZE % PHYSICAL_BLOCK_SIZE == 0, "PHYSICAL_MEMORY_SIZE must be divisible by PHYSICAL_BLOCK_SIZE");

typedef struct block_range {
    uint64_t low_idx;  // inclusive
    uint64_t high_idx; // inclusive
} block_range_t;

// Structure to represent a memory block
typedef struct va_block {
    uint64_t start_addr;        // Starting address of the block
    uint64_t size;              // Size of the block
    block_range_t block_range;  // Range of physical blocks that the block occupies
    int is_free;                // Whether the block is free
    struct va_block *addr_next; // Next block in address-ordered list
    struct va_block *addr_prev; // Previous block in address-ordered list
    CUradixNode radix_node;     // Node in size-ordered radix tree
} va_block_t;

// Default implementation structure
typedef struct {
    va_block_t *addr_list;             // List ordered by address
    CUradixTree size_tree;             // Tree ordered by size
    physical_mem_t **physical_blocks;  // Physical memory
    uint64_t num_physical_blocks;      // Number of physical blocks

    uint64_t total_va_size;            // Total VA space size
    uint64_t used_va_size;             // Currently used VA space
} va_allocator_default_t;

// Helper function to print the va blocks
static void
default_allocator_print(void *impl) {
    va_allocator_default_t *default_impl = (va_allocator_default_t *)impl;
    va_block_t *current = default_impl->addr_list;
    while (current) {
        printf("Block: start_addr: %lu, size: %lu, is_free: %d\n",
            (unsigned long)current->start_addr,
            (unsigned long)current->size,
            current->is_free);
        current = current->addr_next;
    }
} 

// Helper function to insert block into address-ordered list
static void
insert_addr_list(va_allocator_default_t *impl, va_block_t *block) {
    va_block_t *current = impl->addr_list;
    va_block_t *prev = NULL;
    while (current && current->start_addr < block->start_addr) {
        prev = current;
        current = current->addr_next;
    }
    block->addr_next = current;
    block->addr_prev = prev;
    if (prev) {
        prev->addr_next = block;
    } else {
        impl->addr_list = block;
    }
    if (current) {
        current->addr_prev = block;
    }
}

// Helper function to remove block from address-ordered list
static void
remove_addr_list(va_allocator_default_t *impl, va_block_t *block) {
    if (block->addr_prev) {
        block->addr_prev->addr_next = block->addr_next;
    } else {
        impl->addr_list = block->addr_next;
    }
    if (block->addr_next) {
        block->addr_next->addr_prev = block->addr_prev;
    }
}

static void default_free(void *impl, uint64_t addr);

// Implementation of alloc function
static uint64_t
default_alloc(void *impl, uint64_t size) {
    va_allocator_default_t *default_impl = (va_allocator_default_t *)impl;
    if (size == 0 || size > default_impl->total_va_size) {
        return 0;
    }

    CUradixNode *node = radixTreeFindGEQ(&default_impl->size_tree, size);
    if (!node) {
        return 0;  // No suitable block found
    }

    va_block_t *best_fit = container_of(node, va_block_t, radix_node);
    // Split block if necessary
    if (best_fit->size > size) {
        va_block_t *new_block = calloc(1, sizeof(*new_block));
        if (!new_block) {
            return 0;
        }
        new_block->start_addr = best_fit->start_addr + size;
        new_block->size = best_fit->size - size;
        new_block->is_free = 1;
        new_block->addr_next = NULL;
        new_block->addr_prev = NULL;

        new_block->block_range.low_idx = (new_block->start_addr - default_impl->addr_list->start_addr) / PHYSICAL_BLOCK_SIZE;
        new_block->block_range.high_idx = new_block->block_range.low_idx + (new_block->size - 1) / PHYSICAL_BLOCK_SIZE;

        // Update the best fit block's size to reflect this split
        best_fit->size = size;
        best_fit->block_range.high_idx = best_fit->block_range.low_idx + (best_fit->size - 1) / PHYSICAL_BLOCK_SIZE;
        insert_addr_list(default_impl, new_block);
        radixTreeInsert(&default_impl->size_tree, &new_block->radix_node, new_block->size);
    }

    // Mark the best fit block as in use
    best_fit->is_free = 0;
    default_impl->used_va_size += best_fit->size;
    radixTreeRemove(&best_fit->radix_node);

    // Update physical memory tracking
    for (uint64_t i = best_fit->block_range.low_idx; i <= best_fit->block_range.high_idx; i++) {
        default_impl->physical_blocks[i] = allocate_physical_mem(PHYSICAL_BLOCK_SIZE);
        if (!default_impl->physical_blocks[i]) {
            default_free(impl, best_fit->start_addr);
            return 0;
        }
    }

    return best_fit->start_addr;
}

// Implementation of free function
static void
default_free(void *impl, uint64_t addr) {
    va_allocator_default_t *default_impl = (va_allocator_default_t *)impl;
    if (!default_impl) {
        return;
    }

    va_block_t *block = default_impl->addr_list;
    while (block && block->start_addr != addr) {
        block = block->addr_next;
    }
    if (!block || block->is_free) {
        return;
    }

    block->is_free = 1;
    default_impl->used_va_size -= block->size;
    va_block_t *prev = block->addr_prev;
    va_block_t *next = block->addr_next;

    assert(!prev || (prev && (block->start_addr == prev->start_addr + prev->size)));
    if (prev && prev->is_free) {
        prev->size += block->size;
        prev->block_range.high_idx = prev->block_range.low_idx + (prev->size - 1) / PHYSICAL_BLOCK_SIZE;
        remove_addr_list(default_impl, block);
        radixTreeRemove(&prev->radix_node);
        free(block);
        block = prev;
    }

    assert(!next || (next && (block->start_addr + block->size == next->start_addr)));
    if (next && next->is_free) {
        block->size += next->size;
        block->block_range.high_idx = block->block_range.low_idx + (block->size - 1) / PHYSICAL_BLOCK_SIZE;
        remove_addr_list(default_impl, next);
        radixTreeRemove(&next->radix_node);
        free(next);
    }
    radixTreeInsert(&default_impl->size_tree, &block->radix_node, block->size);
}

// Implementation of get_total_size function
static uint64_t
default_get_total_size(void *impl) {
    va_allocator_default_t *default_impl = (va_allocator_default_t *)impl;
    return default_impl->total_va_size;
}

// Implementation of get_used_size function
static uint64_t
default_get_used_size(void *impl) {
    va_allocator_default_t *default_impl = (va_allocator_default_t *)impl;
    return default_impl->used_va_size;
}

// Implementation of destroy function
static void
default_destroy(void *impl) {
    va_allocator_default_t *default_impl = (va_allocator_default_t *)impl;
    if (!default_impl) {
        return;
    }

    va_block_t *current = default_impl->addr_list;
    while (current) {
        va_block_t *next = current->addr_next;
        free(current);
        current = next;
    }
    if (default_impl->addr_list) {
        FREE_VA((void *)default_impl->addr_list->start_addr, default_impl->total_va_size);
    }
    free(default_impl);
}

// Implementation of flush function
static void
default_flush(void *impl) {
    va_allocator_default_t *default_impl = (va_allocator_default_t *)impl;
    if (!default_impl) {
        return;
    }

    // Flush all physical blocks
    va_block_t *block = default_impl->addr_list;
    while (block) {
        if (block->is_free) {
            uint64_t blocks = block->block_range.high_idx - block->block_range.low_idx + 1;
            for (uint64_t i = 0; i < blocks; i++) {
                if (!default_impl->physical_blocks[block->block_range.low_idx + i]) {
                    continue;
                }
                if ((i == 0) && ((block->start_addr - default_impl->addr_list->start_addr) % PHYSICAL_BLOCK_SIZE != 0)) {
                    continue;
                }
                if ((i == blocks - 1) && (block->size % PHYSICAL_BLOCK_SIZE != 0)) {
                    continue;
                }
                free_physical_mem(default_impl->physical_blocks[block->block_range.low_idx + i]);
                default_impl->physical_blocks[block->block_range.low_idx + i] = NULL;
            }
        }
        block = block->addr_next;
    }
}

// Function to get the default implementation operations
va_allocator_ops_t *
get_default_allocator_ops(void) {
    static va_allocator_ops_t ops = {
        .alloc = default_alloc,
        .free = default_free,
        .get_total_size = default_get_total_size,
        .get_used_size = default_get_used_size,
        .print = default_allocator_print,
        .destroy = default_destroy,
        .flush = default_flush,
        .impl = NULL
    };
    return &ops;
}

// Function to initialize the default implementation
void *
init_default_allocator(void) {
    va_allocator_default_t *impl = (va_allocator_default_t *)calloc(1, sizeof(*impl));
    if (!impl) {
        return NULL;
    }

    impl->total_va_size = VA_RESERVATION_SIZE;
    impl->used_va_size = 0;
    impl->addr_list = NULL;
    radixTreeInit(&impl->size_tree, 63);  // Use 63 bits for size keys
    void *va_base = RESERVE_VA(impl->total_va_size);
    if (va_base == MAP_FAILED) {
        free(impl);
        return NULL;
    }

    // Allocate physical memory tracking
    uint64_t num_physical_blocks = impl->total_va_size / PHYSICAL_BLOCK_SIZE;
    impl->physical_blocks = calloc(num_physical_blocks, sizeof(physical_mem_t *));
    if (!impl->physical_blocks) {
        FREE_VA(va_base, impl->total_va_size);
        free(impl);
        return NULL;
    }
    impl->num_physical_blocks = num_physical_blocks;

    va_block_t *initial_block = calloc(1, sizeof(va_block_t));
    if (!initial_block) {
        FREE_VA(va_base, impl->total_va_size);
        free(impl->physical_blocks);
        free(impl);
        return NULL;
    }

    initial_block->start_addr = (uint64_t)va_base;
    initial_block->size = VA_RESERVATION_SIZE;
    initial_block->is_free = 1;
    initial_block->addr_next = NULL;
    initial_block->addr_prev = NULL;
    initial_block->block_range.low_idx = 0;
    initial_block->block_range.high_idx = num_physical_blocks - 1;
    impl->addr_list = initial_block;
    radixTreeInsert(&impl->size_tree, &initial_block->radix_node, VA_RESERVATION_SIZE);

    return impl;
}