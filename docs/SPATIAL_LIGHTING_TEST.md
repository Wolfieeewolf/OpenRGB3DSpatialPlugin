# Spatial lighting — validation checklist

Validate the engine **before** wiring game effects or screen mirror. Effects live under **Spatial · Lighting**.

Engine: `SpatialLighting/SpatialLightingEngine.*`  
Design: [SPATIAL_LIGHTING_ENGINE.md](SPATIAL_LIGHTING_ENGINE.md)

## Setup

1. Rebuild and load the plugin.
2. Layout: at least two controllers on opposite sides of the room.
3. Stack **reference** → **User position** (your seat). This does **not** move the light.
4. Prefer **manual room size** so placement corners match the visible room box.
5. Optional: one **display plane** between a strip and the test light.

## Test A — Room wash light

**Effect:** Room wash light  
**Purpose:** Soft fill at a fixed placement — easy to see falloff and blocking.

| Check | Pass? |
|-------|-------|
| LEDs near the light are brightest; far LEDs dimmer (not uniform strip color) | |
| **Brightness** and color wheel scale overall level and tint | |
| Placement (center / corner / custom %) moves the bright region in room space | |
| Plane between light and a strip: that strip drops sharply with **Shadows** on | |
| **Block through controller bodies** on → strips behind another controller darken | |
| **Ambient occlusion** higher → deeper shade in shielded spots | |
| LEDs in corners / behind geometry slightly darker (AO) | |

## Test B — Room campfire

**Effect:** Room campfire  
**Placement:** Corner (low) or Custom %

| Check | Pass? |
|-------|-------|
| Glow stays in corner when you change reference / user position | |
| **Core / Flame / Outer spill** swatches change hot vs cool regions | |
| LEDs above the fire read brighter than below (**Flame rise**) | |
| **Occlusion** off → blocking disabled; LEDs behind objects brighten | |
| **Block through controller bodies** on → strips behind another controller darken | |
| **Ambient occlusion** higher → deeper shade in shielded spots | |
| **Speed** > 0 → flicker; **Sparks** > 0 → occasional bright pops | |
| **Glow size** / **Light reach** (mm) change core vs how far light spreads | |
| **Room spill** low + **Shadows** on → clearer shadows (not a full-room wash) | |
| Changing spatial anchor does **not** move the fire | |

## Test C — Reference = viewer, not emitter

1. Campfire placement **Near corner (room min)** — note room min corner in viewport.
2. Change stack anchor to **World origin (0,0,0)** vs **User position**.
3. Fire should stay at the **same room corner**; brightness on strips may shift slightly. If the whole glow jumps to the anchor, rebuild — sample warp should be off.

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
3. Save layout, run spatial lighting with **Block through controller bodies** enabled.

## Legacy presets

Stacks saved with effect id **RoomLightProbe** load as **Room wash light** automatically.

## Not in scope yet

- Minecraft / game telemetry lighting  
- Screen mirror occlusion  
- Reference-point / small object meshes (beyond controller boxes)

## Report issues

Note: placement, plane position, which LEDs wrong (too bright / dark / no block), Qt version, screenshot if possible.
