#!/bin/bash
# Script to build Orka for Linux (Release)

set -e

echo "╔══════════════════════════════════════════╗"
echo "║  Linux Release Build — ORKA              ║"
echo "╚══════════════════════════════════════════╝"

# Create build directory
mkdir -p build
cd build

# Run CMake Configuration
echo "[1/3] Configuring CMake for Release..."
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ..

# Build
echo "[2/3] Compiling..."
cmake --build . --config Release -j $(nproc)

# Output
echo "[3/3] Build complete!"
echo ""
echo "Executable is located at: build/orka"
echo "To run tests: g++ -std=c++17 -Isrc -o build_tests tests/test_language_engine.cpp src/core/*.cpp -lpthread && ./build_tests"
