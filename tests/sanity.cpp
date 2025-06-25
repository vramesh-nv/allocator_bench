#include <iostream>
#include "va_allocator.h"
//#include "va_allocator_default.h"

int main(void) {
    //std::cout << "Hello, World!" << std::endl;

    va_allocator_t *allocator = va_allocator_init(VA_ALLOCATOR_TYPE_DEFAULT);
    if (!allocator) {
        std::cerr << "Failed to initialize allocator" << std::endl;
        return 1;
    }

    uint64_t addr = va_alloc(allocator, 33ull * 1024ull * 1024ull);
    if (!addr) {
        std::cerr << "Failed to allocate memory" << std::endl;
        return 1;
    }
    //std::cout << "Allocated memory at " << addr << std::endl;
    va_free(allocator, addr);

    va_flush(allocator);
    //std::cout << "Freed memory at " << addr << std::endl;

    va_allocator_destroy(allocator);
    return 0;
}