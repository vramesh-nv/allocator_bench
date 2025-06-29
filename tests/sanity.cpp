#include <iostream>
#include "va_allocator.h"
#include "common.h"
#include <vector>

static void test_basic_alloc(void)
{
    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_DEFAULT);
    if (!allocator) {
        std::cerr << "Failed to initialize allocator" << std::endl;
        return;
    }

    uint64_t addr = va_alloc(allocator, PHYSICAL_MEMORY_SIZE);
    if (!addr) {
        std::cerr << "Failed to allocate memory" << std::endl;
        return;
    }
    assert(get_physical_mem_usage(allocator) == PHYSICAL_MEMORY_SIZE);

    // Ensure free does not change physical memory usage
    va_free(allocator, addr);
    assert(get_physical_mem_usage(allocator) == PHYSICAL_MEMORY_SIZE);

    // Verify that flush frees all physical memory
    va_flush(allocator);
    assert(get_physical_mem_usage(allocator) == 0);

    uint64_t block_size = 32ull * 1024ull * 1024ull;

    std::vector<uint64_t> vas(PHYSICAL_MEMORY_SIZE / block_size);
    for (uint64_t i = 0; i < vas.size(); i++) {
        vas[i] = va_alloc(allocator, block_size);
        assert(vas[i]);
    }

    assert(get_physical_mem_usage(allocator) == PHYSICAL_MEMORY_SIZE);
    assert(va_alloc(allocator, block_size) == 0);

    for (uint64_t i = 0; i < vas.size(); i += 2) {
        va_free(allocator, vas[i]);
        vas[i] = 0;
    }
    va_flush(allocator);
    assert(get_physical_mem_usage(allocator) == PHYSICAL_MEMORY_SIZE / 2);

    for (uint64_t i = 0; i < vas.size(); i++) {
        if (vas[i]) {
            va_free(allocator, vas[i]);
            vas[i] = 0;
        }
    }
    va_flush(allocator);
    assert(get_physical_mem_usage(allocator) == 0);

    va_allocator_destroy(allocator);
}

int main(void)
{
    test_basic_alloc();
    return 0;
}