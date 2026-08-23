#!/usr/bin/env bash
# Package Linux plugin as .deb and .tar.gz under dist/.
# Usage: package-linux.sh [6] [amd64|arm64|...] — arch names the artifacts.
set -euo pipefail

QT_MAJOR="${1:-6}"
DEB_ARCH="${2:-amd64}"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

VERSION="$("$ROOT/scripts/ci/plugin-version.sh")"
PKG_BASE="openrgb-3d-spatial-plugin"
PKG_NAME="$PKG_BASE"
VARIANT="$DEB_ARCH"

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
Description: OpenRGB 3D Spatial LED Control plugin (Qt6)
Depends: openrgb-qt6 | openrgb
Section: misc
Priority: optional
EOF

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

Requires a Qt6 build of OpenRGB.
Restart OpenRGB after installing the plugin.
EOF

# OpenRGB CI uses *_deb* for .deb jobs and a plain Linux_* name for the .so artifact.
# Name the portable archive with _plugin so it is not mistaken for the .deb.
TAR_FILE="dist/OpenRGB3DSpatialPlugin_Linux_${VARIANT}_plugin.tar.gz"
tar -czf "$TAR_FILE" -C "$TAR_ROOT" .

echo "Packaged: $DEB_FILE"
echo "Packaged: $TAR_FILE"
