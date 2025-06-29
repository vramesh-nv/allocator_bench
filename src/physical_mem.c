#include "physical_mem.h"

typedef struct physical_mem {
    // This is the VA that we will mmap and mlock.
    uint64_t internal_va;
    uint64_t size;
    physical_mem_t *next;
} physical_mem_t;

typedef struct physical_mem_mgr {
    uint64_t total_size;
    uint64_t used_size;
    physical_mem_t *mem_list;
} physical_mem_mgr_t;

physical_mem_mgr_t* physical_mem_mgr_create(void)
{
    physical_mem_mgr_t *mgr = (physical_mem_mgr_t*)calloc(1, sizeof(*mgr));
    if (!mgr) {
        return NULL;
    }

    mgr->total_size = PHYSICAL_MEMORY_SIZE;
    return mgr;
}

void physical_mem_mgr_destroy(physical_mem_mgr_t *mgr)
{
    if (mgr) {
        free(mgr);
    }
}

physical_mem_t* allocate_physical_mem(physical_mem_mgr_t *mgr, uint64_t size)
{
    if (mgr->used_size + size > mgr->total_size) {
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
    mgr->used_size += size;

    mem->next = mgr->mem_list;
    mgr->mem_list = mem;

    return mem;
}

void free_physical_mem(physical_mem_mgr_t *mgr, physical_mem_t* mem) {
    if (mem) {
        uint64_t size = mem->size;

        physical_mem_t *prev = NULL;
        physical_mem_t *curr = mgr->mem_list;
        while (curr) {
            if (curr == mem) {
                if (prev) {
                    prev->next = curr->next;
                } else {
                    mgr->mem_list = curr->next;
                }
                break;
            }
            prev = curr;
            curr = curr->next;
        }
        FREE_VA(UINT2PTR(mem->internal_va), mem->size);
        free(mem);
        assert(mgr->used_size >= size);
        mgr->used_size -= size;

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

uint64_t get_total_physical_mem_usage(physical_mem_mgr_t *mgr)
{
    return mgr->used_size;
}