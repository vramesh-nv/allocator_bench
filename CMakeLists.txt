cmake_minimum_required(VERSION 3.10)
project(va_allocator C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set compiler flags - treat warnings as errors
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

# Add include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

# Create static library for radix tree
add_library(radix STATIC
    utils/radix.c
)
set_target_properties(radix PROPERTIES LINKER_LANGUAGE C)

# Create static library for VA allocator
add_library(va_allocator STATIC
    src/va_allocator.c
    src/va_allocator_default.c
    src/va_allocator_arenas.c
)
set_target_properties(va_allocator PROPERTIES LINKER_LANGUAGE C)

# Link VA allocator with radix
target_link_libraries(va_allocator PRIVATE radix)

# Create test executable
add_executable(test_va_allocator
    tests/test_va_allocator.cpp
)

# Link test executable with libraries
target_link_libraries(test_va_allocator PRIVATE
    va_allocator
    radix
)

# Enable testing
enable_testing()
add_test(NAME va_allocator_test COMMAND test_va_allocator) 