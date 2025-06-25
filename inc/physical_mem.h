#ifndef PHYSICAL_MEM_H
#define PHYSICAL_MEM_H

#include "common.h"

typedef struct physical_mem {
    // This is the VA that we will mmap and mlock.
    uint64_t internal_va;
    uint64_t size;
} physical_mem_t;

physical_mem_t* allocate_physical_mem(uint64_t size);
void free_physical_mem(physical_mem_t* mem);
int map_physical_mem(physical_mem_t* mem, uint64_t va, uint64_t size);
int unmap_physical_mem(physical_mem_t* mem, uint64_t va, uint64_t size);

#endif