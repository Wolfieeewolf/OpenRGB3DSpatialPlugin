#!/usr/bin/env bash
# Print plugin version for packaging (git tag, else 0.0.0).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

describe="$(git describe --tags --always 2>/dev/null || true)"
if [ -n "$describe" ]; then
  tag="${describe#v}"
  tag="${tag%%-*}"
  if [[ "$tag" == *.* ]]; then
    echo "$tag"
    exit 0
  fi
fi

echo "0.0.0"
