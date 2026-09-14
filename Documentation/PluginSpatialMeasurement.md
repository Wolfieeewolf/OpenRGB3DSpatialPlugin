# Plugin spatial measurement contract

This document is the **single source of truth for the OpenRGB 3D Spatial plugin itself** — viewport, layout, effects, LED spacing, reference points, display planes, zones, and spatial lighting.

**Prerequisite for game bridges:** internal plugin math must be consistent here first. Minecraft / telemetry mapping is documented separately in [`SpatialMeasurement.md`](SpatialMeasurement.md) and must consume values produced by this contract without re‑inventing conversions.

**Goal:** one metric story inside the plugin — **1 mm in the UI = 1 mm in stored layout data = predictable grid units at runtime**. No ad‑hoc `/100` for distance, no mixed cm/m, no feature-local unit hacks.

---

## 1. Two layers: millimetres and grid units

The plugin uses **millimetres (mm)** for human-facing distances and **grid units** for runtime spatial math.

| Layer | Role | Examples |
|--------|------|----------|
| **mm** | UI spinboxes, JSON fields labelled mm, physical sizes | Room W×H×D, LED spacing, display plane size, spatial-light reach |
| **Grid units** | Positions, bounds, effect sampling, viewport scene graph | `transform.position`, `LEDPosition3D.world_position`, `GridContext3D.min/max`, `CalculateColorGrid(x,y,z)` |

**Exactly one conversion pair** implements the mm ↔ grid relationship:

```
grid_units = mm / grid_scale_mm
mm       = grid_units * grid_scale_mm
```

Functions: `MMToGridUnits`, `GridUnitsToMM`, `SafeGridScaleMm` in `GridSpaceUtils.cpp`.

Constants: `DEFAULT_GRID_SCALE_MM` (10), `DEFAULT_ROOM_SIZE_MM` (1000) in `GridSpaceUtils.h`.

Rules:

- **Convert at boundaries** (UI load/save, mm-labelled settings entering spatial code). Do not convert twice.
- **Never** embed `* 10`, `/ 10`, `* 1000`, or `/ 1000` for spatial distance except inside `GridSpaceUtils`.
- **`grid_scale_mm`** (default **10**) is global for the layout profile: mm per **one** grid unit. Changing it rescales the whole layout; treat it like changing unit size, not zoom.

Percent sliders (`/ 100.0f`) are for **normalized parameters** (effect offset %, brightness, audio EQ) — not for mm.

---

## 2. RoomGrid — plugin coordinate frame

All plugin spatial subsystems share **one architectural frame** called **RoomGrid**:

| Axis | Direction |
|------|-----------|
| **Origin** | Front-left floor corner |
| **+X** | Right (room width) |
| **+Y** | Up (floor → ceiling) |
| **+Z** | Toward the back wall (depth) |

When **manual room size** is enabled, bounds are pinned to this box:

```
min = (0, 0, 0)
max = MMToGridUnits(width_mm, height_mm, depth_mm)
```

Manual room dimensions in the UI are **always mm** (`GridSettingsPanel`: “Width (X, mm)”, etc.). Layout JSON stores them under `room.width`, `room.depth`, `room.height` as mm floats.

**Do not** re-map RoomGrid to Unity/Unreal/Minecraft axes inside plugin code. Game bridges convert *out* of RoomGrid at the telemetry boundary only.

---

## 3. What each subsystem stores

### 3.1 Grid settings (`grid_scale_mm`, `grid_x/y/z`)

| Field | Unit | Meaning |
|-------|------|---------|
| `grid_scale_mm` | mm | Millimetres per one grid unit |
| `grid_x`, `grid_y`, `grid_z` | **unitless cell counts** | Default template size for **new** device/heuristic layouts only |

`grid_x/y/z` are **not** mm and **not** room bounds. They size fallback 2D/3D device layout grids before spacing is applied. Room bounds come from manual mm or LED AABB (see §4).

### 3.2 Controllers and LEDs

| Field | Unit | Notes |
|-------|------|-------|
| `ControllerTransform.transform.position` | grid units | RoomGrid placement |
| `ControllerTransform.transform.rotation` | degrees | Dimensionless |
| `ControllerTransform.transform.scale` | dimensionless | Scales local LED layout around strip centroid |
| `led_spacing_mm_x/y/z` | mm | Distance between adjacent LEDs when generating layout |
| `LEDPosition3D.local_position` | grid units | After spacing conversion |
| `LEDPosition3D.world_position` | grid units | After transform (rotate/scale/translate) |
| `LEDPosition3D.room_position` | grid units | Used for room-aligned bounds; today equals `world_position` after `UpdateWorldPositions` |

