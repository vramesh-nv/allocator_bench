#include "physical_mem.h"

static uint64_t total_physical_mem_used = 0;

physical_mem_t* allocate_physical_mem(uint64_t size)
{
    if (total_physical_mem_used + size > PHYSICAL_MEMORY_SIZE) {
        printf("Warning: Not enough physical memory available\n");
        return NULL;
    }

    physical_mem_t *mem = (physical_mem_t*)calloc(1, sizeof(*mem));
    if (!mem) {
        return NULL;
    }

    mem->internal_va = PTR2UINT(RESERVE_VA(size));
    if (mem->internal_va == 0) {
        free(mem);
        return NULL;
    }
    mem->size = size;
    total_physical_mem_used += size;

    return mem;
}

void free_physical_mem(physical_mem_t* mem) {
    if (mem) {
        uint64_t size = mem->size;
        FREE_VA(UINT2PTR(mem->internal_va), mem->size);
        free(mem);
        assert(total_physical_mem_used >= size);
        total_physical_mem_used -= size;
    }
}

int map_physical_mem(physical_mem_t* mem, uint64_t va, uint64_t size)
{
    if (mem == NULL || va == 0 || size == 0) {
        return -1;
    }

    /*int ret = (int)mmap(UINT2PTR(va), size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ret == MAP_FAILED) {
        return -1;
    }*/
    return 0;
}

int unmap_physical_mem(physical_mem_t* mem, uint64_t va, uint64_t size)
{
    UNUSED(mem);
    UNUSED(va);
    UNUSED(size);
    return 0;
}