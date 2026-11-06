#!/usr/bin/env bash
# Package macOS plugin dylib as a zip under dist/.
# When PRESETS_ROOT (or sibling/checkout OpenRGB3DSpatialPresets) is available,
# embeds the content pack + install-openrgb3d-content.sh (Typical/Custom/Complete).
# Usage: package-macos.sh [6] [arm64|x86_64]
# Env: DIST_VARIANT (optional override)
set -euo pipefail

QT_MAJOR="${1:-6}"
ARCH="${2:-$(uname -m)}"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

DYLIB="libOpenRGB3DSpatialPlugin.dylib"
if [ ! -f "$DYLIB" ]; then
  echo "Build output not found: $DYLIB"
  exit 1
fi

# Point Qt framework deps at OpenRGB.app's bundled Frameworks (same pattern as Effects plugin).
while IFS= read -r line; do
  old="$(awk '{print $1}' <<<"$line")"
  case "$old" in
    */Qt*.framework/*)
      fw="$(sed -n 's|.*/\(Qt[^/]*\)\.framework/.*|\1|p' <<<"$old")"
      rest="$(sed -n "s|.*${fw}\\.framework/||p" <<<"$old")"
      if [ -n "$fw" ] && [ -n "$rest" ]; then
        new="@executable_path/../Frameworks/${fw}.framework/${rest}"
        echo "install_name_tool: $old -> $new"
        install_name_tool -change "$old" "$new" "$DYLIB" || true
      fi
      ;;
  esac
done < <(otool -L "$DYLIB" | tail -n +2)

install_name_tool -id "@rpath/${DYLIB}" "$DYLIB" || true

if [ -n "${DIST_VARIANT:-}" ]; then
  VARIANT="$DIST_VARIANT"
else
  VARIANT="macOS_${ARCH}"
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

STAGE="$(mktemp -d)"
CONTENT_STAGE=""
cleanup() { rm -rf "$STAGE" ${CONTENT_STAGE:+"$CONTENT_STAGE"}; }
trap cleanup EXIT
cp "$DYLIB" "$STAGE/$DYLIB"

HAS_CONTENT=0
if PRESETS="$(resolve_presets_root)"; then
  echo "Content pack: $PRESETS"
  CONTENT_STAGE="$(mktemp -d)"
  chmod +x "$ROOT/scripts/ci/stage-content-pack.sh"
  bash "$ROOT/scripts/ci/stage-content-pack.sh" "$PRESETS" "$CONTENT_STAGE"
  cp -a "$CONTENT_STAGE" "$STAGE/content"
  install -m 0755 "$ROOT/scripts/install-openrgb3d-content.sh" "$STAGE/install-openrgb3d-content.sh"
  install -m 0755 "$ROOT/scripts/install-openrgb3d.sh" "$STAGE/install.sh"
  HAS_CONTENT=1
else
  echo "warning: presets pack not found — packaging dylib only (set PRESETS_ROOT)" >&2
fi

{
  cat <<EOF
OpenRGB 3D Spatial Plugin — macOS install
=========================================
Recommended (plugin + content with install options):

  ./install.sh

Menu matches the Windows MSI:
  1) Typical  — looks + tools (no controllers)
  2) Complete — + every controller layout
  3) Custom   — toggle each pack / controller brand on or off

Non-interactive:
  ./install.sh typical|complete|custom

Manual: copy ${DYLIB} into OpenRGB.app/Contents/MacOS/plugins/ (or the path
in OpenRGB Settings → Plugins). Content uses ~/.config/OpenRGB/… (same as Linux).

Requires a Qt6 build of OpenRGB. Restart OpenRGB after installing.
EOF
} >"$STAGE/INSTALL.txt"

mkdir -p dist
ZIP_PATH="dist/OpenRGB3DSpatialPlugin_${VARIANT}.zip"
rm -f "$ZIP_PATH"
(
  cd "$STAGE"
  zip -r "$ROOT/$ZIP_PATH" .
)

echo "Packaged: $ROOT/$ZIP_PATH"
