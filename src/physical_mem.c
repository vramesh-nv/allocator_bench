#include "physical_mem.h"

typedef struct physical_mem physical_mem_t;

typedef struct mapping {
    physical_mem_t *mem;
    uint64_t va;
    uint64_t size;
    struct mapping *next;
} mapping_t;

typedef struct physical_mem {
    // This is the VA that we will mmap and mlock.
    uint64_t internal_va;
    uint64_t size;
    mapping_t *mapping_list;
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
        assert(curr == mem);
        assert(curr->mapping_list == NULL);
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

    uint64_t start_va = va;
    uint64_t end_va = va + size;
    assert(start_va < end_va);

    mapping_t *current = mem->mapping_list;
    while (current) {

        if (start_va >= current->va && start_va < (current->va + current->size)) {
            return -1;
        }

        if (end_va >= current->va && end_va < (current->va + current->size)) {
            return -1;
        }
        current = current->next;
    }

    mapping_t *mapping = (mapping_t*)calloc(1, sizeof(*mapping));
    if (!mapping) {
        return -1;
    }

    // Mimic GPU mapping behavior through mmap access flags
    void *ret = mmap(UINT2PTR(va), size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ret == MAP_FAILED) {
        free(mapping);
        return -1;
    }

    mapping->mem = mem;
    mapping->va = va;
    mapping->size = size;
    mapping->next = mem->mapping_list;
    mem->mapping_list = mapping;

    return 0;
}

int unmap_physical_mem(physical_mem_t* mem, uint64_t va, uint64_t size)
{
    if (mem == NULL || va == 0 || size == 0) {
        return -1;
    }

    mapping_t *prev = NULL;
    mapping_t *current = mem->mapping_list;
    while (current) {
        if (current->va == va) {
            if (prev) {
                prev->next = current->next;
            } else {
                mem->mapping_list = current->next;
            }
            break;
        }
        prev = current;
        current = current->next;
    }

    if (current == NULL) {
        return -1;
    }

    // Mimic GPU unmapping behavior through mmap access flags
    void *ret = mmap(UINT2PTR(va), size, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ret == MAP_FAILED) {
        assert(0);
        return -1;
    }
    free(current);

    return 0;
}

uint64_t get_total_physical_mem_usage(physical_mem_mgr_t *mgr)
{
    return mgr->used_size;
}