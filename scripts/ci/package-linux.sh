#!/usr/bin/env bash
# Package Linux plugin as .deb and .tar.gz under dist/.
# When PRESETS_ROOT (or sibling/checkout OpenRGB3DSpatialPresets) is available,
# embeds the content pack + install-openrgb3d-content.sh (Typical/Custom/Complete).
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
QT_TAG=""
if [ -n "${QT_AQT_VERSION:-}" ] && [ "${QT_AQT_VERSION}" != "6.8.3" ]; then
  QT_TAG="_Qt${QT_AQT_VERSION}"
fi

resolve_presets_root() {
  if [ -n "${PRESETS_ROOT:-}" ] && [ -d "$PRESETS_ROOT" ]; then
    echo "$PRESETS_ROOT"
    return
  fi
  if [ -d "$ROOT/OpenRGB3DSpatialPresets" ]; then
    echo "$ROOT/OpenRGB3DSpatialPresets"
    return
  fi
  local sibling
  sibling="$(cd "$ROOT/.." && pwd)/OpenRGB3DSpatialPresets"
  if [ -d "$sibling" ]; then
    echo "$sibling"
    return
  fi
  return 1
}

mkdir -p dist

CONTENT_STAGE=""
INSTALL_CONTENT_SH="$ROOT/scripts/install-openrgb3d-content.sh"
INSTALL_SH="$ROOT/scripts/install-openrgb3d.sh"
if PRESETS="$(resolve_presets_root)"; then
  echo "Content pack: $PRESETS"
  CONTENT_STAGE="$(mktemp -d)"
  chmod +x "$ROOT/scripts/ci/stage-content-pack.sh"
  bash "$ROOT/scripts/ci/stage-content-pack.sh" "$PRESETS" "$CONTENT_STAGE"
else
  echo "warning: presets pack not found — packaging plugin binary only (set PRESETS_ROOT)" >&2
fi

# .deb
STAGE="$(mktemp -d)"
TAR_ROOT=""
cleanup() { rm -rf "$STAGE" ${TAR_ROOT:+"$TAR_ROOT"} ${CONTENT_STAGE:+"$CONTENT_STAGE"}; }
trap cleanup EXIT
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

if [ -n "$CONTENT_STAGE" ]; then
  mkdir -p "$STAGE/usr/share/openrgb-3d-spatial-plugin"
  cp -a "$CONTENT_STAGE" "$STAGE/usr/share/openrgb-3d-spatial-plugin/content"
  install -m 0755 "$INSTALL_CONTENT_SH" \
    "$STAGE/usr/share/openrgb-3d-spatial-plugin/install-openrgb3d-content.sh"
  install -m 0755 "$INSTALL_SH" \
    "$STAGE/usr/share/openrgb-3d-spatial-plugin/install-openrgb3d.sh"
  mkdir -p "$STAGE/usr/bin"
  cat >"$STAGE/usr/bin/openrgb3d-install-content" <<'WRAP'
#!/usr/bin/env bash
# Interactive Typical / Complete / Custom (or pass typical|complete|custom).
export CONTENT_ROOT=/usr/share/openrgb-3d-spatial-plugin/content
export SKIP_PLUGIN=1
SHARE=/usr/share/openrgb-3d-spatial-plugin
if [ "$#" -eq 0 ]; then
  exec "$SHARE/install-openrgb3d.sh"
fi
exec "$SHARE/install-openrgb3d-content.sh" "$@"
WRAP
  chmod 0755 "$STAGE/usr/bin/openrgb3d-install-content"
  cat >"$STAGE/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
echo "OpenRGB 3D Spatial: plugin under /usr/lib/openrgb/plugins/"
echo "Content options: run  openrgb3d-install-content"
echo "  (menu: Typical / Complete / Custom — pick packs and controllers)"
echo "  or: openrgb3d-install-content typical|complete|custom"
EOF
  chmod 0755 "$STAGE/DEBIAN/postinst"
fi

DEB_FILE="dist/${PKG_NAME}_${VERSION}_${DEB_ARCH}${QT_TAG}.deb"
dpkg-deb --build "$STAGE" "$DEB_FILE"

# Portable tarball (manual install to PREFIX/lib/openrgb/plugins/)
TAR_ROOT="$(mktemp -d)"
mkdir -p "$TAR_ROOT/usr/lib/openrgb/plugins"
cp libOpenRGB3DSpatialPlugin.so "$TAR_ROOT/usr/lib/openrgb/plugins/"
if [ -n "$CONTENT_STAGE" ]; then
  cp -a "$CONTENT_STAGE" "$TAR_ROOT/content"
  install -m 0755 "$INSTALL_CONTENT_SH" "$TAR_ROOT/install-openrgb3d-content.sh"
  install -m 0755 "$INSTALL_SH" "$TAR_ROOT/install.sh"
fi

{
  cat <<'EOF'
OpenRGB 3D Spatial Plugin — Linux install
========================================
Recommended (plugin + content with install options):

  ./install.sh

Menu matches the Windows MSI:
  1) Typical  — looks + tools (no controllers)
  2) Complete — + every controller layout
  3) Custom   — toggle each pack / controller brand on or off

Non-interactive:
  ./install.sh typical|complete|custom
  ./install-openrgb3d-content.sh …     # content only

Manual plugin copy (if you skip the installer):
  usr/lib/openrgb/plugins/libOpenRGB3DSpatialPlugin.so
  → /usr/lib/openrgb/plugins/ (or your AppImage plugins folder)

Content lands in ~/.config/OpenRGB/plugins/settings/OpenRGB3DSpatialPlugin/
(override with OPENRGB_CONFIG).

Requires OpenRGB 1.0 (Plugin API 5), matching Qt. Flatpak OpenRGB: use the
Flatpak extension — do not load a host .so.

Restart OpenRGB after installing.
EOF
} >"$TAR_ROOT/INSTALL.txt"

# OpenRGB CI uses *_deb* for .deb jobs and a plain Linux_* name for the .so artifact.
# Name the portable archive with _plugin so it is not mistaken for the .deb.
TAR_FILE="dist/OpenRGB3DSpatialPlugin_Linux_${VARIANT}${QT_TAG}_plugin.tar.gz"
tar -czf "$TAR_FILE" -C "$TAR_ROOT" .

echo "Packaged: $DEB_FILE"
echo "Packaged: $TAR_FILE"
