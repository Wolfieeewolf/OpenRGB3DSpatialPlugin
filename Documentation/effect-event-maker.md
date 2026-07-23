# Effect / Event Maker (design)

Status: **in progress** — viewport-synced preview + xLights-style timeline editor; event bindings next.

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

## Effect packs (v1)

- Timeline length: short clips up to about **60 seconds** (extend later if needed).
- Modes: one-shot, loop forever, or loop **while event active**.
- Targets: all LEDs → device → zone → LED (coarse first; fine only when needed).
- Simple tools: solid colour, fade, pulse/chase-style blocks (grow later).
- Metadata: duration, loop mode, priority (so a toast can interrupt a rainbow, then resume).
- Storage: `{PluginRoot}/effect-packs/*.oreffect.json`

## Pack file schema (v1 draft)

Extension: `.oreffect.json` (JSON, UTF-8). Version field required.

```json
{
  "format": "openrgb3d.effect_pack",
  "version": 1,
  "id": "rainbow_wash",
  "name": "Rainbow wash",
  "duration_ms": 60000,
  "loop": "forever",
  "priority": 10,
  "tracks": [
    {
      "name": "All LEDs",
      "target": { "kind": "all" },
      "blocks": [
        {
          "type": "solid",
          "start_ms": 0,
          "end_ms": 60000,
          "color": "#FF0000",
          "intensity": 1.0
        }
      ]
    }
  ]
}
```

### Fields

| Field | Notes |
|-------|--------|
| `loop` | `once` \| `forever` \| `while_active` |
| `duration_ms` | 1…60000 in v1 |
| `priority` | Higher wins when multiple packs want the same LEDs |
| `target.kind` | `all` \| `device` \| `zone` \| `leds` |
| `target` extras | `device_name`, `zone_name`, `led_indices` as needed |
| `blocks[].type` | v1: `solid`, `fade`, `pulse` |

### Block types (v1)

- **solid** — constant `color` + `intensity` over `[start_ms, end_ms)`
- **fade** — `color_from` → `color_to` over the span
- **pulse** — `color`, `period_ms`, optional `min_intensity` / `max_intensity`

## Runtime spine

```text
Event catalog (game / Windows / manual / media)
        ↓ binding table (default or user override)
Effect pack (.oreffect.json)
        ↓ player
LED frames over time → existing OpenRGB / 3D Spatial output
```

Built-in C++ effects can remain as defaults until replaced by packs.

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

## Related product notes

- Game APIs / mods publish **events + telemetry**; looks stay in effect packs.
- Long movie/game beats: prefer **loop while active** or chained cues over a 20‑minute ruler.

## Code home

- `Effects3D/EffectPacks/EffectPack.*` — load/save/evaluate (solid, fade, pulse)
- `Effects3D/EffectPacks/EffectPackPlayer.h` — playback clock
- `Effects3D/EffectPacks/EffectPackApplier.*` — hardware + viewport `preview_color` apply
- `Effects3D/EffectPacks/EffectPackLibrary.*` — scan/seed `effect-packs/` folder
- `ui/EffectPackPanel.*` — Run-tab list + Preview/Stop / New / Edit
- `ui/EffectPackEditorDialog.*` — modeless editor: models left, timeline center, props right
- `ui/EffectPackTimelineWidget.*` — ruler / rows / blocks / playhead (Ctrl+wheel zoom)
- `PluginSettingsPaths::EffectPacksDir` — `{PluginRoot}/effect-packs`
- Example pack: `Documentation/examples/rainbow_wash.oreffect.json`
- Bindings JSON later beside packs (e.g. `effect-bindings.json`)
