#!/usr/bin/env bash
# Print plugin version for packaging (PROJECT_VERSION or calendar fallback).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
if [ -f "$ROOT/PROJECT_VERSION" ]; then
  head -n1 "$ROOT/PROJECT_VERSION" | tr -d '\r\n'
  exit 0
fi
date -u +%y.%m.%d.1
