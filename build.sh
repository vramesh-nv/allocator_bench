#!/bin/bash

# Default to debug build
BUILD_TYPE=${1:-Debug}

# Create build directory if it doesn't exist
mkdir -p build

# Configure and build
cd build
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..
make -j$(nproc)

# Print build type
echo "Built in $BUILD_TYPE mode"

# Run the tests
ctest --output-on-failure 