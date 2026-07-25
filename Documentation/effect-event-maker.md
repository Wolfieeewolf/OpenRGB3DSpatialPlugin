# Effect / Event Maker (design)

Status: **in progress** — viewport-synced preview + timeline pack editor; event bindings next.

Captures the agreed direction: plugin-wide user-authored effect packs bound to event catalogs (games, Windows, manual, later media).

## End goal

User-authored **effect packs** (no coding) play through OpenRGB when something happens:

| Trigger source | Examples |
|----------------|----------|
| Games | Minecraft damage, low health, weather, biome, … |
| Windows / OS | Notifications, lock/unlock, focus (later) |
| Manual | Preview / test from the plugin UI |
| Media (later) | VLC / scripted movie cues by timestamp |

Same packs everywhere. Only the **event catalog + bindings** change per source.

Inspired by amBX / Chroma-style “event → look”, but editable without an SDK. Not a full xLights/Vixen show suite.

## Why not author in xLights / Vixen

- Would force a **second layout** (devices already live in this plugin’s 3D map).
- Keyboards/mice/etc. don’t map cleanly as E1.31 fixtures.
- Wrong timescale (season shows vs short event clips).

Optional later: import **baked** channel data. Primary authoring stays in-plugin against the existing LED map.

## Plugin packaging

- **v1:** authoring + player + bindings live in **3D Spatial**.
- **Portable contract:** effect pack files + playback rules so other OpenRGB plugins could play the same packs later.
- Do **not** rely on plugins calling each other live; share **files + a player**.

## Effect packs (current)

- Timeline length: short clips up to about **60 seconds**.
- Modes: one-shot, loop forever, or loop **while event active**.
- Targets: all LEDs → device → zone → LED.
- Timeline marks appear **only on the row matching that target** (no ghosting onto child rows).
- Pack `devices` lists scene controllers in scope (empty = whole scene).
- Catalog categories: **Basic** | **Pixel** | **Volume** (toolbar and right-click share `EffectPackCatalog`).
- Dual add UX: right-click Add effect, or drag from the effect toolbar; drag colors / gradients / intensity curves onto blocks.
- Spatial sampling:
  - **Device** space (default on device/zone/LED rows): axes follow that controller’s viewport orientation.
  - **Room** space (default on All): one shared world AABB across pack devices in scope.
  - Preset directions or custom yaw/pitch in the chosen space.
- Volume / field types sample full XYZ; wipe/chase/spin use axis or angle in that space.
- Storage: `{PluginRoot}/effect-packs/*.oreffect.json` (format version **3**; v1–v2 still load).

### Block types

| Category | Types |
|----------|--------|
| Basic | `solid`, `fade`, `pulse`, `wipe`, `chase`, `twinkle`, `alternating`, `strobe`, `spin`, `candle`, `dissolve` |
| Pixel | `colorwash`, `plasma`, `snow`, `fire`, `balls`, `bars` |
| Volume | `spherewipe`, `orbit`, `ripple`, `meteor`, `noise3d` |

## Pack file schema (v3)

Extension: `.oreffect.json` (JSON, UTF-8). Version field required (`1`…`3`).

```json
{
  "format": "openrgb3d.effect_pack",
  "version": 3,
  "id": "rainbow_wash",
  "name": "Rainbow wash",
  "duration_ms": 60000,
  "loop": "forever",
  "priority": 10,
  "devices": ["Keyboard"],
  "tracks": [
    {
      "name": "All LEDs",
      "target": { "kind": "all" },
      "blocks": [
        {
          "type": "wipe",
          "start_ms": 0,
          "end_ms": 2000,
          "direction": "right",
          "axis_space": "device",
          "axis_mode": "preset",
          "axis_yaw_deg": 0.0,
          "axis_pitch_deg": 0.0,
          "speed": 1.0,
          "intensity": 1.0,
          "gradient": [
            { "pos": 0.0, "color": "#FF0000" },
            { "pos": 1.0, "color": "#FFFFFF" }
          ],
          "intensity_curve": [
            { "pos": 0.0, "value": 0.0 },
            { "pos": 0.5, "value": 1.0 },
            { "pos": 1.0, "value": 0.0 }
          ]
        }
      ]
    }
  ]
}
```

### Fields

| Field | Notes |
| ----- | ----- |
| `loop` | `once` \| `forever` \| `while_active` |
| `duration_ms` | 1…60000 |
| `priority` | Higher wins when multiple packs want the same LEDs |
| `devices` | Optional scene controller names; empty = all |
| `target.kind` | `all` \| `device` \| `zone` \| `leds` |
| `blocks[].type` | See table above |
| `axis_space` | `device` \| `room` (v2 missing → room; v3 missing → device) |
| `axis_mode` | `preset` \| `custom` |
| `axis_yaw_deg` / `axis_pitch_deg` | Custom unit axis in the chosen space |
| `intensity_curve` | Optional `{pos,value}` points 0…1 |

## Runtime spine

```text
Event catalog (game / Windows / manual / media)
        ↓ binding table (default or user override)
Effect pack (.oreffect.json)
        ↓ player
LED frames over time → OpenRGB + 3D viewport (layout-aware)
```

## Build order

1. Pack format + player (incl. ~60s + loop + priority).
2. Effects library UI (create/preview/save with minimal tools).
3. Bindings UI — Manual + at least one real source (Minecraft or Windows).
4. Grow catalogs and authoring tools; media/VLC cues when packs are solid.

## Non-goals for v1

- Endless timeline / full Christmas-show sequencing.
- Pixel-perfect multi-minute movie editors.
- Live E1.31 bridge as the main workflow.
- Cross-plugin in-process API.

## Code home

- `Effects3D/EffectPacks/EffectPack.*` — load/save/axis evaluate
- `Effects3D/EffectPacks/EffectPackSpatial.cpp` — curves, world/volume evaluate
- `Effects3D/EffectPacks/EffectPackPlayer.h` — playback clock
- `Effects3D/EffectPacks/EffectPackApplier.*` — hardware + viewport apply
- `Effects3D/EffectPacks/EffectPackLibrary.*` — scan/seed `effect-packs/`
- `ui/EffectPackCatalog.h` — shared Basic/Pixel/Volume catalog
- `ui/EffectPackPanel.*` — Run-tab list + Preview/Stop / New / Edit
- `ui/EffectPackEditorDialog.*` — modeless editor
- `ui/EffectPackTimelineWidget.*` — ruler / rows / blocks / playhead
- `ui/EffectPackToolBar.*` — effect/color/gradient/curve strips
- `ui/EffectPackGradientBar.*` — gradient stops UI
- `PluginSettingsPaths::EffectPacksDir` — `{PluginRoot}/effect-packs`
