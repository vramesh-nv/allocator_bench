cmake_minimum_required(VERSION 3.10)
project(va_allocator C CXX)

# Set build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type (Debug or Release)" FORCE)
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set compiler flags - treat warnings as errors
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -Wno-error=shift-negative-value")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wno-error=shift-negative-value")

# Set debug flags
set(CMAKE_C_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

# Set release flags
set(CMAKE_C_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# Add include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

# Create static library for radix tree
add_library(radix STATIC
    utils/radix.c
    utils/bitvector.c
    utils/avl.c
    utils/addrtracker.c
)
set_target_properties(radix PROPERTIES 
    LINKER_LANGUAGE C
    COMPILE_FLAGS "-g"
)

# Create static library for VA allocator
add_library(va_allocator STATIC
    src/va_allocator.c
    src/va_allocator_default.c
    src/va_allocator_arenas.c
)
set_target_properties(va_allocator PROPERTIES 
    LINKER_LANGUAGE C
    COMPILE_FLAGS "-g"
)

# Link VA allocator with radix
target_link_libraries(va_allocator PRIVATE radix)

# Create test executable
add_executable(test_va_allocator
    tests/test_va_allocator.cpp
)
set_target_properties(test_va_allocator PROPERTIES
    COMPILE_FLAGS "-g -O0"
    LINK_FLAGS "-g"
)

# Link test executable with libraries
target_link_libraries(test_va_allocator PRIVATE
    va_allocator
    radix
)

# Create arena allocator test executable
add_executable(test_va_allocator_arena
    tests/test_va_allocator_arena.cpp
)
set_target_properties(test_va_allocator_arena PROPERTIES
    COMPILE_FLAGS "-g -O0"
    LINK_FLAGS "-g"
)
target_link_libraries(test_va_allocator_arena PRIVATE
    va_allocator
    radix
)

# Create performance test executable
add_executable(test_va_allocator_perf tests/test_va_allocator_perf.cpp)
set_target_properties(test_va_allocator_perf PROPERTIES
    COMPILE_FLAGS "-g -O0"
    LINK_FLAGS "-g"
)
target_link_libraries(test_va_allocator_perf PRIVATE
    va_allocator
    radix
)

# Create benchmark test executable
add_executable(test_va_allocator_benchmark tests/test_va_allocator_benchmark.cpp)
set_target_properties(test_va_allocator_benchmark PROPERTIES
    COMPILE_FLAGS "-g -O3"  # Use O3 for better benchmark performance
    LINK_FLAGS "-g"
)
target_link_libraries(test_va_allocator_benchmark PRIVATE
    va_allocator
    radix
)

# Enable testing
enable_testing()
add_test(NAME va_allocator_test COMMAND test_va_allocator)
add_test(NAME va_allocator_arena_test COMMAND test_va_allocator_arena)
add_test(NAME va_allocator_perf_test COMMAND test_va_allocator_perf)
add_test(NAME va_allocator_benchmark_test COMMAND test_va_allocator_benchmark) 