#!/usr/bin/env bash
# Install Linux build dependencies for the plugin CI image (Qt6).
# Usage: install-linux-deps.sh [6]
#
# When QT_AQT_VERSION is set (default in CI: 6.8.3), Qt is installed via aqtinstall
# to match OpenRGB 1.0. Otherwise falls back to distro qt6-base-dev for local dev.
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
  python3 \
  python3-pip \
  python3-venv

QT_AQT_VERSION="${QT_AQT_VERSION:-}"
QT_AQT_ARCH="${QT_AQT_ARCH:-linux_gcc_64}"
QT_INSTALL_DIR="${QT_INSTALL_DIR:-/opt/qt}"

if [ -n "$QT_AQT_VERSION" ]; then
  python3 -m pip install --quiet --break-system-packages aqtinstall 2>/dev/null \
    || python3 -m pip install --quiet aqtinstall
  python3 -m aqt install-qt linux desktop "$QT_AQT_VERSION" "$QT_AQT_ARCH" -O "$QT_INSTALL_DIR"

  QT_BIN_DIR="${QT_INSTALL_DIR}/${QT_AQT_VERSION}/gcc_64/bin"
  if [ ! -x "${QT_BIN_DIR}/qmake" ]; then
    echo "aqtinstall did not produce qmake at ${QT_BIN_DIR}/qmake"
    exit 1
  fi
  export PATH="${QT_BIN_DIR}:${PATH}"
  echo "Qt ${QT_AQT_VERSION} installed via aqtinstall; qmake at ${QT_BIN_DIR}/qmake"
  qmake -v
else
  apt-get install -y -qq qt6-base-dev qt6-base-dev-tools
fi
