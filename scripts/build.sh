#!/bin/bash

set -e

export CXX_LD="mold"
export CXX="clang++-16"

ROOT_SRC_DIR="$(cd -P "$(dirname "${BASH_SOURCE[0]}")/.." && pwd )"
BUILD_DIR="$ROOT_SRC_DIR/build"

if [ ! -d "$BUILD_DIR" ]; then
     mkdir "$BUILD_DIR"
fi

cd "$BUILD_DIR"

cmake -GNinja -DCMAKE_BUILD_TYPE=Release "$ROOT_SRC_DIR"
ninja