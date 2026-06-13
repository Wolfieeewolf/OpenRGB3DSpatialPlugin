# Spatial lighting — validation checklist

Validate emitter-relay shading and occlusion **before** wiring game effects or screen mirror.

Engine: `SpatialLighting/SpatialLightingEngine.*`  
Design: [SPATIAL_LIGHTING_ENGINE.md](SPATIAL_LIGHTING_ENGINE.md), [SPATIAL_MODES_GUIDE.md](SPATIAL_MODES_GUIDE.md)

## Setup

1. Rebuild and load the plugin.
2. Layout: keyboard (or other emitter) plus strips on opposite sides of the room.
3. Stack **reference** → **User position** (your seat).
4. Prefer **manual room size** so room bounds match the visible room box.
5. Optional: one **display plane** between emitter and receiver strips.

## Test A — Emitter + relay (Room output panel)

**Effect:** **Color Wheel** (or any spatial pattern)  
**Room output:** **Emitter relay** (or **Emitter source** on one layer + **Relay shade** on another)  
**Purpose:** Keyboard runs the pattern; other LEDs shade from emitter colors with occlusion.

| Check | Pass? |
|-------|-------|
| Emitter zone LEDs show the pattern; receivers shade from emitter colors | |
| **Room coordinates → Room mapped** paints hue on room bounds, not effect origin | |
| **Occlusion / Shadows** on → strips behind a display plane or controller darken | |
| **Block through controller bodies** on → strips behind another controller darken | |
| **Ambient occlusion** higher → deeper shade in shielded spots | |
| **Room fill** adjusts how much unoccluded ambient bleeds in | |
| Changing stack anchor does **not** move emitter device selection | |

## Test B — Emitter + relay (Room output)

**Effect:** Any classic spatial effect (e.g. **Color Wheel**) with **Room output → Emitter + relay (screen mirror)**.

| Check | Pass? |
|-------|-------|
| Emitter checkboxes match expected controllers | |
| Receiver LEDs shade from emitter pattern | |
| Occlusion toggles match Test A | |

## Occlusion sources (today)

| Source | Toggles | Notes |
|--------|---------|-------|
| Display planes | Shadows (master) | Viewport-aligned transform |
| Custom controller cells | Block through controller bodies | **Add light blocker** in custom controller editor (per layer) |
| Physical controllers | Block through controller bodies | Full-device box |
| Room walls | Include room walls | Off by default; can darken whole room |

### Custom controller light blockers

1. Edit custom controller → select cells on a layer (e.g. under key switches).
2. **Add light blocker** → cells show **B** (purple).
3. Save layout, run relay shading with **Block through controller bodies** enabled.

## Legacy presets

| Removed effect id | Load behaviour |
|-------------------|----------------|
| **RoomColorWheel**, **RoomEmissiveRelay** | Load as **ColorWheel** — set **Room output** (room mapped / emitter relay) |
| **RoomLightProbe**, **RoomWashLight**, **RoomCampfire** | No replacement — re-add layers using **Room output** on classic effects |

## Not in scope yet

- Minecraft / game telemetry lighting  
- Screen mirror occlusion  
- Reference-point / small object meshes (beyond controller boxes)

## Report issues

Note: emitter/receiver selection, plane position, which LEDs wrong (too bright / dark / no block), Qt version, screenshot if possible.
