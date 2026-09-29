#!/usr/bin/env bash
# Build a slim static Assimp into dependencies/assimp-install.
# Formats: OBJ, STL, 3MF, glTF/GLB.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ASSIMP_SRC="$ROOT/dependencies/assimp"
BUILD_DIR="$ROOT/dependencies/assimp-build"
INSTALL_DIR="$ROOT/dependencies/assimp-install"

if [ ! -f "$ASSIMP_SRC/CMakeLists.txt" ]; then
  echo "Assimp sources missing at $ASSIMP_SRC (git submodule update --init dependencies/assimp)" >&2
  exit 1
fi

rm -rf "$BUILD_DIR" "$INSTALL_DIR"
mkdir -p "$BUILD_DIR" "$INSTALL_DIR"

GENERATOR_ARGS=()
if command -v ninja >/dev/null 2>&1; then
  GENERATOR_ARGS=(-G Ninja)
fi

cmake -S "$ASSIMP_SRC" -B "$BUILD_DIR" "${GENERATOR_ARGS[@]}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
  -DASSIMP_BUILD_TESTS=OFF \
  -DASSIMP_BUILD_ASSIMP_TOOLS=OFF \
  -DASSIMP_BUILD_SAMPLES=OFF \
  -DASSIMP_INSTALL_PDB=OFF \
  -DBUILD_SHARED_LIBS=OFF \
  -DASSIMP_BUILD_ZLIB=ON \
  -DASSIMP_NO_EXPORT=ON \
  -DASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=OFF \
  -DASSIMP_BUILD_OBJ_IMPORTER=ON \
  -DASSIMP_BUILD_STL_IMPORTER=ON \
  -DASSIMP_BUILD_3MF_IMPORTER=ON \
  -DASSIMP_BUILD_GLTF_IMPORTER=ON \
  -DASSIMP_WARNINGS_AS_ERRORS=OFF \
  -DASSIMP_INJECT_DEBUG_POSTFIX=OFF

cmake --build "$BUILD_DIR" --config Release --parallel
cmake --install "$BUILD_DIR" --config Release

echo "Slim Assimp installed to $INSTALL_DIR"
