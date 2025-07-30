#ifndef BUDDY_H
#define BUDDY_H

#include "common.h"
#include "physical_mem.h"

/*
* Notes:
* When implementing a buddy allocator for physical memory in the CUDA driver, we need to take care of a few things:
* - Ensure we do a best fit without splitting first.
* - If that fails, we can split one of the blocks.
* Doing otherwise will result in fragmenting the allocator. Maybe this also requires us to keep track of free list of portions of blocks
* as per size. Keeping the global list and block's internal split state consistent will be tricky.
*
*/

typedef struct buddy_allocator buddy_allocator_t;
typedef struct buddy_alloc_block buddy_alloc_block_t;

buddy_allocator_t* buddy_allocator_create(uint64_t base_block_size);
void buddy_allocator_destroy(buddy_allocator_t *allocator);

buddy_alloc_block_t *buddy_allocator_alloc(buddy_allocator_t *allocator, uint64_t size);
void buddy_allocator_free(buddy_allocator_t *allocator, buddy_alloc_block_t *alloc_block);

void buddy_free_physical_blocks(buddy_allocator_t *allocator);
uint64_t buddy_allocator_get_total_physical_mem_usage(buddy_allocator_t *allocator);

#endif