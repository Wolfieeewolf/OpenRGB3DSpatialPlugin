# OpenRGB Minecraft Sender

Fabric client mod that sends UDP telemetry and Room Ambilight cubemap samples (shared memory) to the OpenRGB 3D Spatial plugin.

Build:

```bat
gradlew.bat build
```

Output JAR: `build/libs/openrgb-minecraft-sender-<version>.jar`

Plugin-side Minecraft effects live under `Effects3D/Games/Minecraft/` (C++). This folder is the in-game companion only.
