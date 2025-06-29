#ifndef PHYSICAL_MEM_H
#define PHYSICAL_MEM_H

#include "common.h"

typedef struct physical_mem_mgr physical_mem_mgr_t;
typedef struct physical_mem physical_mem_t;

physical_mem_mgr_t* physical_mem_mgr_create(void);
void physical_mem_mgr_destroy(physical_mem_mgr_t *mgr);

physical_mem_t* allocate_physical_mem(physical_mem_mgr_t *mgr, uint64_t size);
void free_physical_mem(physical_mem_mgr_t *mgr, physical_mem_t* mem);
int map_physical_mem(physical_mem_t* mem, uint64_t va, uint64_t size);
int unmap_physical_mem(physical_mem_t* mem, uint64_t va, uint64_t size);

uint64_t get_total_physical_mem_usage(physical_mem_mgr_t *mgr);

#endif