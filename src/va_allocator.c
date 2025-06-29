#define _GNU_SOURCE
#include "va_allocator.h"
#include "va_allocator_default.h"
#include "va_allocator.arenas.h"
#include "common.h"

// Main allocator structure
struct va_allocator {
    va_allocator_ops_t* ops;
};

va_allocator_t* va_allocator_init(va_allocator_type_t type) {
    va_allocator_t *allocator = (va_allocator_t *)calloc(1, sizeof(*allocator));
    if (!allocator) {
        return NULL;
    }

    // Select implementation based on type
    switch (type) {
        case VA_ALLOCATOR_TYPE_DEFAULT:
            allocator->ops = get_default_allocator_ops();
            allocator->ops->impl = init_default_allocator();
            break;
        case VA_ALLOCATOR_TYPE_ARENA:
            allocator->ops = get_arena_allocator_ops();
            allocator->ops->impl = init_arena_allocator();
            break;
        default:
            free(allocator);
            return NULL;
    }

    if (!allocator->ops->impl) {
        free(allocator);
        return NULL;
    }

    return allocator;
}

void
va_allocator_destroy(va_allocator_t *allocator) {
    if (!allocator) {
        return;
    }

    if (allocator->ops && allocator->ops->destroy) {
        allocator->ops->destroy(allocator->ops->impl);
    }   
    free(allocator);
}

uint64_t
va_alloc(va_allocator_t *allocator, uint64_t size) {
    if (!allocator || !allocator->ops || !allocator->ops->alloc) {
        return 0;
    }
    return allocator->ops->alloc(allocator->ops->impl, size);
}

void
va_free(va_allocator_t *allocator, uint64_t addr) {
    if (!allocator || !allocator->ops || !allocator->ops->free) {
        return;
    }
    allocator->ops->free(allocator->ops->impl, addr);
}

uint64_t
va_allocator_get_total_size(va_allocator_t *allocator) {
    if (!allocator || !allocator->ops || !allocator->ops->get_total_size) {
        return 0;
    }
    return allocator->ops->get_total_size(allocator->ops->impl);
}

uint64_t
va_allocator_get_used_size(va_allocator_t *allocator) {
    if (!allocator || !allocator->ops || !allocator->ops->get_used_size) {
        return 0;
    }
    return allocator->ops->get_used_size(allocator->ops->impl);
}

void
va_allocator_print(va_allocator_t *allocator) {
    if (!allocator || !allocator->ops || !allocator->ops->impl) {
        return;
    }
    allocator->ops->print(allocator->ops->impl);
}

void
va_flush(va_allocator_t *allocator) {
    if (!allocator || !allocator->ops || !allocator->ops->flush) {
        return;
    }
    allocator->ops->flush(allocator->ops->impl);
}

uint64_t
get_physical_mem_usage(va_allocator_t *allocator) {
    if (!allocator) {
        return 0;
    }
    return allocator->ops->get_physical_mem_usage(allocator->ops->impl);
}