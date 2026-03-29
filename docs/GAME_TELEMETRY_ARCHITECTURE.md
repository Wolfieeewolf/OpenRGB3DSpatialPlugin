# Game integration architecture

## One C++ effect per game

- Each game lives in its own folder under `Effects3D/Games/` (for example `Effects3D/Games/Minecraft/`).
- Each game provides:
  - A **`SpatialEffect3D` subclass** registered with `EFFECT_REGISTERER_3D` (class id, display name, category **`Game`**), so it appears in the **Effect Library** like any other effect.
  - Optional shared logic in a companion `.cpp` / namespace (for example `MinecraftGame` for render + settings widgets).
- **Shared telemetry:** `GameTelemetryBridge` listens on **127.0.0.1:9876** (see `docs/GAME_INTEGRATION_PROTOCOL_V1.md`). All game effects read the same snapshot via `GameTelemetryBridge::GetTelemetrySnapshot()`.
- **Local UDP smoke test:** `tools/game_telemetry_udp_test_sender.py` sends sample v1 JSON datagrams to the listener.

## UI

- There is **no separate Games tab or library** in the main panel. Users add **Minecraft (Fabric)** (or future games) from the effect library, configure in the effect’s custom UI, and **Start** the stack layer.

## Adding a new game

1. Create `Effects3D/Games/<YourGame>/` with an effect class + `REGISTER_EFFECT_3D`.
2. Parse protocol v1 in the shared bridge only if you need **new** message types; otherwise reuse `GameTelemetryBridge::TelemetrySnapshot` fields or extend JSON handling in `GameTelemetryBridge.cpp`.
3. Add sources/headers to `OpenRGB3DSpatialPlugin.pro`.
