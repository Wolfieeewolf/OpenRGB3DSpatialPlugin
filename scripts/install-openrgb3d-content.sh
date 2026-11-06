#!/usr/bin/env bash
# Install OpenRGB3DSpatialPresets content into the plugin data root.
# Mirrors Windows MSI modes: typical | complete | custom (pick features).
#
# Usage (from a release package that contains ./content/):
#   ./install-openrgb3d-content.sh              # Typical / Complete / Custom menu
#   ./install-openrgb3d-content.sh typical
#   ./install-openrgb3d-content.sh complete
#   ./install-openrgb3d-content.sh custom       # toggle checklist
#   ./install-openrgb3d-content.sh --features effects-volume,controllers-fans
#
# Env:
#   OPENRGB_CONFIG   — OpenRGB config dir (default: $XDG_CONFIG_HOME/OpenRGB or ~/.config/OpenRGB)
#   CONTENT_ROOT     — staged content dir (default: ./content next to this script)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONTENT_ROOT="${CONTENT_ROOT:-$SCRIPT_DIR/content}"
FEATURES_TSV="${CONTENT_ROOT}/.features.tsv"

die() { echo "error: $*" >&2; exit 1; }

resolve_data_root() {
  local cfg="${OPENRGB_CONFIG:-}"
  if [[ -z "$cfg" ]]; then
    if [[ -n "${XDG_CONFIG_HOME:-}" ]]; then
      cfg="$XDG_CONFIG_HOME/OpenRGB"
    elif [[ -n "${HOME:-}" ]]; then
      cfg="$HOME/.config/OpenRGB"
    else
      die "cannot resolve OpenRGB config (set OPENRGB_CONFIG or HOME)"
    fi
  fi
  echo "$cfg/plugins/settings/OpenRGB3DSpatialPlugin"
}

usage() {
  cat <<EOF
Install OpenRGB 3D Spatial content pack (looks, tools, optional controllers).

Usage:
  $(basename "$0")                      # Typical / Complete / Custom menu
  $(basename "$0") typical              # effects + Action tools + Triggers + Movie Sync
  $(basename "$0") complete             # typical + every controller layout category
  $(basename "$0") custom               # interactive on/off checklist
  $(basename "$0") --features id,id     # explicit feature ids from .features.tsv

Data root: $(resolve_data_root)
Content:   $CONTENT_ROOT
EOF
}

[[ -d "$CONTENT_ROOT" ]] || die "content folder not found: $CONTENT_ROOT (shipped next to this script in release packages)"
[[ -f "$FEATURES_TSV" ]] || die "missing $FEATURES_TSV"

declare -a FEAT_IDS=()
declare -a FEAT_LEVELS=()
declare -a FEAT_TITLES=()
declare -a FEAT_PATHS=()

while IFS=$'\t' read -r id level title path || [[ -n "${id:-}" ]]; do
  [[ -z "${id:-}" || "$id" =~ ^# ]] && continue
  [[ "$id" == "id" ]] && continue
  # Strip CR from Windows-checked-out TSV
  id="${id%$'\r'}"
  level="${level%$'\r'}"
  title="${title%$'\r'}"
  path="${path%$'\r'}"
  FEAT_IDS+=("$id")
  FEAT_LEVELS+=("$level")
  FEAT_TITLES+=("$title")
  FEAT_PATHS+=("$path")
done < "$FEATURES_TSV"

[[ "${#FEAT_IDS[@]}" -gt 0 ]] || die "no features listed in $FEATURES_TSV"

install_feature() {
  local idx="$1"
  local path="${FEAT_PATHS[$idx]}"
  local src="$CONTENT_ROOT/$path"
  local dst="$DATA_ROOT/$path"
  if [[ ! -d "$src" ]]; then
    echo "  skip ${FEAT_IDS[$idx]} (not in package)"
    return 0
  fi
  mkdir -p "$dst"
  cp -a "$src"/. "$dst"/
  echo "  + ${FEAT_TITLES[$idx]} (${FEAT_IDS[$idx]})"
}

select_by_max_level() {
  local max="$1"
  local i
  SELECTED=()
  for i in "${!FEAT_IDS[@]}"; do
    if [[ "${FEAT_LEVELS[$i]}" -le "$max" ]]; then
      SELECTED+=("$i")
    fi
  done
}

choose_setup_type() {
  cat <<EOF

Content install options (same as Windows MSI)
---------------------------------------------
  1) Typical   — looks + Action tools + Triggers + Movie Sync
                 (controller layouts OFF)
  2) Complete  — Typical + every controller layout
  3) Custom    — turn each pack / controller brand on or off

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

