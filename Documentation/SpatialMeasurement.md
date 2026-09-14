# Game bridge measurement (Minecraft / telemetry)

This document covers **game-facing** coordinates and scale — how RoomGrid data from the plugin reaches the Minecraft sender mod and other telemetry consumers.

**Read first:** plugin-internal rules (viewport, effects, spacing, reference points, grid) live in [`PluginSpatialMeasurement.md`](PluginSpatialMeasurement.md). Game bridges must not introduce a second mm/grid story. Fix plugin consistency there before tuning Minecraft alignment here.

**Goal:** predictable mapping from plugin RoomGrid → game world samples. No extra unit hops at the SHM/JSON boundary.

---

## 1. Units at the bridge (summary)

Plugin side (see plugin doc for detail):

| Layer | Storage | Rule |
|--------|---------|------|
| Manual room size | mm in UI / layout JSON | → grid via `MMToGridUnits` |
| LED spacing | `spacing_mm_*` | mm between LEDs |
| Grid pitch | `grid_scale_mm` | mm per grid unit |
| Published SHM bounds / origins | **grid units** | Same RoomGrid frame as effects |

Game side (this doc):

| Layer | Rule |
|--------|------|
| Minecraft blocks | `mm_per_block = 1000 / blocks_per_meter` |
| `room_to_world_scale` | blocks per grid unit (derived once in plugin) |

---

## 2. Three coordinate spaces (do not mix)

Room VR tint samples **room SHM only** (no GPU panorama / cubemap path).

### RoomGrid (plugin — see PluginSpatialMeasurement.md)

- **Origin:** front-left floor corner (`grid.min_x/y/z`, usually `0,0,0` for manual room).
- **+X:** right (room width).
- **+Y:** up (floor → ceiling).
- **+Z:** toward back wall (depth).
- **LED positions / effect samples:** grid units in this frame.

### PlayerLocal (per frame, from telemetry)

- **+X:** player right, **+Y:** up, **+Z:** view forward (horizontal).
- Built from Minecraft look vector + yaw offset (`heading_offset_deg` / Room yaw).

### GameWorld (Minecraft)

- **+X:** east, **+Y:** up, **+Z:** south.
- Player **eye** position from mod telemetry (`player_pose` / `getEyeY()`); samples are block colours in this space.

**Pipeline:**

```
LED (RoomGrid) → offset from effect origin → PlayerLocal blocks → GameWorld sample point
```

Shared helpers: `SpatialBasisUtils::BuildHorizontalBasis`, `RoomSampleMapping`, `RoomSampleWorldMapper`.

---

## 3. Reference point vs effect origin

| Concept | Where set | Used for |
|---------|-----------|----------|
| **Reference point** | Setup → Reference points (place at **eye height** where you stand) | Anchor for “where you stand” in the room |
| **Effect origin** | `GetEffectOriginGrid()` = reference + % offset on half room size | Minecraft mapping origin published to SHM |
| **Effect offset %** | Effect settings (`effect_offset_x/y/z` ÷ 100 × half room) | Fine-tune without moving reference point |

Minecraft Room VR expects: **reference point at eye height**, effect **3D origin** on that reference, then **stand there in-game**. The mod samples from **player eyes** (`getEyeY()`), not feet, so ceiling and wall LEDs line up with what you see.

Published to mod (`RoomSampleConfigPublisher`):

- `room_min/max_*` — room bounds in grid units
- `effect_origin_*` — effect origin grid position
- `room_to_world_scale` — **blocks per grid unit** (see §4)
- `heading_offset_deg`, position offset sliders (blocks)

---

## 4. Minecraft scale chain (one path)

Plugin (`MinecraftGame::ComputeRoomToWorldScale`):

```
blocks_per_m     = from mod telemetry (config, default 4)
mm_per_block     = 1000 / blocks_per_m
grid_units_per_block = mm_per_block / grid_scale_mm
room_to_world_scale  = room_vr_scale_tune / grid_units_per_block   // blocks per grid unit
```

Mod (`RoomSampleWorldMapper`):

