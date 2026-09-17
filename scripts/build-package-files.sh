#!/usr/bin/env bash
# Fill debian/changelog or fedora spec VERSION from qmake (OpenRGBEffectsPlugin).
set -euo pipefail
cd "$(dirname "$0")/.."

if [ -z "${1:-}" ]; then
  echo "ERROR! No file given to parse."
  exit 1
fi

INFILE_PATH="${1}.in"
if [ ! -e "$INFILE_PATH" ]; then
  echo "ERROR! Source file ${INFILE_PATH} missing."
  exit 1
fi

VERSION_VAR="VERSION_NUM"
if [[ "$1" == *"debian"* ]]; then
  VERSION_VAR="VERSION_DEB"
elif [[ "$1" == *"fedora"* ]]; then
  VERSION_VAR="VERSION_RPM"
fi

QMAKE_EXE="$(command -v qmake6 || command -v qmake-qt6 || command -v qmake)"
PACKAGE_VERSION="$("$QMAKE_EXE" OpenRGB3DSpatialPlugin.pro 2>&1 | grep "$VERSION_VAR" | cut -d ':' -f 3 | tr -d ' ')"
if [ -z "$PACKAGE_VERSION" ]; then
  echo "ERROR! Could not read $VERSION_VAR from qmake."
  exit 1
fi
echo "$PACKAGE_VERSION"
sed -e "s/__VERSION__/${PACKAGE_VERSION}/g" "$INFILE_PATH" > "$1"
