# OpenRGB Minecraft Sender

Fabric client mod that sends UDP telemetry and Room Ambilight cubemap samples (shared memory) to the OpenRGB 3D Spatial plugin.

Build:

```bat
gradlew.bat build
```

Output JAR: `build/libs/openrgb-minecraft-sender-<version>.jar`

GitHub releases (`v*`) also publish that JAR next to the plugin packages — download it from the release page if you do not build locally.

Plugin-side Minecraft effects live under `Effects3D/Games/Minecraft/` (C++). This folder is the in-game companion only.
