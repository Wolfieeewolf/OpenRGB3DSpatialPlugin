#!/usr/bin/env bash
# Build a slim static Assimp (OBJ / STL / 3MF / glTF).
# Default dirs: dependencies/assimp-build + dependencies/assimp-install.
# Override with ASSIMP_BUILD_DIR / ASSIMP_INSTALL_DIR (qmake uses OUT_PWD/_deps).
# Usage: scripts/build-assimp.sh [--clean]
# Idempotent: skips when lib/libassimp.a already exists (unless --clean).

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ASSIMP_SRC="$ROOT/dependencies/assimp"
BUILD_DIR="${ASSIMP_BUILD_DIR:-$ROOT/dependencies/assimp-build}"
INSTALL_DIR="${ASSIMP_INSTALL_DIR:-$ROOT/dependencies/assimp-install}"
STABLE_LIB="$INSTALL_DIR/lib/libassimp.a"
CLEAN=0

for arg in "$@"; do
  case "$arg" in
    --clean|-Clean|-clean) CLEAN=1 ;;
  esac
done

if [ ! -f "$ASSIMP_SRC/CMakeLists.txt" ]; then
  echo "Assimp sources missing at $ASSIMP_SRC — run: git submodule update --init dependencies/assimp" >&2
  exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
  echo "cmake not found. Install cmake (apt/dnf/pacman or: brew install cmake)." >&2
  exit 1
fi

if [ "$CLEAN" -eq 1 ]; then
  rm -rf "$BUILD_DIR" "$INSTALL_DIR"
fi

if [ -f "$STABLE_LIB" ] && [ "$CLEAN" -eq 0 ]; then
  echo "Assimp already installed: $STABLE_LIB"
  exit 0
fi

mkdir -p "$BUILD_DIR" "$INSTALL_DIR"

GENERATOR_ARGS=()
if command -v ninja >/dev/null 2>&1; then
  GENERATOR_ARGS=(-G Ninja)
fi

cmake -S "$ASSIMP_SRC" -B "$BUILD_DIR" "${GENERATOR_ARGS[@]}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
  -DCMAKE_INSTALL_LIBDIR=lib \
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

if [ ! -f "$STABLE_LIB" ] && [ -f "$INSTALL_DIR/lib64/libassimp.a" ]; then
  mkdir -p "$INSTALL_DIR/lib"
  cp -f "$INSTALL_DIR/lib64/libassimp.a" "$STABLE_LIB"
fi

if [ ! -f "$STABLE_LIB" ]; then
  echo "Assimp install finished but $STABLE_LIB is missing" >&2
  exit 1
fi

echo "Slim Assimp installed to $INSTALL_DIR"
