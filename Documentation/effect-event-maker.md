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
- **Portable contract:** effect pack files + playback rules so other OpenRGB plugins (Effects, Visual Map, …) could play the same packs later.
- Extract a standalone “Effect Studio” plugin only if the editor grows large enough to deserve its own home.

Do **not** rely on plugins calling each other live; share **files + a player**.

## Effect packs (current)

- Timeline length: short clips up to about **60 seconds**.
- Modes: one-shot, loop forever, or loop **while event active**.
- Targets: all LEDs → device → zone → LED.
- Timeline marks appear **only on the row matching that target** (no ghosting onto child zone/LED rows). A wipe on a device uses the whole-device canvas; the same wipe on one LED is a different effect.
- Pack `devices` lists scene controllers in scope (empty = whole scene).
- Tools: Set Level, Fade, Pulse, Wipe, Chase, Twinkle, ColorWash (Basic + Pixel catalog; more types planned).
- Dual add UX (Vixen/xLights-style): **right-click** Add effect (Basic/Pixel) and **drag** from the effect toolbar onto a row; drag colors/gradients onto blocks.
- Gradients, direction (grid/world Left/Right/Up/Down), speed, chase length.
- Spatial wipe/chase/colorwash use **3D layout world positions**, not raw OpenRGB LED index order.
- Storage: `{PluginRoot}/effect-packs/*.oreffect.json` (`format` version **2**).
- Later: curve + gradient libraries (intensity curves, saved presets) — see plan `vixen_xlights_compare`.

## Pack file schema (v2)

Extension: `.oreffect.json` (JSON, UTF-8). Version field required (`1`…`2`).

```json
{
  "format": "openrgb3d.effect_pack",
  "version": 2,
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
          "speed": 1.0,
          "intensity": 1.0,
          "gradient": [
            { "pos": 0.0, "color": "#FF0000" },
            { "pos": 1.0, "color": "#FFFFFF" }
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
| `blocks[].type` | `solid`, `fade`, `pulse`, `wipe`, `chase`, `twinkle`, `colorwash` |

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

- `Effects3D/EffectPacks/EffectPack.*` — load/save/evaluate
- `Effects3D/EffectPacks/EffectPackPlayer.h` — playback clock
- `Effects3D/EffectPacks/EffectPackApplier.*` — hardware + viewport apply (throttled `UpdateLEDs`)
- `Effects3D/EffectPacks/EffectPackLibrary.*` — scan/seed `effect-packs/`
- `ui/EffectPackPanel.*` — Run-tab list + Preview/Stop / New / Edit
- `ui/EffectPackEditorDialog.*` — modeless editor
- `ui/EffectPackTimelineWidget.*` — ruler / rows / blocks / playhead
- `ui/EffectPackGradientBar.*` — gradient stops UI
- `PluginSettingsPaths::EffectPacksDir` — `{PluginRoot}/effect-packs`
