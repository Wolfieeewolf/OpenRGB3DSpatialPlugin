#!/usr/bin/env bash
# Install Linux build dependencies for the plugin CI image (Qt6).
# Usage: install-linux-deps.sh [6]
set -euo pipefail

QT_MAJOR="${1:-6}"
if [ "$QT_MAJOR" != "6" ]; then
  echo "Only Qt6 builds are supported (got Qt${QT_MAJOR})"
  exit 1
fi

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq \
  build-essential \
  libusb-1.0-0-dev \
  libhidapi-dev \
  libgl1-mesa-dev \
  libglu1-mesa-dev \
  dpkg-dev \
  fakeroot \
  qt6-base-dev \
  qt6-base-dev-tools
