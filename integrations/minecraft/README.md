# OpenRGB Minecraft Sender

Fabric client mod that sends UDP telemetry (pose, vitals, damage) and **Room Ambilight** cubemap samples over shared memory to the OpenRGB 3D Spatial plugin.

## Expectations (alpha)

This is **alpha**, same as the plugin — get a path working, polish later. Nowhere near a beta “supported configs” bar.

- **Vanilla** Minecraft + this mod is what has been exercised enough to call “works well enough.”
- **Not** meaningfully tested against large modpacks, shader/resource packs, or HD / high UV resolutions. Those can stutter, drop frames, sample wrong, or fall over.
- Room Ambilight samples the world for LEDs only; it still costs client time. Heavier packs and higher UV/face settings make that worse.
- Code and docs here are largely AI-assisted / vibe-coded with little manual review. Expect rough edges and cleanup debt.

Current contract (mod **≥ 0.9.46**):

- Cubemap dims: `face × face × 6` (`kFlagCubemap`)
- Frame payload: **sparse LED texels only** (`kFlagSparseLedTexels`) — not dense `face²×6×4`, not LZ4
- Quality knobs (face size, UV dim, sky fill) live in the **OpenRGB plugin** UI; Mod Menu only toggles send on/off and vitals rate

Build:

```bat
gradlew.bat build
```

Output JAR: `build/libs/openrgb-minecraft-sender-<version>.jar`

**Mod Menu** (optional): telemetry on/off, Room Ambilight on/off, vitals tick rate, and OpenRGB link status.

GitHub releases (`v*`) also publish that JAR next to the plugin packages — download it from the release page if you do not build locally.

Plugin-side Minecraft effects live under `Effects3D/Games/Minecraft/` (C++). Protocol and scale: [`Documentation/SpatialMeasurement.md`](../../Documentation/SpatialMeasurement.md). This folder is the in-game companion only.