**Physical controller layout path:** `GenerateCustomGridLayoutWithSpacing` multiplies cell indices by `MMToGridUnits(spacing_mm_*, grid_scale_mm)`.

When `spacing_mm_*` is unset (≤ 0.001), layout uses **1.0 grid unit** per cell step — physically equal to **`grid_scale_mm` mm** between LEDs.

**Virtual controller path:** `CellGridPointMm` accumulates per-column/per-row/per-layer sizes in **mm**, then `MMToGridUnits` once per LED.

**World update:** `ControllerLayout3D::UpdateWorldPositions` — local → scale → rotate → add controller position. All in grid units.

### 3.3 Reference points

| Field | Unit |
|-------|------|
| `VirtualReferencePoint3D.transform.position` | grid units |

Reference points are RoomGrid positions. Effect origin modes (`REF_MODE_ROOM_CENTER`, custom point, LED centroid, etc.) resolve to grid units via `SpatialEffect3D::GetReferencePointGrid` / `GetEffectOriginGrid`.

Effect offset sliders are **percent of half room size in grid units** (`effect_offset_x/y/z / 100 * half_w/h/d`) — not mm.

### 3.4 Display planes

| Field | Unit |
|-------|------|
| `width_mm`, `height_mm` | mm |
| `transform.position`, rotation, scale | grid units / degrees / dimensionless |

Viewport and spatial lighting convert plane size with `MMToGridUnits(plane->GetWidthMM(), grid_scale_mm)` at use time.

Screen Mirror converts **distance** to mm only at the Screen Mirror API boundary (`GridUnitsToMM` in `SpatialMapToScreen` / falloff helpers). Internal ray tests use grid units.

### 3.5 Effects and render frame

Each frame builds two bounds contexts in `OpenRGB3DSpatialTab_EffectsRender.cpp`:

| Context | Bounds source | Used when |
|---------|---------------|-----------|
| `room_grid` | `ComputeRoomAlignedBounds` | Default for most room effects |
| `world_grid` | `ComputeGridBounds` | Effects with “use world bounds” |

Both carry the same `grid_scale_mm`. LED colours are sampled at **`room_position`** (grid units):

```
CalculateColorGrid(room_x, room_y, room_z, time, grid)
```

`GridContext3D` fields (`min_x`…`max_z`, `width`, `center_*`) are all **grid units**. Use `grid.grid_scale_mm` when an effect needs mm (e.g. reach, labels, UI feedback).

### 3.6 Spatial lighting

Parameters exposed as mm in UI (`glow_radius_mm`, `light_reach_mm`) convert with `MMToGridUnits` inside the engine/occluder build. Occluder geometry is stored in **grid units**.

### 3.7 Zones

`ZoneGrid3D` can build per-zone `GridContext3D` pairs using the same `room_grid_scale_mm` / `world_grid_scale_mm` (both should equal layout `grid_scale_mm`). Zone centroids and anchors are grid units.

---

## 4. Room bounds: manual vs auto

| Mode | Bounds | Consistency rule |
|------|--------|------------------|
| **Manual room** (`use_manual_room_size`) | Fixed box from mm → grid | LEDs should be placed inside this box; reference point should match physical standing location in the same frame |
| **Auto room** | AABB of all LED `world_position` / `room_position` | No mm room box; `grid_x/y/z` does not set bounds |

**Note:** `room_position` currently equals `world_position` after `UpdateWorldPositions`. `ComputeRoomAlignedBounds` and `ComputeGridBounds` therefore match until room-aligned transforms are implemented.

Default empty layout fallback: **1000 mm** per axis → grid units via `MMToGridUnits` (`DEFAULT_ROOM_SIZE_MM` in `GridSpaceUtils.h`).

---

## 5. End-to-end flow (plugin only)

```
┌─────────────────────────────────────────────────────────────┐
│  UI / JSON (mm)                                             │
│  room W×H×D, spacing_mm_*, plane width/height, light reach  │
└──────────────────────────┬──────────────────────────────────┘
                           │ MMToGridUnits(..., grid_scale_mm)
                           ▼
┌─────────────────────────────────────────────────────────────┐
│  RoomGrid (grid units)                                      │
│  controller positions, LED world_position, reference points │
│  display plane transforms, GridContext3D bounds             │
└──────────────────────────┬──────────────────────────────────┘
                           │ per LED each frame
                           ▼
┌─────────────────────────────────────────────────────────────┐
│  Effects / Screen Mirror / Spatial lighting                 │
│  sample at (room_x, room_y, room_z), use grid.min/max       │
└──────────────────────────┬──────────────────────────────────┘
                           │ GridUnitsToMM only when UI needs mm
                           ▼
┌─────────────────────────────────────────────────────────────┐
│  Labels, distance readouts, reach comparisons in mm         │
└─────────────────────────────────────────────────────────────┘
```

