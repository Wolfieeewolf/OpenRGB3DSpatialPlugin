# Integrations

Companion apps, mods and extensions that talk to the OpenRGB 3D Spatial plugin on the same
computer. How they talk (ports, messages, limits): [Documentation/LocalAPI.md](../Documentation/LocalAPI.md).

| Path | For | Talks over | Notes |
|------|-----|-----------|-------|
| [browser-extension/](browser-extension/) | Movie Sync with Netflix, Prime Video, Crunchyroll and others (Chrome, Edge, Brave, Opera, Vivaldi, Firefox) | HTTP `127.0.0.1:27180` | Built and attached to each release |
| [minecraft/](minecraft/) | Minecraft (Fabric) | UDP `127.0.0.1:9876` + shared memory | Alpha: sparse cubemap Room Ambilight + vitals |

They live in this repository so a change to a message and its sender land in one commit and
ship in one release. Plugin effect layers for games stay under `engines/game/<Game>/`.
Developer tools (the stream probe) are under `tools/`.