```
scale = room_to_world_scale   // blocks per grid unit
grid_units_per_block = 1 / scale
rightBlocks   = (roomX - effOx) * scale
forwardBlocks = (effOz - roomZ) * scale   // room +Z is backward; forward is −Z
worldPos      = playerEyes + basis * (right, up, forward)   // getEyeY(), not feet
```

**Sanity check (typical setup):**

- `grid_scale_mm = 10`, `blocks_per_m = 4` → `mm_per_block = 250`
- `grid_units_per_block = 25`, `room_vr_scale_tune = 1` → `scale = 0.04` blocks/grid unit
- One grid unit = 10 mm ≈ 0.04 blocks → 250 mm/block ✓

If this chain is wrong anywhere, alignment drifts uniformly (scale) or along one axis (heading / origin).

---

## 5. Room sample grid (SHM)

1. Plugin publishes config: Nx×Ny×Nz cells covering `room_min`…`room_max`.
2. Mod fills each cell `(ix,iy,iz)` with colour at world target:
   - room cell centre in grid units → `mapRoomToWorldTarget` → raycast/sample
3. Plugin reads cell by **LED position** `(grid_x, grid_y, grid_z)` via trilinear lookup in `RoomSampleMapping`.

Cell centre in grid units:

```
roomX = room_min_x + (ix + 0.5) * span_x / size_x
```

---

## 6. Known alignment pitfalls

### A. Reference point height (eyes vs feet)

Plugin reference points and effect origins are at **eye height** (where effects feel centered). The mod must sample from **player eyes** (`getEyeY()` in Fabric, `player_pose` y in telemetry). If the game sends **feet** position instead, vertical mapping is wrong by ~1.6 blocks — ceiling LEDs look disconnected from what you see.

Symptom: consistent up/down shift or “floating” room colours → confirm mod ≥ 0.9.7 and reference point is at standing eye height.

### B. Heading / yaw

“Room yaw” rotates room layout vs in-game look direction. Symptom: consistent left/right shift → tune **Room yaw** or **Right offset (blocks)**.

### C. Bounds source

Room SHM bounds use `grid.min/max` from layout (manual room or LED AABB). LED strips must sit in the same coordinate frame as the reference point (room-aligned positions).

### D. Multiple spacing paths

LED layout uses `spacing_mm_*` + `grid_scale_mm`. Room manual size uses `width_mm` etc. Both must use the **same** `grid_scale_mm` or grid units won't match physical mm.

---

## 7. Calibration checklist

1. Set **manual room size** to measured mm (W×H×D).
2. Set **grid scale mm** (e.g. 10) once; don't change without relayout.
3. Place **reference point** at your physical standing spot at **eye height** (in front of screens).
4. Set effect **3D origin** to that reference (Minecraft effect UI).
5. In game: stand at the same spot; face your screens.
6. In room: stand at reference point; tune **Room yaw** if left/right shifted; **Forward/Right/Up offset** for fine trim (0.1 block = slider step).
7. Confirm mod config: `room_to_world_scale` matches §4 (≈0.08 for 10 mm grid, 8 blocks/m).
8. Confirm mod version ≥ **0.9.7** (eye-level `player_pose` and room sampling).

---

## 8. Code map (game bridge)

| Concern | Primary files |
|---------|----------------|
| Plugin mm/grid (do not duplicate) | [`PluginSpatialMeasurement.md`](PluginSpatialMeasurement.md), `GridSpaceUtils.cpp` |
| Space helpers | `SpatialSamplers/SpatialBasisUtils.h`, `RoomSampleMapping.*` |
| MC scale publish | `MinecraftGame.cpp` `ComputeRoomToWorldScale`, `RoomSampleConfigPublisher.cpp` |
| MC world map | `RoomSampleWorldMapper.java` |
| SHM sample read | `RoomSampleMapping.cpp` |

**Rule for changes:** plugin distances stay in the plugin doc; this file only adds the **single** `room_to_world_scale` step and PlayerLocal/GameWorld transforms.
