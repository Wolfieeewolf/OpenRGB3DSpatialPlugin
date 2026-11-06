#!/usr/bin/env bash
# Interactive installer for Linux / macOS release packages.
# Same choices as the Windows MSI: Typical / Complete / Custom (pick features).
#
# From an extracted release archive:
#   ./install.sh                 # menu (recommended)
#   ./install.sh typical
#   ./install.sh complete
#   ./install.sh custom
#
# Env:
#   OPENRGB_CONFIG     — OpenRGB config dir
#   OPENRGB_PLUGINS    — plugins dir for the .so/.dylib (optional override)
#   SKIP_PLUGIN=1      — content only
#   SKIP_CONTENT=1     — plugin binary only
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONTENT_SH="${SCRIPT_DIR}/install-openrgb3d-content.sh"
CONTENT_ROOT="${CONTENT_ROOT:-$SCRIPT_DIR/content}"

die() { echo "error: $*" >&2; exit 1; }

have_content() {
  [[ -d "$CONTENT_ROOT" && -f "$CONTENT_ROOT/.features.tsv" && -x "$CONTENT_SH" ]] \
    || [[ -d "$CONTENT_ROOT" && -f "$CONTENT_ROOT/.features.tsv" && -f "$CONTENT_SH" ]]
}

find_plugin_binary() {
  local f
  for f in \
    "$SCRIPT_DIR/libOpenRGB3DSpatialPlugin.so" \
    "$SCRIPT_DIR/libOpenRGB3DSpatialPlugin.dylib" \
    "$SCRIPT_DIR/usr/lib/openrgb/plugins/libOpenRGB3DSpatialPlugin.so"
  do
    if [[ -f "$f" ]]; then
      echo "$f"
      return 0
    fi
  done
  return 1
}

default_plugins_dir() {
  if [[ -n "${OPENRGB_PLUGINS:-}" ]]; then
    echo "$OPENRGB_PLUGINS"
    return
  fi
  case "$(uname -s)" in
    Darwin)
      local app="$HOME/Applications/OpenRGB.app/Contents/MacOS/plugins"
      if [[ -d "$app" ]]; then
        echo "$app"
        return
      fi
      if [[ -d /Applications/OpenRGB.app/Contents/MacOS/plugins ]]; then
        echo /Applications/OpenRGB.app/Contents/MacOS/plugins
        return
      fi
      echo "$HOME/.config/OpenRGB/plugins"
      ;;
    *)
      if [[ -d /usr/lib/openrgb/plugins ]]; then
        echo /usr/lib/openrgb/plugins
        return
      fi
      echo "$HOME/.config/OpenRGB/plugins"
      ;;
  esac
}

prompt_yn() {
  local q="$1"
  local def="${2:-y}"
  local ans
  printf "%s [%s] " "$q" "$def"
  read -r ans || true
  ans="${ans:-$def}"
  case "$ans" in
    y|Y|yes|YES) return 0 ;;
    *) return 1 ;;
  esac
}

install_plugin_binary() {
  local src dest
  src="$(find_plugin_binary)" || {
    echo "No plugin .so/.dylib in this package — skipping binary install."
    return 0
  }
  dest="$(default_plugins_dir)"
  echo
  echo "Plugin binary: $(basename "$src")"
  echo "Default plugins folder: $dest"
  if ! prompt_yn "Install plugin binary there?" y; then
    printf "Plugins folder path (empty = skip): "
    read -r dest || true
    [[ -z "${dest:-}" ]] && { echo "Skipped plugin binary."; return 0; }
  fi
  mkdir -p "$dest"
  cp -f "$src" "$dest/"
  echo "Installed → $dest/$(basename "$src")"
}

choose_setup_type() {
  cat <<EOF

OpenRGB 3D Spatial — install options
====================================
  1) Typical   — looks + Action tools + Triggers + Movie Sync
                 (no controller layouts)
  2) Complete  — Typical + every controller layout category
  3) Custom    — pick each pack / controller brand on or off

EOF
  local choice
  while true; do
    printf "Setup type [1/2/3]: "
    read -r choice || true
    case "${choice:-}" in
      1|typical|Typical) echo typical; return ;;
      2|complete|Complete) echo complete; return ;;
      3|custom|Custom) echo custom; return ;;
      *) echo "Enter 1, 2, or 3." ;;
    esac
  done
}

usage() {
  cat <<EOF
Interactive installer (same options as the Windows MSI).

Usage:
  $(basename "$0")              # show Typical / Complete / Custom menu
  $(basename "$0") typical
  $(basename "$0") complete
  $(basename "$0") custom       # toggle checklist for each feature
  $(basename "$0") --help

Also installs the plugin .so/.dylib when present in the package
(unless SKIP_PLUGIN=1). Content-only: SKIP_PLUGIN=1.
EOF
}

MODE="${1:-}"
case "$MODE" in
  -h|--help) usage; exit 0 ;;
esac

echo "OpenRGB 3D Spatial Plugin installer"
echo "Package: $SCRIPT_DIR"

if [[ "${SKIP_PLUGIN:-0}" != "1" ]]; then
  if find_plugin_binary >/dev/null; then
    install_plugin_binary
  fi
fi

if [[ "${SKIP_CONTENT:-0}" == "1" ]]; then
  echo "SKIP_CONTENT=1 — done."
  exit 0
fi

have_content || die "content pack missing (expected $CONTENT_ROOT). Re-download a full release package."

if [[ -z "$MODE" ]]; then
  MODE="$(choose_setup_type)"
fi

case "$MODE" in
  typical|complete|custom) ;;
  *) die "unknown mode '$MODE' (typical|complete|custom)" ;;
esac

chmod +x "$CONTENT_SH" 2>/dev/null || true
CONTENT_ROOT="$CONTENT_ROOT" bash "$CONTENT_SH" "$MODE"

echo
echo "Restart OpenRGB after install."
