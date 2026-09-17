# Game bridge measurement (Minecraft / telemetry)

This document covers **game-facing** coordinates and scale — how RoomGrid data from the plugin reaches the Minecraft sender mod and other telemetry consumers.

**Status:** alpha contract notes, not a support matrix. The bridge is exercised mainly on **vanilla**; modpacks, texture packs, and HD UV/face settings are largely untested and can stutter or break. The plugin as a whole is still alpha (get paths working, polish later) — see the [Minecraft README](../integrations/minecraft/README.md) and root [README](../README.md).

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

**Room Ambilight** samples an **eye-centred cubemap** over room-sample SHM (not a dense Nx×Ny×Nz room volume, and not a GPU panorama path). UDP carries vitals/damage/pose only — colours for Room Ambilight never go over UDP.

### RoomGrid (plugin — see PluginSpatialMeasurement.md)

- **Origin:** front-left floor corner (`grid.min_x/y/z`, usually `0,0,0` for manual room).
- **+X:** right (room width).
- **+Y:** up (floor → ceiling).
- **+Z:** toward back wall (depth).
- **LED positions / effect samples:** grid units in this frame.

### PlayerLocal (per frame, from look basis)

- **+X:** player right, **+Y:** up, **+Z:** view forward (horizontal).
- Built from the Minecraft look vector (`RoomSampleWorldMapper::buildHorizontalBasis`). Header slots `heading_offset_deg` / position offsets exist for layout stability but are **always published as 0** — there is no Room-yaw UI.

### GameWorld (Minecraft)

- **+X:** east, **+Y:** up, **+Z:** south.
- Player **eye** position from the mod (`getEyeY()` / `getEyePosition()`); cubemap rays sample block/sky colours in this space.

**Pipeline (Room Ambilight):**

```
LED (RoomGrid) → direction from effect origin → cubemap face/uv
  → mod ray from eyes along PlayerLocal → GameWorld hit colour
  → sparse SHM texel → plugin nearest-texel LED colour
```

Shared helpers: `SpatialBasisUtils::BuildHorizontalBasis`, `RoomSampleMapping`, `RoomSampleWorldMapper`.

---

## 3. Reference point vs effect origin

| Concept | Where set | Used for |
|---------|-----------|----------|
| **Reference point** | Setup → Reference points (place at **eye height** where you stand) | Anchor for “where you stand” in the room |
| **Effect origin** | `GetEffectOriginGrid()` = reference + % offset on half room size | Minecraft mapping origin published to SHM |
| **Effect offset %** | Effect settings (`effect_offset_x/y/z` ÷ 100 × half room) | Fine-tune without moving reference point |

Room Ambilight expects: **reference point at eye height**, effect **3D origin** on that reference, then **stand there in-game**. The mod samples from **player eyes** (`getEyeY()`), not feet, so ceiling and wall LEDs line up with what you see.

Published to mod (`RoomSampleConfigPublisher`):

- `room_min/max_*` — room bounds in grid units
- `effect_origin_*` — effect origin grid position
- `room_to_world_scale` — **blocks per grid unit** (see §4)
- Cubemap face size, important texel flats, sky flag, UV texture dim (reserved fields)
- Alignment floats in the header (`heading_offset_deg`, `pos_offset_*`) — **always 0** (reserved layout)

Plugin UI knobs for Room Ambilight quality: **cubemap face size**, **texture UV dim**, **sky fill**.

---

## 4. Minecraft scale chain (one path)

Plugin (`MinecraftGame::ComputeRoomToWorldScale`):

```
blocks_per_m     = from mod telemetry (config `blocksPerMeter`, default 4)
mm_per_block     = 1000 / blocks_per_m
grid_units_per_block = mm_per_block / grid_scale_mm
room_to_world_scale  = 1 / grid_units_per_block   // blocks per grid unit
```

Mod (`RoomSampleWorldMapper` — room deltas and ray range):

```
scale = room_to_world_scale   // blocks per grid unit
rightBlocks   = (roomX - effOx) * scale
upBlocks      = (roomY - effOy) * scale
forwardBlocks = (effOz - roomZ) * scale   // room +Z is backward; forward is −Z
```

Cubemap LED rays use `mapLocalDirToWorldTarget` (eye + PlayerLocal direction × room AABB range), not a dense cell-centre grid.

**Sanity check (typical setup):**

- `grid_scale_mm = 10`, `blocks_per_m = 4` → `mm_per_block = 250`
- `grid_units_per_block = 25` → `scale = 0.04` blocks/grid unit
- One grid unit = 10 mm ≈ 0.04 blocks → 250 mm/block ✓

If this chain is wrong anywhere, alignment drifts uniformly (scale) or along one axis (origin / standing spot).

