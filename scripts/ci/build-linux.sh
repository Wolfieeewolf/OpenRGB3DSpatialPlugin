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

# Explicit QT_BIN_DIR (CI: Qt 6.8.3 from install-qt-action) wins over distro qmake6.
QM=""
if [ -n "${QT_BIN_DIR:-}" ] && [ -x "${QT_BIN_DIR}/qmake" ]; then
  QM="${QT_BIN_DIR}/qmake"
fi
if [ -z "$QM" ]; then
  QM="$(command -v qmake6 2>/dev/null || command -v qmake-qt6 2>/dev/null || true)"
fi
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

# Release builds: look for Qt libraries OpenRGB does not ship next to the plugin
# (the default RUNPATH is a directory that only exists on the build machine).
# Local distro builds (install-linux-user.sh) keep using the system Qt.
if command -v patchelf >/dev/null 2>&1; then
  patchelf --set-rpath '$ORIGIN' libOpenRGB3DSpatialPlugin.so
  patchelf --print-rpath libOpenRGB3DSpatialPlugin.so
fi