# Custom: show checklist, toggle by number, Enter confirms.
select_custom_interactive() {
  declare -a ON=()
  local i
  for i in "${!FEAT_IDS[@]}"; do
    if [[ "${FEAT_LEVELS[$i]}" -le 1 ]]; then
      ON[$i]=1
    else
      ON[$i]=0
    fi
  done

  while true; do
    echo
    echo "Custom — enter a number to toggle, A=all, N=none, Enter=install"
    echo
    local n=1
    for i in "${!FEAT_IDS[@]}"; do
      local mark=" "
      [[ "${ON[$i]}" -eq 1 ]] && mark="x"
      local tag=""
      [[ "${FEAT_LEVELS[$i]}" -gt 1 ]] && tag=" (controller)"
      printf "  %2d) [%s] %s%s\n" "$n" "$mark" "${FEAT_TITLES[$i]}" "$tag"
      n=$((n + 1))
    done
    echo
    printf "Toggle # / A / N / Enter: "
    local ans
    read -r ans || true
    if [[ -z "${ans:-}" ]]; then
      break
    fi
    case "$ans" in
      [Aa]|all|ALL)
        for i in "${!FEAT_IDS[@]}"; do ON[$i]=1; done
        ;;
      [Nn]|none|NONE)
        for i in "${!FEAT_IDS[@]}"; do ON[$i]=0; done
        ;;
      *)
        if [[ "$ans" =~ ^[0-9]+$ ]]; then
          local idx=$((ans - 1))
          if [[ "$idx" -ge 0 && "$idx" -lt "${#FEAT_IDS[@]}" ]]; then
            if [[ "${ON[$idx]}" -eq 1 ]]; then
              ON[$idx]=0
            else
              ON[$idx]=1
            fi
          else
            echo "Out of range."
          fi
        else
          echo "Enter a feature number, A, N, or press Enter."
        fi
        ;;
    esac
  done

  SELECTED=()
  for i in "${!FEAT_IDS[@]}"; do
    if [[ "${ON[$i]}" -eq 1 ]]; then
      SELECTED+=("$i")
    fi
  done
}

select_by_ids() {
  local csv="$1"
  SELECTED=()
  local want id i
  IFS=',' read -r -a want <<<"$csv"
  for id in "${want[@]}"; do
    id="$(echo "$id" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
    [[ -z "$id" ]] && continue
    local found=0
    for i in "${!FEAT_IDS[@]}"; do
      if [[ "${FEAT_IDS[$i]}" == "$id" ]]; then
        SELECTED+=("$i")
        found=1
        break
      fi
    done
    [[ "$found" -eq 1 ]] || die "unknown feature id: $id"
  done
}

MODE="${1:-}"
case "$MODE" in
  -h|--help) usage; exit 0 ;;
esac

DATA_ROOT="$(resolve_data_root)"
declare -a SELECTED=()

if [[ -z "$MODE" ]]; then
  MODE="$(choose_setup_type)"
fi

case "$MODE" in
  typical) select_by_max_level 1 ;;
  complete) select_by_max_level 100 ;;
  custom) select_custom_interactive ;;
  --features)
    [[ -n "${2:-}" ]] || die "--features needs a comma-separated id list"
    select_by_ids "$2"
    ;;
  *)
    die "unknown mode '$MODE' (use typical|complete|custom|--features, or no args for menu)"
    ;;
esac

[[ "${#SELECTED[@]}" -gt 0 ]] || die "nothing selected — run again and leave at least one feature on"

echo
echo "Will install ${#SELECTED[@]} feature(s) → $DATA_ROOT"
echo "Selected:"
for i in "${SELECTED[@]}"; do
  echo "  - ${FEAT_TITLES[$i]}"
done
echo
printf "Proceed? [Y/n] "
read -r go || true
go="${go:-y}"
case "$go" in
  y|Y|yes|YES) ;;
  *) echo "Cancelled."; exit 1 ;;
esac

echo "Installing content → $DATA_ROOT"
mkdir -p "$DATA_ROOT"
for i in "${SELECTED[@]}"; do
  install_feature "$i"
done
echo "Done. Restart OpenRGB (or reload the plugin) so looks rescan."
