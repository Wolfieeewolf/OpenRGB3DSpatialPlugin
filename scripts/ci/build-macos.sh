#!/usr/bin/env bash
# Build OpenRGB3DSpatialPlugin on macOS (Qt 5 or 6 via Homebrew).
# Usage: build-macos.sh <5|6> [release|debug]
set -euo pipefail

QT_MAJOR="${1:?Qt major version required (5 or 6)}"
BUILD_TYPE="${2:-release}"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

if ! command -v brew >/dev/null 2>&1; then
  echo "Homebrew is required (https://brew.sh/)"
  exit 1
fi

eval "$(brew shellenv)"

if [ "$QT_MAJOR" = "6" ]; then
  brew list qt >/dev/null 2>&1 || brew list qt@6 >/dev/null 2>&1 || brew install qt
  QM="$(brew --prefix qt 2>/dev/null)/bin/qmake"
  if [ ! -x "$QM" ]; then
    QM="$(brew --prefix qt@6 2>/dev/null)/bin/qmake"
  fi
else
  brew list qt@5 >/dev/null 2>&1 || brew install qt@5
  QM="$(brew --prefix qt@5)/bin/qmake"
fi

if [ ! -x "$QM" ]; then
  echo "qmake not found for Qt${QT_MAJOR} (looked for: $QM)"
  exit 1
fi

CONFIG_ARGS=(OpenRGB3DSpatialPlugin.pro)
if [ "$BUILD_TYPE" = "release" ]; then
  CONFIG_ARGS+=(CONFIG+=release)
fi

"$QM" -v
"$QM" "${CONFIG_ARGS[@]}"
make -j"$(sysctl -n hw.ncpu)"

DYLIB=""
for candidate in \
  libOpenRGB3DSpatialPlugin.dylib \
  release/libOpenRGB3DSpatialPlugin.dylib \
  debug/libOpenRGB3DSpatialPlugin.dylib
do
  if [ -f "$candidate" ]; then
    DYLIB="$candidate"
    break
  fi
done

if [ -z "$DYLIB" ]; then
  echo "Expected libOpenRGB3DSpatialPlugin.dylib after build"
  find . -name 'libOpenRGB3DSpatialPlugin.dylib' 2>/dev/null || true
  exit 1
fi

# Normalize to repo root for packaging.
if [ "$DYLIB" != "libOpenRGB3DSpatialPlugin.dylib" ]; then
  cp -f "$DYLIB" libOpenRGB3DSpatialPlugin.dylib
fi

echo "Built: $ROOT/libOpenRGB3DSpatialPlugin.dylib"
