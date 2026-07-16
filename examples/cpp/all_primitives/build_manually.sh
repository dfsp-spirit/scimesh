#!/bin/bash
set -e

# Build scimesh's "all_primitives" example without CMake, using only g++.
#
# The directory layout assumed by this script (from scimesh repo root):
#
#   scimesh/
#   ├── examples/
#   │   └── cpp/
#   │       └── all_primitives/        ← this script and main.cpp live here
#   │           ├── main.cpp
#   │           ├── build_manually.sh
#   │           └── build_manual/       ← created below, run from here
#   └── src/                            ← scimesh library source
#       ├── core/
#       │   ├── renderer.cpp
#       │   ├── camera.cpp
#       │   ├── image.cpp
#       │   ├── primitives.cpp
#       │   ├── rasterizer.cpp
#       │   ├── normals.cpp
#       │   ├── clipping.cpp
#       │   └── transforms.cpp
#       └── third_party/
#           └── glm/
#
# This mirrors the manual build described in docs/CPP_GETTING_STARTED.md:
# the scimesh src/ directory sits as a sibling to the project and is
# referenced via relative ".." paths from the build directory.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build_manual"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

g++ -std=c++17 -O2 -DSCIMESH_STB_WRITE_IMPL \
    -I../../../../src/core \
    -I../../../../src/third_party \
    -I../../../../src/third_party/glm \
    ../main.cpp \
    ../../../../src/core/renderer.cpp \
    ../../../../src/core/camera.cpp \
    ../../../../src/core/image.cpp \
    ../../../../src/core/primitives.cpp \
    ../../../../src/core/rasterizer.cpp \
    ../../../../src/core/normals.cpp \
    ../../../../src/core/clipping.cpp \
    ../../../../src/core/transforms.cpp \
    -o all_primitives

echo "Build successful.  Run:"
echo "  cd $BUILD_DIR && ./all_primitives"
