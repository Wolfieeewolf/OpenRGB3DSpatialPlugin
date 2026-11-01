#!/usr/bin/env bash
# Build and install the plugin as an org.openrgb.OpenRGB.Plugin API-5 extension
# against the OpenRGB *pipeline* Flatpak (runtime-version master, KDE 6.10).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MANIFEST="$ROOT/flatpak/org.openrgb.OpenRGB.Plugin.Spatial.yaml"
BUILD_DIR="${FLATPAK_BUILD_DIR:-$ROOT/build/flatpak-spatial}"

if ! command -v flatpak-builder >/dev/null 2>&1; then
  echo "flatpak-builder is not installed (CachyOS: pacman -S flatpak-builder)"
  exit 1
fi

if [ ! -d "$ROOT/OpenRGB/qt" ]; then
  echo "OpenRGB submodule is empty. Run: git submodule update --init --depth 1 OpenRGB"
  exit 1
fi

flatpak --user remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak --user install -y flathub org.kde.Sdk//6.10 org.kde.Platform//6.10 || true

# Pipeline OpenRGB Flatpak from CalcProgrammer1/OpenRGB master, same as Effects.
if ! flatpak info org.openrgb.OpenRGB &>/dev/null; then
  echo "Install pipeline OpenRGB first, e.g.:"
  echo "  curl -L -o org.openrgb.OpenRGB.flatpak \\"
  echo "    'https://gitlab.com/CalcProgrammer1/OpenRGB/-/jobs/artifacts/master/raw/org.openrgb.OpenRGB.flatpak?job=Linux+amd64+Flatpak'"
  echo "  flatpak install --user --assumeyes org.openrgb.OpenRGB.flatpak"
  exit 1
fi

flatpak-builder --user --install --force-clean "$BUILD_DIR" "$MANIFEST"
echo "Installed org.openrgb.OpenRGB.Plugin.Spatial (API 5 / pipeline). Restart OpenRGB."
