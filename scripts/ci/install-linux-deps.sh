#!/usr/bin/env bash
# Install Linux build dependencies for the plugin CI image.
# Usage: install-linux-deps.sh <5|6>
set -euo pipefail

QT_MAJOR="${1:?Qt major version required (5 or 6)}"

export DEBIAN_FRONTEND=noninteractive
apt-get update -qq
apt-get install -y -qq \
  build-essential \
  libusb-1.0-0-dev \
  libhidapi-dev \
  libgl1-mesa-dev \
  libglu1-mesa-dev \
  dpkg-dev \
  fakeroot

if [ "$QT_MAJOR" = "6" ]; then
  apt-get install -y -qq qt6-base-dev qt6-base-dev-tools
else
  apt-get install -y -qq qtbase5-dev qt5-qmake qtbase5-dev-tools
fi
