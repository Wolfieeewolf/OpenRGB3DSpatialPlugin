# Spatial lighting engine (LED sampling)

Design direction for **room-native lighting**: effects and game data exist **in 3D room space**; each LED samples that scene as a probe. The user’s **reference point** is primarily **where you stand/sit** (viewer), not where waves spawn from.

Related: [VIEWPORT.md](VIEWPORT.md) (layout), [GAME.md](GAME.md) (telemetry volumes), [VIEWPORT_QT515.md](VIEWPORT_QT515.md).

---

## Problem with “reference = effect origin”

Most spatial effects today use `GetEffectOriginGrid()` / `GetReferencePointGrid()` so distance, angle, and falloff radiate **from one point**. That reads well on a strip; in a **room** it often feels like a projector stuck in the middle, not a object or light **in the corner**.

**Goal:** A blob in the **left corner** should read as **physically there**—brightness and color on nearby strips, falloff along surfaces, and (later) shadows from display planes and controllers—while still looking **balanced from the user’s position**.

---

## Core ideas

### 1. Viewer anchor (reference point)

| Role | Meaning |
|------|--------|
| **Viewer anchor** | User head / desk position in room coords (`REF_MODE_USER_POSITION`, custom point, or calibrated “listening position”). |
| **Used for** | Falloff bias (“importance” toward viewer), specular-ish highlights toward viewer, compass/game facing, comfort tuning—not mandatory effect origin. |
| **Not used for** | Forcing all waves/particles to emit from that point unless the effect is explicitly **origin-based**. |

Existing reference modes stay; **spatial lighting** effects declare `MappingMode::SpatialLighting` and treat anchor as **viewer**, not **emitter**.

### 2. Spatial lighting engine (SLE)

Small **room scene** evaluated at each LED world position `(room_x, room_y, room_z)`:

```text
Layout (controllers, planes, walls)
        → Scene (lights, emissive objects, occluders)
        → Shade(LED position, viewer anchor, time) → RGB
        → Existing effect stack / blend
```

Not a game engine: no full physics, no Blender. **Lights + emissive bodies + hard/soft occluders** sampled per LED.

**Example:** Back-wall wash light + display plane occluder → LEDs behind the plane get no direct light; LEDs on the lit side do. Blob in corner is an **emissive volume** at fixed room coords; viewer anchor only shapes **how strongly** sides facing the user read (optional).

### 3. Hologram volume (separate path)

**Different mapping mode:** density/color defined in a **volume** (SDF, 3D grid, particles) for “shape in space” aesthetics—sparkles, smoke, abstract forms—not necessarily physically correct lighting.

| Path | Intent |
|------|--------|
| **Spatial lighting** | Believable room lighting, occlusion, game/world presence. |
| **Hologram volume** | Artistic 3D forms; per-LED sample of a field; viewer anchor for composition. |

Same LED loop; different evaluator behind `CalculateColorGrid`.

---

## Effect taxonomy (library UX)

Group effects by **mapping system** so UI shows relevant controls only (not one giant reference/slider panel for everything).

| Mapping mode | Library group (example) | Evaluator | Typical UI |
|--------------|-------------------------|-----------|------------|
| **Origin field** | Spatial (classic) | Distance/angle from effect origin | Speed, size, origin offset, reference as emitter |
| **Spatial lighting** | Spatial · Lighting | SLE shade + scene lights/objects | Lights, intensity, shadow quality, viewer-linked bias |
| **Hologram volume** | Spatial · Volume | Volume/SDF/particle field | Shape, density, blend, volume mix |
| **Surface / plane** | Media | Display planes, capture | Plane list, calibration |
| **Room map** | Game · Room | Compass layers, voxels, telemetry | Heading, layers, voxel mix |
| **Device strip** | (legacy 2D) | Zone/strip order | Strip-oriented controls |

Implementation hook: `SpatialEffect3D::GetMappingMode()` (enum) + filter settings panels by mode. Categories in `EffectListManager3D` can mirror groups (`Spatial`, `Spatial · Lighting`, `Spatial · Volume`, `Game`, `Media`).

---

## Per-LED pipeline (unchanged shell)

The tab already samples **each LED** at its room position. SLE only replaces **what** `CalculateColorGrid` asks:

1. Map LED → `(room_x, room_y, room_z)` (existing).
2. `SpatialLightingEngine::Shade(scene, led_pos, viewer_anchor, time)`.
3. Blend with stack (existing).

Waves that should stay origin-based keep `MappingMode::OriginField` and current math.

---

## Phasing

### Test effects (validate before other systems)

See [SPATIAL_LIGHTING_TEST.md](SPATIAL_LIGHTING_TEST.md).

| Effect | Role |
|--------|------|
| **Room light probe (test)** | Static white source at room center — falloff + occlusion |
| **Room campfire / blob** | Colored corner/center placement, AO, viewer rim, flicker |

Do **not** hook game or screen mirror until these pass the checklist.

### Phase A — Room campfire (first slice)

- Effect: **Room campfire / blob** (`Spatial · Lighting`).
- Emissive + direct light at fixed room placement; **user reference = viewer** (rim toward seat).
- **Occlusion:** display planes block segment LED → fire; **AO:** axis probes darken ambient in shade.

### Phase B — Occluders (in progress)

- Display planes (viewport-aligned transform)
- Controller oriented bounding boxes (per-device, self-shadow skipped)
- Room walls (optional)

Still planned: reference-point props, denser mesh occluders, screen-mirror coupling.

### Phase C — Upgrade path

- Optional SLE backend for selected classic effects (e.g. wave as **traveling light front** in volume, not origin ripple).
- Game telemetry feeds **scene emissive volume** + same shade pass.

### Phase D — Hologram volume

- Separate effect group; SDF or low-res room grid; no shadow requirement v1.

---

## Code placement

| Piece | Location |
|-------|----------|
| Room taxonomy, frame context, depth preset | `SpatialRoom/` |
| Scene, lights, occluders, `ShadeLed` | `SpatialLighting/` |
| Game/voxel projection | `SpatialSamplers/` |
| Lighting effect plugins | `Effects3D/SpatialLighting/` |
| Effect hooks | `SpatialEffect3D::GetSpatialRoomMode()` |

See [SPATIAL_ROOM.md](SPATIAL_ROOM.md).

---

## Success criteria

1. Blob in corner looks **anchored in the room**, not centered on user.
2. Moving **user/reference** changes **how it reads**, not **where the blob sits**.
3. With occlusion: back-wall light + plane → believable dark side.
4. Library: user picks **Spatial · Lighting** and sees lighting controls, not wave-origin sliders.

---

## Open questions

- Default viewer anchor: `REF_MODE_USER_POSITION` vs explicit “View point” object in viewport?
- Soft shadows vs hard (performance vs 200+ LEDs)?
- One global scene per stack vs per-effect scene merge?

Track decisions here as phases land.