---

## 6. Allowed exceptions (not mm)

These are intentional and **not** spatial distance conversions:

| Pattern | Meaning |
|---------|---------|
| `value / 100.0f` on effect sliders | Percent or normalized 0–1 |
| `effect_speed / 200.0f` | Normalized effect parameter |
| Rotation in degrees | Angular, not length |
| `transform.scale` | Unitless multiplier |
| Audio/UI meters `* 1000` | Display scaling only |
| OpenRGB device brightness `/ 100` | Protocol 0–100, unrelated to layout |

---

## 7. Consistency checklist (before adding features)

1. Is this distance shown to the user in mm? → Store and label as mm; convert once with `MMToGridUnits`.
2. Is this a position in the room? → Grid units in RoomGrid; never store raw mm on `Vector3D` position fields.
3. Does the effect need a length in mm? → Read `grid.grid_scale_mm` or convert with `GridUnitsToMM`.
4. Are you changing `grid_scale_mm`? → Regenerate LED layouts and re-check manual room bounds (existing UI already does this on scale change).
5. Are you mixing `world_position` and `room_position`? → Prefer `room_position` for effect sampling; document if they must diverge later.
6. Are you adding a game bridge? → Stop at RoomGrid + published config; see [`SpatialMeasurement.md`](SpatialMeasurement.md).

---

## 8. Common pitfalls (plugin)

### A. Double conversion

Calling `MMToGridUnits` on values already in grid units (e.g. controller `transform.position`) doubles scale error.

### B. `grid_x/y/z` mistaken for room size

Layout size spinboxes are **cell counts** for device templates, not room millimetres. Room size is the **Room size** group (mm) or LED AABB.

### C. Spacing vs grid scale

LED spacing is in **mm between LEDs**. Grid scale is **mm per grid unit**. They combine only through `MMToGridUnits(spacing_mm, grid_scale_mm)`.

### D. Manual room vs LED placement

Manual room defines bounds independent of where LEDs sit. Effects normalize against `grid.min/max`; LEDs outside the box still render but may clip normalized coordinates.

### E. Separate math in one effect

Effects should use `GridContext3D` bounds and `grid_scale_mm`. Avoid hard-coded “10 mm per unit” or `/ 10` in effect code — read from context.

---

## 9. Code map (plugin spatial)

| Concern | Primary files |
|---------|----------------|
| mm ↔ grid conversion | `GridSpaceUtils.h`, `GridSpaceUtils.cpp` |
| Room bounds | `ComputeGridBounds`, `ComputeRoomAlignedBounds` |
| LED layout + spacing | `ControllerLayout3D.cpp`, `VirtualController3D.cpp` |
| World positions | `ControllerLayout3D::UpdateWorldPositions` |
| Render grid context | `ui/OpenRGB3DSpatialTab_EffectsRender.cpp` |
| Effect origin | `SpatialEffect3D::GetReferencePointGrid`, `GetEffectOriginGrid` |
| Viewport room box | `ui/LEDViewport3D.cpp` (`SetRoomDimensions` mm → draw via grid scale) |
| Display planes | `DisplayPlane3D.h`, `Geometry3DUtils.h` (`SpatialMapToScreen`) |
| Spatial lighting mm params | `Effects3D/SpatialLighting/RoomSpatialLightingUi.h`, `SpatialLightingEngine.cpp` |
| Layout persistence | `ui/OpenRGB3DSpatialTab_Layout.cpp` (`grid.scale_mm`, `room.width/depth/height`) |

**Change rule:** new spatial distance logic goes through `GridSpaceUtils` or uses `GridContext3D.grid_scale_mm`. No new standalone converters.

---

## 10. Relation to game / Minecraft doc

[`SpatialMeasurement.md`](SpatialMeasurement.md) describes **PlayerLocal**, **GameWorld**, SHM, and Minecraft block scale — layers **outside** the plugin core. Those bridges must:

- Read room bounds and effect origin already in **RoomGrid grid units**
- Read `grid_scale_mm` for mm semantics
- Apply **one** external scale (`room_to_world_scale`) at the mod boundary

If plugin-internal distances disagree (viewport vs effect vs spacing), fix here first — game alignment cannot be stable otherwise.
