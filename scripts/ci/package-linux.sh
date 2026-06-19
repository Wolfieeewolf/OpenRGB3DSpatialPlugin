#!/usr/bin/env bash
# Package Linux plugin as .deb and .tar.gz under dist/.
# Usage: package-linux.sh <5|6> [amd64|arm64|...]
set -euo pipefail

QT_MAJOR="${1:?Qt major version required (5 or 6)}"
DEB_ARCH="${2:-amd64}"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

VERSION="$("$ROOT/scripts/ci/plugin-version.sh")"
PKG_BASE="openrgb-3d-spatial-plugin"
PKG_NAME="$PKG_BASE"
VARIANT="amd64"
if [ "$QT_MAJOR" = "6" ]; then
  PKG_NAME="${PKG_BASE}-qt6"
  VARIANT="amd64_Qt6"
fi

mkdir -p dist

# .deb
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
mkdir -p "$STAGE/usr/lib/openrgb/plugins"
cp libOpenRGB3DSpatialPlugin.so "$STAGE/usr/lib/openrgb/plugins/"
mkdir -p "$STAGE/DEBIAN"
cat >"$STAGE/DEBIAN/control" <<EOF
Package: ${PKG_NAME}
Version: ${VERSION}
Architecture: ${DEB_ARCH}
Maintainer: OpenRGB3DSpatialPlugin CI <noreply@openrgb.org>
Description: OpenRGB 3D Spatial LED Control plugin
Depends: openrgb
Section: misc
Priority: optional
EOF
if [ "$QT_MAJOR" = "6" ]; then
  sed -i 's/Depends: openrgb/Depends: openrgb-qt6 | openrgb/' "$STAGE/DEBIAN/control" 2>/dev/null || \
    sed -i '' 's/Depends: openrgb/Depends: openrgb-qt6 | openrgb/' "$STAGE/DEBIAN/control"
fi

DEB_FILE="dist/${PKG_NAME}_${VERSION}_${DEB_ARCH}.deb"
dpkg-deb --build "$STAGE" "$DEB_FILE"

# Portable tarball (manual install to PREFIX/lib/openrgb/plugins/)
TAR_ROOT="$(mktemp -d)"
trap 'rm -rf "$STAGE" "$TAR_ROOT"' EXIT
mkdir -p "$TAR_ROOT/usr/lib/openrgb/plugins"
cp libOpenRGB3DSpatialPlugin.so "$TAR_ROOT/usr/lib/openrgb/plugins/"
cat >"$TAR_ROOT/INSTALL.txt" <<'EOF'
OpenRGB 3D Spatial Plugin — Linux manual install
================================================
Extract and copy libOpenRGB3DSpatialPlugin.so to your OpenRGB plugins directory,
typically /usr/lib/openrgb/plugins/ (system packages) or the plugins folder next
to your OpenRGB AppImage/binary.

Restart OpenRGB after installing the plugin.
EOF

TAR_FILE="dist/OpenRGB3DSpatialPlugin_Linux_${VARIANT}.tar.gz"
tar -czf "$TAR_FILE" -C "$TAR_ROOT" .

echo "Packaged: $DEB_FILE"
echo "Packaged: $TAR_FILE"
