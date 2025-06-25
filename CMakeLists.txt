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
    src/physical_mem.c
)
set_target_properties(va_allocator PROPERTIES 
    LINKER_LANGUAGE C
    COMPILE_FLAGS "-g"
)

# Link VA allocator with radix
target_link_libraries(va_allocator PRIVATE radix)

# Create sanity test executable
add_executable(sanity
    tests/sanity.cpp
)
set_target_properties(sanity PROPERTIES
    COMPILE_FLAGS "-g -O0"
    LINK_FLAGS "-g"
)

# Link sanity test executable with libraries
target_link_libraries(sanity PRIVATE
    va_allocator
    radix
)

# Enable testing
enable_testing()
add_test(NAME sanity_test COMMAND sanity) 