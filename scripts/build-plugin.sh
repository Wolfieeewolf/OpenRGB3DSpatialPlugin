#!/usr/bin/env bash
# OpenRGB Effects Plugin–style entry point for Linux CI.
# Usage: build-plugin.sh [qt6|Qt6]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
QT_MAJOR=5
if [ "${1:-}" = "qt6" ] || [ "${1:-}" = "Qt6" ]; then
  QT_MAJOR=6
fi
exec bash "$ROOT/scripts/ci/build-linux.sh" "$QT_MAJOR" release
