#!/usr/bin/env bash
# Stage OpenRGB3DSpatialPresets into DEST for packaging (full pack; installers filter).
# Usage: stage-content-pack.sh <presets_root> <dest_dir>
# Env: FEATURES_TSV (optional override of scripts/ci/content-pack-features.tsv)
set -euo pipefail

PRESETS_ROOT="${1:?presets_root required}"
DEST="${2:?dest_dir required}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
FEATURES_TSV="${FEATURES_TSV:-$ROOT/scripts/ci/content-pack-features.tsv}"

if [[ ! -d "$PRESETS_ROOT" ]]; then
  echo "Presets root not found: $PRESETS_ROOT" >&2
  exit 1
fi
if [[ ! -f "$FEATURES_TSV" ]]; then
  echo "Features TSV not found: $FEATURES_TSV" >&2
  exit 1
fi

rm -rf "$DEST"
mkdir -p "$DEST"

# Skip authoring / docs noise (same idea as Windows MSI staging).
should_skip() {
  local rel="$1"
  case "$rel" in
    *.md|*.py|*.pyc|*.ps1|*.sh|*.bat|.gitignore|.gitattributes) return 0 ;;
  esac
  case "/$rel/" in
    */.git/*|*/.github/*|*/.wiki-seed/*|*/docs/*|*/scripts/*|*/__pycache__/*) return 0 ;;
  esac
  local base
  base="$(basename "$rel")"
  case "$base" in
    LICENSE|CONTRIBUTING.md|template.controller.json) return 0 ;;
  esac
  return 1
}

copy_tree() {
  local src="$1"
  local dst="$2"
  [[ -d "$src" ]] || return 1
  local count=0
  while IFS= read -r -d '' f; do
    local rel="${f#"$src"/}"
    if should_skip "$rel"; then
      continue
    fi
    mkdir -p "$(dirname "$dst/$rel")"
    cp -a "$f" "$dst/$rel"
    count=$((count + 1))
  done < <(find "$src" -type f -print0)
  [[ "$count" -gt 0 ]]
}

copied=0
while IFS=$'\t' read -r id level title path || [[ -n "${id:-}" ]]; do
  [[ -z "${id:-}" || "$id" =~ ^# ]] && continue
  [[ "$id" == "id" ]] && continue
  src="$PRESETS_ROOT/$path"
  dst="$DEST/$path"
  if copy_tree "$src" "$dst"; then
    copied=$((copied + 1))
  else
    echo "warning: skipping empty/missing feature $id ($path)" >&2
  fi
done < "$FEATURES_TSV"

cp "$FEATURES_TSV" "$DEST/.features.tsv"

if [[ "$copied" -lt 1 ]]; then
  echo "No content staged from $PRESETS_ROOT" >&2
  exit 1
fi

echo "Staged $copied feature trees into $DEST"
