# Spatial lighting — manual test checklist

Validate the engine **before** wiring game effects or screen mirror. Effects live under **Spatial · Lighting**.

Engine: `SpatialLighting/SpatialLightingEngine.*`  
Design: [SPATIAL_LIGHTING_ENGINE.md](SPATIAL_LIGHTING_ENGINE.md)

## Setup

1. Rebuild and load the plugin.
2. Layout: at least two controllers on opposite sides of the room.
3. Stack **reference** → **User position** (your seat). This does **not** move the light.
4. Prefer **manual room size** so placement corners match the visible room box.
5. Optional: one **display plane** between a strip and the test light.

## Test A — Room light probe (test)

**Effect:** Room light probe (test)  
**Purpose:** Static white source at **room center** — easy to see falloff and blocking.

| Check | Pass? |
|-------|-------|
| LEDs near center are brightest; far LEDs dimmer (not uniform strip color) | |
| **Brightness** scales overall level | |
| Plane between probe and a strip: that strip drops sharply | |
| Controller between probe and a strip: that strip drops sharply | |
| LEDs in corners / behind geometry slightly darker (AO) | |

## Test B — Room campfire / blob

**Effect:** Room campfire / blob  
**Placement:** Corner (low) or Custom %

| Check | Pass? |
|-------|-------|
| Glow stays in corner when you change reference / user position | |
| **Occlusion** off → blocking disabled; LEDs behind objects brighten | |
| **Block through controller bodies** on → strips behind another controller darken | |
| **Ambient occlusion** higher → deeper shade in shielded spots | |
| **Speed** > 0 → visible flicker | |
| **Glow size** / **Light reach** (mm) change core vs how far light spreads | |
| **Room fill** down + **Shadows** on → clearer shadows | |
| Changing spatial anchor does **not** move the fire | |

## Test C — Reference = viewer, not emitter

1. Campfire placement **Near corner (room min)** — note room min corner in viewport.
2. Change stack anchor to **World origin (0,0,0)** vs **User position**.
3. Fire should stay at the **same room corner**; brightness on strips may shift slightly. If the whole glow jumps to the anchor, rebuild — sample warp should be off.

## Occlusion sources (today)

| Source | Campfire toggles | Notes |
|--------|------------------|-------|
| Display planes | Shadows (master) | Viewport-aligned transform |
| Custom controller cells | Block through controller bodies | **Add light blocker** in custom controller editor (per layer) |
| Physical controllers | Block through controller bodies | Full-device box |
| Room walls | Include room walls | Off by default; can darken whole room |

### Custom controller light blockers

1. Edit custom controller → select cells on a layer (e.g. under key switches).
2. **Add light blocker** → cells show **B** (purple).
3. Save layout, run spatial lighting with **Block through controller bodies** enabled.

## Not in scope yet

- Minecraft / game telemetry lighting  
- Screen mirror occlusion  
- Reference-point / small object meshes (beyond controller boxes)

## Report issues

Note: placement, plane position, which LEDs wrong (too bright / dark / no block), Qt version, screenshot if possible.
