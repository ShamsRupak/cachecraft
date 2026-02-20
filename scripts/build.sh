#!/usr/bin/env bash
set -euo pipefail

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build"

EXTRA_FLAGS=""
if [[ "$BUILD_TYPE" == "Debug" ]]; then
    EXTRA_FLAGS="-DENABLE_ASAN=ON -DENABLE_UBSAN=ON"
fi

echo "==> Configuring ($BUILD_TYPE)..."
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" $EXTRA_FLAGS

echo "==> Building..."
cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"

echo "==> Done. Binaries in $BUILD_DIR/"
echo "    ./build/cachecraftd       — server"
echo "    ./build/cachecraft-cli    — client"
echo "    ./build/cachecraft-bench  — benchmark"
