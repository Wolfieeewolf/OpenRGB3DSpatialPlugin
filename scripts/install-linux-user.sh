#!/usr/bin/env bash
# Build the plugin with the system Qt 6 toolchain and copy it into the
# per-user OpenRGB plugins folder (CachyOS / Arch / Fedora native OpenRGB).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${OPENRGB_PLUGIN_DIR:-$HOME/.config/OpenRGB/plugins}"

if [ ! -d "$ROOT/OpenRGB/qt" ]; then
  echo "OpenRGB submodule is empty. Run: git submodule update --init --depth 1 OpenRGB"
  exit 1
fi

cd "$ROOT"
QM="$(command -v qmake6 2>/dev/null || command -v qmake-qt6 2>/dev/null || true)"
if [ -z "$QM" ]; then
  echo "qmake6 not found. On CachyOS: pacman -S qt6-base qt6-tools mesa glu"
  exit 1
fi

"$QM" -v
"$QM" OpenRGB3DSpatialPlugin.pro PREFIX=/usr CONFIG+=release
make -j"$(nproc)"

if [ ! -f libOpenRGB3DSpatialPlugin.so ]; then
  echo "Build did not produce libOpenRGB3DSpatialPlugin.so"
  exit 1
fi

mkdir -p "$DEST"
cp -f libOpenRGB3DSpatialPlugin.so "$DEST/"
echo "Installed $DEST/libOpenRGB3DSpatialPlugin.so"
echo "Restart OpenRGB. Qt of this plugin must match Information → Software Info."
