#!/usr/bin/env bash
# Package macOS plugin dylib as a zip under dist/.
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

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
cp "$DYLIB" "$STAGE/$DYLIB"

cat >"$STAGE/INSTALL.txt" <<EOF
OpenRGB 3D Spatial Plugin — macOS install
=========================================
Copy ${DYLIB} into the plugins folder next to OpenRGB
(OpenRGB.app/Contents/MacOS/plugins/ or the path shown in
OpenRGB Settings → Plugins).

Requires a Qt6 build of OpenRGB.
Restart OpenRGB after installing the plugin.
EOF

mkdir -p dist
ZIP_PATH="dist/OpenRGB3DSpatialPlugin_${VARIANT}.zip"
rm -f "$ZIP_PATH"
(
  cd "$STAGE"
  zip -r "$ROOT/$ZIP_PATH" .
)

echo "Packaged: $ROOT/$ZIP_PATH"
