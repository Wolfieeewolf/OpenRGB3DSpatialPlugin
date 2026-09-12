#!/usr/bin/env bash
# Linux pipeline build (same entry as OpenRGBEffectsPlugin).
# Usage: ./scripts/build-plugin.sh qt6
set -euo pipefail
cd "$(dirname "$0")/.."

if [ "${1:-qt6}" != "qt6" ] && [ "${1:-}" != "Qt6" ]; then
  echo "Only Qt6 pipeline builds are supported (OpenRGB 1.0 / Plugin API 5)."
  echo "Qt5 OpenRGB hosts are not a target for this plugin."
  exit 1
fi

export QT_SELECT=qt6
QMAKE="$(command -v qmake6 || command -v qmake-qt6 || command -v qmake)"
"$QMAKE" OpenRGB3DSpatialPlugin.pro PREFIX=/usr CONFIG+=release CONFIG-=debug_and_release
make -j"$(nproc)"