---

## 5. Room Ambilight cubemap (SHM)

1. Plugin publishes config (`RoomSampleConfigPublisher`):
   - Cubemap dims: `size_x == size_y == face`, `size_z == 6` (`kFlagCubemap`)
   - Important texel flats (LED-mapped uv/face indices), even-strided when over `kMaxImportantCells` (65536)
   - Bounds, effect origin, `room_to_world_scale`, sky flag, UV max dim
2. Mod fills **only** those important flats (block sample, optional upward-biased sky fill) and writes a **sparse** frame (`kFlagSparseLedTexels`): `uint32 count` + `count × (uint32 flat, u8 r,g,b,a)`. Dense `face²×6×4` and LZ4 payloads are **not** used.
3. Plugin expands sparse → dense cubemap buffer, then colours each LED by **nearest** cubemap texel for its room direction (`RoomSampleMapping`). Empty gaps (stride / dense wallpaper matrices) borrow the nearest filled neighbour before painting black.

Flat index (face order +X, −X, +Y, −Y, +Z, −Z):

```
flat = (u * face + v) * 6 + face
```

**Contract:** Fabric mod ≥ **0.9.46**, plugin reader accepts **sparse-only** frames. Do not reintroduce dense/LZ4 dual-format loaders.

---

## 6. Known alignment pitfalls

### A. Reference point height (eyes vs feet)

Plugin reference points and effect origins are at **eye height**. The mod must sample from **player eyes** (`getEyeY()`). If the game sends **feet** instead, vertical mapping is wrong by ~1.6 blocks.

Symptom: consistent up/down shift → confirm mod ≥ 0.9.46 and reference point at standing eye height.

### B. Standing spot / facing

There is no Room-yaw slider. Face your screens in-game while standing on the reference point. Left/right or depth miss usually means the reference/origin is wrong, not a hidden heading offset.

### C. Bounds source

Room SHM bounds use `grid.min/max` from layout (manual room or LED AABB). LED strips must sit in the same coordinate frame as the reference point.

### D. Multiple spacing paths

LED layout uses `spacing_mm_*` + `grid_scale_mm`. Room manual size uses `width_mm` etc. Both must use the **same** `grid_scale_mm` or grid units won't match physical mm.

### E. Black bands on dense LED matrices

Wallpaper Engine–style displays can exceed the unique-texel budget. The publisher **even-strides** important cells; the reader **neighbour-fills** gaps. Rebuild plugin + mod together after face-size / LED-count changes. Symptom: horizontal black stripes → check important count vs LED count in plugin logs.

### F. Sky fill

Sky is optional (`kFlagSkyEnabled`). Open-air rays use upward-biased sky/weather fill only (`localUp` gate). Toggling sky republishes config; the bridge matches on cubemap **size**, not `config_id`, so the effect should not go black solely from a config-id churn.

---

## 7. Calibration checklist

1. Set **manual room size** to measured mm (W×H×D).
2. Set **grid scale mm** (e.g. 10) once; don't change without relayout.
3. Place **reference point** at your physical standing spot at **eye height** (in front of screens).
4. Set effect **3D origin** to that reference (Minecraft effect UI).
5. In game: stand at the same spot; face your screens.
6. Tune Room Ambilight **face size** / **UV dim** / **sky** in the plugin as needed.
7. Confirm published `room_to_world_scale` matches §4 (≈**0.04** for 10 mm grid, 4 blocks/m).
8. Confirm mod version ≥ **0.9.46** (sparse LED cubemap SHM; eye-level sampling).

---

## 8. Code map (game bridge)

| Concern | Primary files |
|---------|----------------|
| Plugin mm/grid (do not duplicate) | [`PluginSpatialMeasurement.md`](PluginSpatialMeasurement.md), `GridSpaceUtils.cpp` |
| Space helpers | `SpatialSamplers/SpatialBasisUtils.h`, `RoomSampleMapping.*` |
| MC scale publish | `MinecraftGame.cpp` `ComputeRoomToWorldScale`, `RoomSampleConfigPublisher.cpp` |
| Protocol / flags | `Game/RoomSampleFrameProtocol.h` |
| Sparse frame write (mod) | `integrations/minecraft/.../RoomSampleFrameShmWriter.java`, `OpenRGBSenderMod.java` |
| Sparse frame read (plugin) | `Game/RoomSampleFrameShmReader.cpp` |
| MC world / ray map | `RoomSampleWorldMapper.java` |
| SHM sample → LED | `SpatialSamplers/RoomSampleMapping.cpp` |

**Rule for changes:** plugin distances stay in the plugin doc; this file only adds the **single** `room_to_world_scale` step, PlayerLocal/GameWorld transforms, and the cubemap SHM contract.
