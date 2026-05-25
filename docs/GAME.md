# Game integration

**Vendor SDK capture** (Chroma, iCUE, LightFX, …): see [`GAME_WRAPPERS.md`](GAME_WRAPPERS.md).

## How it works in the plugin

- Games are normal **Effect library** entries (category **Game**), not a separate top-level tab.
- **`GameTelemetryBridge`** (`Game/`) listens on **`127.0.0.1:9876`**, UDP, UTF-8 JSON per datagram.
- Effects read **`GameTelemetryBridge::GetTelemetrySnapshot()`** — no game-specific code in the bridge beyond parsing v1 types.
- **Minecraft hub** (`MinecraftLibraryPanel.ui`): filter library to Fabric family, pick sub-layer type, add to stack. Sub-effects use **`MinecraftGame::ApplyFabricGameEffectChrome`** and optional **`AttachRoomMappingPanel`** (room / voxel mapping).

## Minecraft today

| Piece | Role |
|-------|------|
| `MinecraftGameEffect3D` | Hub / library entry |
| Sub-effects | Health, Hunger, Air, Durability, Damage, World Tint, Lightning |
| `MinecraftSenderMod/` | Optional Fabric mod for local UDP tests |
| `Effects3D/Games/Minecraft/` | Render + settings UI (`MinecraftGameSettingsScroll.ui`, `EffectUiRows`) |

**Status:** plumbing and protocol v1 work; gameplay feel, calibration UX, and edge-case telemetry need more iteration. Treat as **experimental**.

## Adding another game

1. `Effects3D/Games/<Game>/` + `REGISTER_EFFECT_3D`.
2. Ship an adapter (mod, bridge, companion app) that emits protocol v1 on localhost.
3. Extend `GameTelemetryBridge.cpp` only if you need a **new** `type` or fields; prefer optional v1 fields first.
4. Add sources to `OpenRGB3DSpatialPlugin.pro`.

## Protocol v1 (adapter contract)

**Envelope (every message):** `version` (1), `type`, `timestamp_ms`, `source`.

| `type` | Required fields |
|--------|-----------------|
| `player_pose` | `x,y,z`, `fx,fy,fz`, `ux,uy,uz` |
| `world_light` | `id`; update: position, `r,g,b`, `intensity`, `radius`; remove: `"remove": true` |
| `damage_event` | `dx,dy,dz`, `strength` |
| `health_state` | `health`, `health_max`; optional `hp_per_heart`, `hearts`, `hearts_max`, hunger/air/durability |

**Rates:** pose 20–60 Hz; world_light on change + light refresh; damage event-driven; health 2–10 Hz. Resend pose/lights; receiver drops stale lights (~2–5 s).

**Security:** localhost by default; validate types/ranges; no executable payloads. Breaking changes → `version: 2`.

### Example `player_pose`

```json
{
  "version": 1,
  "type": "player_pose",
  "timestamp_ms": 1711111111111,
  "source": "minecraft-fabric",
  "x": 12.4,
  "y": 65.0,
  "z": -104.7,
  "fx": 0.98,
  "fy": 0.0,
  "fz": 0.20,
  "ux": 0.0,
  "uy": 1.0,
  "uz": 0.0
}
```

Canonical parsers and extra fields: **`Game/GameTelemetryBridge.cpp`**, **`MinecraftSenderMod/`**.

## Ideas forward

- In-plugin calibration wizard (game space → room grid).
- Sample stack presets per game (health bar on strip, damage flash on desk zone, …).
- Second adapter (Roblox bridge, small Unity sample) to stress-test v1 without Minecraft-only assumptions.
- Protocol v2 only when a breaking schema is unavoidable.
