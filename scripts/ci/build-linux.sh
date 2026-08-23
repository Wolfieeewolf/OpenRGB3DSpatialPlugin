#!/usr/bin/env bash
# Build OpenRGB3DSpatialPlugin on Linux (Qt6).
# Usage: build-linux.sh [6] [release|debug]
set -euo pipefail

QT_MAJOR="${1:-6}"
BUILD_TYPE="${2:-release}"
PREFIX="${PREFIX:-/usr}"

if [ "$QT_MAJOR" != "6" ]; then
  echo "Only Qt6 builds are supported (got Qt${QT_MAJOR})"
  exit 1
fi

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

CONFIG_ARGS=(OpenRGB3DSpatialPlugin.pro "PREFIX=$PREFIX")
if [ "$BUILD_TYPE" = "release" ]; then
  CONFIG_ARGS+=(CONFIG+=release)
fi

QM="$(command -v qmake6 2>/dev/null || command -v qmake-qt6 2>/dev/null || true)"
if [ -z "$QM" ] && [ -x /usr/lib/qt6/bin/qmake ]; then
  QM=/usr/lib/qt6/bin/qmake
fi
if [ -z "$QM" ]; then
  echo "Qt6 qmake not found"
  exit 1
fi

"$QM" -v
"$QM" "${CONFIG_ARGS[@]}"
make -j"$(nproc)"

if [ ! -f libOpenRGB3DSpatialPlugin.so ]; then
  echo "Expected libOpenRGB3DSpatialPlugin.so after build"
  exit 1
fi
