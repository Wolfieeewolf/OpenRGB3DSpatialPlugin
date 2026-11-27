#!/usr/bin/env bash
# Optional local rebuild against system Qt 6, then copy into
# ~/.config/OpenRGB/plugins. Prefer a matching GitHub/GitLab release .so when
# one exists (AppImage / Debian pipeline). Distro OpenRGB (CachyOS/Arch/Fedora)
# usually needs this rebuild so Qt matches Information → Software Info.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${OPENRGB_PLUGIN_DIR:-$HOME/.config/OpenRGB/plugins}"

if [ ! -d "$ROOT/OpenRGB/qt" ]; then
  echo "OpenRGB submodule is empty. Run: git submodule update --init --depth 1 OpenRGB"
  exit 1
fi

cd "$ROOT"
DEPS_HINT="CachyOS/Arch: sudo pacman -S --needed base-devel git cmake qt6-base qt6-tools mesa glu libpipewire
Debian/Ubuntu: sudo apt install build-essential git cmake qt6-base-dev qt6-tools-dev libgl-dev libglu1-mesa-dev libpipewire-0.3-dev
Fedora: sudo dnf install gcc-c++ make git cmake qt6-qtbase-devel qt6-qttools-devel mesa-libGLU-devel pipewire-devel"
QM="$(command -v qmake6 2>/dev/null || command -v qmake-qt6 2>/dev/null || true)"
if [ -z "$QM" ]; then
  echo "qmake6 not found. Install the build tools first:"
  echo "$DEPS_HINT"
  exit 1
fi
if ! command -v cmake >/dev/null 2>&1; then
  echo "cmake not found (needed to build the 3D model library, Assimp). Install the build tools first:"
  echo "$DEPS_HINT"
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
