#!/bin/bash

set -e

export CXX_LD="mold"
# need to use g++ instead of clang++ since my default (14) is not new enough
# and cba to install a newer version
export CXX="g++"

ROOT_SRC_DIR="$(cd -P "$(dirname "${BASH_SOURCE[0]}")/.." && pwd )"
BUILD_DIR="$ROOT_SRC_DIR/build"

if [ ! -d "$BUILD_DIR" ]; then
     mkdir "$BUILD_DIR"
fi

cd "$BUILD_DIR"

cmake -GNinja -DCMAKE_BUILD_TYPE=Release "$ROOT_SRC_DIR"
ninja