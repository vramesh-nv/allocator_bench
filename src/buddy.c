#include "buddy.h"

#define MIN_BLOCK_SIZE (2ull * 1024ull * 1024ull)

#define COMPUTE_LOG2(out,x)             \
    do {                                \
        NvU32 i, j;                     \
        for (i=0, j=(x); j > 1; i++) {  \
            j = j >> 1;                 \
        }                               \
        (out) = i;                      \
    } while(0)

typedef enum buddy_block_state {
    BUDDY_BLOCK_STATE_FREE = 0,
    BUDDY_BLOCK_STATE_SPLIT = 1,
    BUDDY_BLOCK_STATE_ALLOCATED = 2,
    BUDDY_BLOCK_STATE_MAX = 0x7
} buddy_block_state_t;

typedef struct buddy_physical_block {
    physical_mem_t *mem;
    uint64_t size;
    //CUbitvector *bitmap;
    uint64_t num_levels;
    buddy_block_state_t *state;
    struct buddy_physical_block *next;
} buddy_physical_block_t;

typedef struct buddy_alloc_block {
    buddy_physical_block_t *block;
    uint64_t size;
    uint64_t offset;
} buddy_alloc_block_t;

struct buddy_allocator {
    physical_mem_mgr_t *mgr;
    uint64_t base_block_size;
    buddy_physical_block_t *blocks;
};

buddy_allocator_t *
buddy_allocator_create(uint64_t base_block_size)
{
    assert(base_block_size >= MIN_BLOCK_SIZE);
    assert(base_block_size % MIN_BLOCK_SIZE == 0);

    buddy_allocator_t *allocator = (buddy_allocator_t*)calloc(1, sizeof(*allocator));
    if (!allocator) {
        return NULL;
    }

    allocator->base_block_size = base_block_size;

    allocator->mgr = physical_mem_mgr_create();
    if (!allocator->mgr) {
        free(allocator);
        return NULL;
    }

    return allocator;
}

void buddy_allocator_destroy(buddy_allocator_t *allocator)
{
    if (!allocator) {
        return;
    }
    
    if (allocator->mgr) {
        physical_mem_mgr_destroy(allocator->mgr);
    }

    free(allocator);
}

#define INVALID_OFFSET (uint64_t)(-1)
static uint64_t split_or_alloc(buddy_physical_block_t *physical_block, uint64_t size, uint64_t level, uint64_t idx)
{
    uint64_t level_size = physical_block->size / (1ull << level);

    if (physical_block->state[idx] == BUDDY_BLOCK_STATE_ALLOCATED) {
        return INVALID_OFFSET;
    }

    if (size > level_size) {
        return INVALID_OFFSET;
    }

    if ((physical_block->state[idx] == BUDDY_BLOCK_STATE_FREE) && (size == level_size)) {
        physical_block->state[idx] = BUDDY_BLOCK_STATE_ALLOCATED;
        return 0;
    }

    // Even at the last level we can't find a free block.
    uint64_t next_level = level + 1;
    if (physical_block->num_levels == next_level) {
        return INVALID_OFFSET;
    }

    uint64_t left = 2*idx + 1;
    uint64_t right = 2*idx + 2;

    if (physical_block->state[left] != BUDDY_BLOCK_STATE_ALLOCATED) {
        uint64_t ret = split_or_alloc(physical_block, size, next_level, left);
        if (ret != INVALID_OFFSET) {
            assert(physical_block->state[left] != BUDDY_BLOCK_STATE_FREE);
            physical_block->state[idx] = BUDDY_BLOCK_STATE_SPLIT;
            return ret;
        }
    }

    if (physical_block->state[right] != BUDDY_BLOCK_STATE_ALLOCATED) {
        uint64_t ret = split_or_alloc(physical_block, size, next_level, right);
        if (ret != INVALID_OFFSET) {
            assert(physical_block->state[right] != BUDDY_BLOCK_STATE_FREE);
            physical_block->state[idx] = BUDDY_BLOCK_STATE_SPLIT;
            return ret + (physical_block->size / (1ull << next_level));
        }
    }

    return INVALID_OFFSET;
}

static buddy_alloc_block_t *
buddy_allocator_alloc_from_physical_block(buddy_allocator_t *allocator,
                                          buddy_physical_block_t *physical_block,
                                          uint64_t size)
{
    UNUSED(allocator);
    assert(size % MIN_BLOCK_SIZE == 0);
    assert(physical_block->state[0] == BUDDY_BLOCK_STATE_FREE);

    buddy_alloc_block_t *block = calloc(1, sizeof(*block));
    if (!block) {
        return NULL;
    }

    uint64_t offset = split_or_alloc(physical_block, size, 0, 0);
    assert(offset != INVALID_OFFSET);

    block->block = physical_block;
    block->size = size;
    block->offset = offset;

    return block;
}

static buddy_physical_block_t *
buddy_allocator_allocate_physical_block(buddy_allocator_t *allocator)
{
    buddy_physical_block_t *block = calloc(1, sizeof(*block));
    if (!block) {
        return NULL;
    }

    block->mem = allocate_physical_mem(allocator->mgr, allocator->base_block_size);
    if (!block->mem) {
        free(block);
        return NULL;
    }

    block->size = allocator->base_block_size;

    uint64_t levels = 0;
    COMPUTE_LOG2(levels, (block->size / MIN_BLOCK_SIZE));
    block->num_levels = levels + 1;
    block->state = calloc((1ull << block->num_levels) - 1, sizeof(*block->state));
    if (!block->state) {
        free_physical_mem(allocator->mgr, block->mem);
        free(block);
        return NULL;
    }

    return block;
}

buddy_alloc_block_t *
buddy_allocator_alloc(buddy_allocator_t *allocator, uint64_t size)
{
    if (!allocator || !size) {
        return NULL;
    }

    // Allocation request must never exceed the base block size
    assert(size <= allocator->base_block_size);

    for (buddy_physical_block_t *current = allocator->blocks; current; current = current->next) {
        if (current->state[0] != BUDDY_BLOCK_STATE_ALLOCATED) {
            uint64_t offset = split_or_alloc(current, size, 0, 0);
            if (offset != INVALID_OFFSET) {
                buddy_alloc_block_t *alloc_block = calloc(1, sizeof(*alloc_block));
                assert(alloc_block);

                alloc_block->block = current;
                alloc_block->size = size;
                alloc_block->offset = offset;

                return alloc_block;
            }
        }
    }

    // We couldn't find a block that can serve this request, so we need to allocate a new one
    buddy_physical_block_t *block = buddy_allocator_allocate_physical_block(allocator);
    if (!block) {
        return NULL;
    }

    buddy_alloc_block_t *alloc_block = buddy_allocator_alloc_from_physical_block(allocator, block, size);
    if (!alloc_block) {
        //buddy_allocator_free_physical_block(allocator, block);
        return NULL;
    }

    // Add the physical block to the allocator's linked list
    block->next = allocator->blocks;
    allocator->blocks = block;

    return alloc_block;
}

uint64_t buddy_allocator_get_total_physical_mem_usage(buddy_allocator_t *allocator)
{
    return get_total_physical_mem_usage(allocator->mgr);
}