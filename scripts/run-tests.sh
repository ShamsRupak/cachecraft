#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build"

if [[ ! -d "$BUILD_DIR" ]]; then
    echo "Build directory not found. Run scripts/build.sh first."
    exit 1
fi

echo "==> Running tests..."
ctest --test-dir "$BUILD_DIR" --output-on-failure "$@"
