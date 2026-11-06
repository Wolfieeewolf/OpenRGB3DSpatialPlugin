#!/usr/bin/env bash
# Build OpenRGB3DSpatialPlugin on macOS.
# Prefers qmake from PATH (jurplel/install-qt-action / aqt). Falls back to Homebrew.
# Usage: build-macos.sh [6] [release|debug]
set -euo pipefail

QT_MAJOR="${1:-6}"
BUILD_TYPE="${2:-release}"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

if [ "$QT_MAJOR" != "6" ]; then
  echo "Only Qt6 builds are supported (got Qt${QT_MAJOR})"
  exit 1
fi

resolve_qmake() {
  if command -v qmake >/dev/null 2>&1; then
    command -v qmake
    return 0
  fi
  if command -v brew >/dev/null 2>&1; then
    eval "$(brew shellenv)"
    local p
    for p in "$(brew --prefix qt 2>/dev/null)/bin/qmake" \
             "$(brew --prefix qt@6 2>/dev/null)/bin/qmake" \
             /usr/local/opt/qt/bin/qmake \
             /usr/local/opt/qt@6/bin/qmake \
             /opt/homebrew/opt/qt/bin/qmake \
             /opt/homebrew/opt/qt@6/bin/qmake
    do
      if [ -n "$p" ] && [ -x "$p" ]; then
        echo "$p"
        return 0
      fi
    done
    brew list qt >/dev/null 2>&1 || brew list qt@6 >/dev/null 2>&1 || brew install qt@6
    for p in "$(brew --prefix qt 2>/dev/null)/bin/qmake" \
             "$(brew --prefix qt@6 2>/dev/null)/bin/qmake"
    do
      if [ -n "$p" ] && [ -x "$p" ]; then
        echo "$p"
        return 0
      fi
    done
  fi
  return 1
}

QM="$(resolve_qmake)" || {
  echo "qmake not found. Install Qt 6 (CI: jurplel/install-qt-action) or: brew install qt@6"
  exit 1
}

if ! command -v cmake >/dev/null 2>&1; then
  if command -v brew >/dev/null 2>&1; then
    eval "$(brew shellenv)"
    brew install cmake
  else
    echo "cmake not found (needed for Assimp)"
    exit 1
  fi
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

if [ "$DYLIB" != "libOpenRGB3DSpatialPlugin.dylib" ]; then
  cp -f "$DYLIB" libOpenRGB3DSpatialPlugin.dylib
fi

echo "Built: $ROOT/libOpenRGB3DSpatialPlugin.dylib"
