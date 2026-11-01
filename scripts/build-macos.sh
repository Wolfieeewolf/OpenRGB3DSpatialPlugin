#!/usr/bin/env bash
# macOS pipeline build (OpenRGBEffectsPlugin layout). Qt6 only.
# Usage: ./scripts/build-macos.sh qt6 arm|intel
set -euo pipefail
cd "$(dirname "$0")/.."

if [ "${1:-}" != "qt6" ]; then
  echo "Only Qt6 pipeline builds are supported. Example: ./scripts/build-macos.sh qt6 arm"
  exit 1
fi

if [ "${2:-}" = "arm" ]; then
  ARCH=""
  BREW_PATH=/opt/homebrew
elif [ "${2:-}" = "intel" ]; then
  ARCH="arch -x86_64"
  BREW_PATH=/usr/local
else
  echo "Specify arm or intel. Example: ./scripts/build-macos.sh qt6 arm"
  exit 1
fi

eval "$("$BREW_PATH/bin/brew" shellenv)"
# shellcheck disable=SC2086
$ARCH "$BREW_PATH/bin/qmake" OpenRGB3DSpatialPlugin.pro CONFIG+=release
# shellcheck disable=SC2086
$ARCH make -j"$(sysctl -n hw.ncpu)"

rewrite() {
  local framework="$1"
  install_name_tool -change \
    "$BREW_PATH/opt/qtbase/lib/${framework}.framework/Versions/A/${framework}" \
    "@executable_path/../Frameworks/${framework}.framework/Versions/A/${framework}" \
    libOpenRGB3DSpatialPlugin.dylib 2>/dev/null || true
}

rewrite QtCore
rewrite QtGui
rewrite QtOpenGL
rewrite QtOpenGLWidgets
rewrite QtWidgets

if [ -n "${CODESIGN_IDENTITY:-}" ]; then
  # shellcheck disable=SC2086
  $ARCH codesign --force --verify -s "$CODESIGN_IDENTITY" libOpenRGB3DSpatialPlugin.dylib
elif security find-identity -v -p codesigning 2>/dev/null | grep -q "OpenRGB"; then
  # shellcheck disable=SC2086
  $ARCH codesign --force --verify -s OpenRGB libOpenRGB3DSpatialPlugin.dylib
fi
