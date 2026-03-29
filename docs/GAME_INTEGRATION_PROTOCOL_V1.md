<!-- SPDX-License-Identifier: GPL-2.0-only -->

# Game Integration Protocol v1

## Goal

Define one universal, game-agnostic event stream that feeds the OpenRGB 3D Spatial Plugin.
Game-specific adapters (Minecraft mod, Roblox bridge, Unity/Unreal plugin) translate native game state into this protocol.

## Transport

- Primary transport: localhost UDP JSON
- Default host: `127.0.0.1`
- Default port: `9876`
- Encoding: UTF-8 JSON, one JSON object per datagram

Rationale:
- Fast, simple, and available everywhere.
- Stateless delivery is fine for high-frequency telemetry.

## Current Implementation Status

- Protocol v1 message schema is active and is the canonical contract for game adapters.
- Current plugin runtime path for telemetry is UDP JSON on localhost.
- Each supported title is a normal **Effect Library** entry (category `Game`) under `Effects3D/Games/`; effects read the shared snapshot from `GameTelemetryBridge`.
- For quick local checks, use `tools/game_telemetry_udp_test_sender.py` against `127.0.0.1:9876`.

## Envelope

Every message must include:

- `version` (integer): protocol version, currently `1`
- `type` (string): event type
- `timestamp_ms` (integer): sender timestamp in milliseconds
- `source` (string): adapter name, for example `minecraft-fabric`

Example:

```json
{
  "version": 1,
  "type": "player_pose",
  "timestamp_ms": 1711111111111,
  "source": "minecraft-fabric"
}
```

## Coordinate System

- Right-handed coordinate space.
- Units are meters.
- Adapter must provide coordinates in game world space.
- Plugin applies game-to-room transform (configured by user/calibration).

Vectors:
- Position: `(x, y, z)`
- Forward: normalized vector `(fx, fy, fz)`
- Up: normalized vector `(ux, uy, uz)`

## Event Types (v1)

### `player_pose`

Required fields:
- `x`, `y`, `z` (float)
- `fx`, `fy`, `fz` (float)
- `ux`, `uy`, `uz` (float)

Optional:
- `velocity` (float)
- `health` (float 0.0 to 1.0)

Example:

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

### `world_light`

Create/update/remove world-anchored lights.

Required fields:
- `id` (string): stable identifier for this light source

For create/update:
- `x`, `y`, `z` (float)
- `r`, `g`, `b` (int 0-255)
- `intensity` (float 0.0+)
- `radius` (float > 0.0)

For remove:
- `remove` (boolean true)

Example update:

```json
{
  "version": 1,
  "type": "world_light",
  "timestamp_ms": 1711111111120,
  "source": "minecraft-fabric",
  "id": "torch:12,65,-104",
  "x": 12.0,
  "y": 65.6,
  "z": -104.0,
  "r": 255,
  "g": 180,
  "b": 90,
  "intensity": 1.0,
  "radius": 3.5
}
```

Example remove:

```json
{
  "version": 1,
  "type": "world_light",
  "timestamp_ms": 1711111111199,
  "source": "minecraft-fabric",
  "id": "torch:12,65,-104",
  "remove": true
}
```

### `damage_event`

Transient hit/damage cue for directional effects.

Required fields:
- `dx`, `dy`, `dz` (float): direction toward incoming hit
- `strength` (float 0.0 to 1.0)

Optional:
- `kind` (string), for example `melee`, `explosion`, `projectile`

Example:

```json
{
  "version": 1,
  "type": "damage_event",
  "timestamp_ms": 1711111111205,
  "source": "minecraft-fabric",
  "dx": -0.8,
  "dy": 0.0,
  "dz": 0.6,
  "strength": 0.7,
  "kind": "projectile"
}
```

### `health_state`

Continuous health status for pulse/heartbeat overlays.

Required fields:
- `current` (float >= 0.0)
- `max` (float > 0.0)

Example:

```json
{
  "version": 1,
  "type": "health_state",
  "timestamp_ms": 1711111111230,
  "source": "minecraft-fabric",
  "current": 12.0,
  "max": 20.0
}
```

## Recommended Send Rates

- `player_pose`: 20-60 Hz
- `world_light`: on change, plus occasional refresh (1-2 Hz for active lights)
- `damage_event`: event-driven only
- `health_state`: 2-10 Hz

## Reliability Model

- UDP packets may be dropped.
- Sender should periodically resend key state (`player_pose`, active `world_light`s).
- Receiver should age out stale lights if no update is seen (recommended timeout: 2-5 seconds).

## Security

- Listen only on localhost by default.
- Do not parse executable/script payloads.
- Enforce strict type/range checks on all fields.

## Versioning Policy

- Backward-compatible additions are allowed in v1 (new optional fields).
- Breaking changes require `version = 2`.
- Unknown event fields should be ignored.

## Adapter Packaging Guidance

- Minecraft: Fabric mod distributed via Modrinth/CurseForge.
- Roblox: Lua bridge or local helper that emits protocol events.
- Unity/Unreal: lightweight plugin/component that publishes the same schema.

The plugin must remain universal and not embed game-specific logic.
