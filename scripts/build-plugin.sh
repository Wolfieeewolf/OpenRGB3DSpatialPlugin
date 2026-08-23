#!/usr/bin/env bash
# OpenRGB Effects Plugin–style entry point for Linux CI (Qt6).
# Usage: build-plugin.sh [qt6|Qt6]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
QT_MAJOR=6
exec bash "$ROOT/scripts/ci/build-linux.sh" "$QT_MAJOR" release
