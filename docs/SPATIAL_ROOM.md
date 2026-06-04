# Spatial room evaluation

How effects use the **3D layout** as a coordinate system—not a full game engine. Works for large room setups and small ones (few mapped LEDs).

Related: [SPATIAL_LIGHTING_ENGINE.md](SPATIAL_LIGHTING_ENGINE.md), [SPATIAL_LIGHTING_TEST.md](SPATIAL_LIGHTING_TEST.md), [VIEWPORT.md](VIEWPORT.md).

---

## Folder layout (siblings)

| Folder | Role |
|--------|------|
| **`SpatialRoom/`** | Taxonomy, capabilities, per-frame context (orchestration) |
| **`SpatialLighting/`** | Lights + occluders → `ShadeLed()` |
| **`SpatialSamplers/`** | Game/voxel **projection** onto `(x,y,z)` (`SpatialLayerCore`, …) |
| **`Effects3D/`** | Effect plugins (stack entries) |

Do **not** put room policy in `SpatialSamplers/` (that is incoming field mapping, not “how the stack runs”).

Do **not** fold everything into `SpatialLighting/` (lighting is one evaluator).

Existing **`SpatialMappingMode`** on `SpatialEffect3D` (Off / SubtleTint / CompassPalette / VoxelVolume) is the **room tint / game probe** UI—separate from **`SpatialRoom::SpatialRoomMode`**.

---

## `SpatialRoomMode` (effect family)

| Mode | Library group (typical) | Evaluator |
|------|-------------------------|-----------|
| `OriginField` | Spatial | Classic origin distance/angle |
| `SpatialLighting` | Spatial · Lighting | `SpatialLighting::ShadeLed` |
| `RoomMappedPattern` | Spatial · Mapped | Pattern → room sample (Color Wheel 3D, disco ball, …) |
| `HologramVolume` | Spatial · Volume | Volume/SDF (future) |
| `RoomMap` | Game · Room | Game bridge + `SpatialSamplers` |
| `AudioReactive` | Spatial · Audio | FFT/beat room placement (planned) |
| `SurfaceMedia` | Media | Display planes / capture |
| `DeviceStrip` | Device | Strip-local |

Effects override `GetSpatialRoomMode()` on `SpatialEffect3D`. Default: `OriginField`.

---

## Capabilities & depth preset

`SpatialRoomCapabilities` flags (per effect, merged with frame preset):

- `CapSkipSampleWarp` — room-fixed emitters (campfire)
- `CapUseOcclusion` / `CapUseAmbientOcclusion` — `SpatialLighting` only
- `CapPreferLedOnlyIteration` — hint for small setups (future render path)
- `CapRoomGridCoordinates` — room grid, not world LED coords

`SpatialRoomDepthPreset` on the render pass (`SpatialRoomFrame`):

- **Simple** — no occlusion, no AO (fast / small rooms)
- **Standard** — default
- **Quality** — full occlusion + AO

`OpenRGB3DSpatialTab::RenderEffectStack` calls `SpatialRoom::BeginEffectRenderFrame` / `EndEffectRenderFrame`. Room grid overlay uses `BeginRoomGridOverlayPass` (fast preview: no shadow/AO rays).

---

## Per-LED pipeline (unchanged shell)

1. Layout → LED `(room_x, room_y, room_z)` (existing).
2. Effect `CalculateColorGrid` / `EvaluateColorGrid`.
3. Stack blend → hardware.

Room natives call `SpatialLighting::ShadeLed`. Mapped classics keep pattern math, change **input coordinates**. Audio effects will use `AudioReactive` mode + room placement (next wave).

---

## Phasing (next work)

1. **Migrate classics** — e.g. Color Wheel → `RoomMappedPattern`, room coords + optional occlusion off by default.
2. **Room natives** — disco ball (`RoomMappedPattern` or lighting rays).
3. **Audio** — `AudioReactive` mode: spectrum/beat → position in room → existing audio color math.
4. **LED-only iteration** — honor `CapPreferLedOnlyIteration` in `OpenRGB3DSpatialTab_EffectsRender.cpp`.
5. **Game / mirror** — consume `SpatialRoom` + `SpatialLighting`, not duplicate.

---

## Code hooks

```cpp
// Effect plugin
SpatialRoom::SpatialRoomMode GetSpatialRoomMode() const override;

// Optional override; default from mode
SpatialRoom::SpatialRoomCapabilities GetSpatialRoomCapabilities() const override;

// Merged with frame depth preset
EffectiveSpatialRoomCapabilities();
```

Lighting effects: `Effects3D/SpatialLighting/`, base `RoomSpatialLightingEffect3D`.
