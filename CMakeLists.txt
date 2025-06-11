cmake_minimum_required(VERSION 3.10)
project(va_allocator C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set compiler flags - treat warnings as errors
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")

# Create static library for radix tree
add_library(radix STATIC
    radix.c
)
set_target_properties(radix PROPERTIES LINKER_LANGUAGE C)

# Create static library for VA allocator
add_library(va_allocator STATIC
    va_allocator.c
    va_allocator_default.c
)
set_target_properties(va_allocator PROPERTIES LINKER_LANGUAGE C)

# Link VA allocator with radix
target_link_libraries(va_allocator PRIVATE radix)

# Create test executable
add_executable(test_va_allocator
    test_va_allocator.cpp
)

# Link test executable with libraries
target_link_libraries(test_va_allocator PRIVATE
    va_allocator
    radix
)

# Add include directories
target_include_directories(va_allocator PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(radix PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(test_va_allocator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Enable testing
enable_testing()
add_test(NAME va_allocator_test COMMAND test_va_allocator) 