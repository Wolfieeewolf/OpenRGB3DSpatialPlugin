# Effect / Event Maker (design)

Status: **production path** â€” Actions authoring + Triggers (Manual + Windows + Linux + macOS). Pack format version **4** only; bindings format version **1** only.

Captures the agreed direction: plugin-wide user-authored Actions bound to event catalogs (games, OS, manual, later media).

## End goal

User-authored **Actions** (no coding) play through OpenRGB when something happens:

| Trigger source | Examples |
|----------------|----------|
| Games | Minecraft damage, low health, weather, biome, â€¦ |
| OS | Lock/unlock, sleep/wake; Windows also focus/display/device/battery |
| Manual | Preview / test from the plugin UI |
| Media (later) | VLC / scripted Movie Syncs by timestamp |

Same packs everywhere. Only the **event catalog + bindings** change per source.

Inspired by amBX / Chroma-style â€œevent â†’ lookâ€, but editable without an SDK. Not a full xLights/Vixen show suite.

## Why not author in xLights / Vixen

- Would force a **second layout** (devices already live in this pluginâ€™s 3D map).
- Keyboards/mice/etc. donâ€™t map cleanly as E1.31 fixtures.
- Wrong timescale (season shows vs short event clips).

Optional later: import **baked** channel data. Primary authoring stays in-plugin against the existing LED map.

## Plugin packaging

- **v1:** authoring + player + bindings live in **3D Spatial**.
- **Portable contract:** Actions files + playback rules so other OpenRGB plugins could play the same packs later.
- Do **not** rely on plugins calling each other live; share **files + a player**.

## Actions (current)

- Timeline length: short clips up to about **60 seconds**.
- Modes: one-shot, loop forever, or loop **while event active**.
- Targets: All (pack) â†’ **scene zone** (Object Creator â†’ Zone) â†’ device â†’ OpenRGB HW zone â†’ LED.
- Scene zone rows mirror `ZoneManager3D` groups (Desk / Wall / â€¦), each with **All LEDs** plus member controllers. Drag controllers in the gutter to reorder (drives **Sequence** space).
- Timeline marks appear **only on the row matching that target** (no ghosting onto child rows).
- Pack `devices` lists scene controllers in scope (empty = whole scene).
- Catalog categories: **Basic** | **Pixel** | **Volume** (toolbar and right-click share `ActionCatalog`).
- Dual add UX: right-click Add effect, or drag from the **Tools** tab; drag colors / gradients / intensity curves onto clips.
- Authoring entry points: **Object Creator** â†’ Zone / Actions / **Triggers** (removed from the Run left column).
- Triggers file: `{PluginRoot}/triggers/triggers.json` (format `openrgb3d.triggers` v1).
- Event sources shown only for this OS/build: **Manual** always; **Windows** / **Linux** / **macOS** OS catalogs on their platforms. Game/app bridges later.
- Spatial sampling (`axis_space`):
  - **Device** (default on device / HW-zone / LED rows): per-controller local axes / AABB.
  - **Room** (default on All / scene zone): one shared **world** AABB across all LEDs in the target â€” Wipe, Ripple, Sphere Wipe, Fire, etc. continue across controllers using the 3D layout.
  - **Sequence**: Wipe/Chase/Spin/Meteor/Bars march along controllerâ†’LED order (zone drag order); Volume/Pixel types still use room XYZ.
  - Preset directions or custom yaw/pitch in Device/Room space.
  - Named directions aim at the matching viewport wall/side: Left/Right (Â±X), Down/Up (Â±Y),
    **Forward â†’ Front wall (Z=0 / âˆ’Z)**, **Back â†’ Back wall (+Z)**. Explicit `+z`/`-z` stay geometric.
- Storage: `{PluginRoot}/triggers/effects/*.action.json` (Triggers) or `{PluginRoot}/movie-sync/effects/<category>/` (Movie Sync). Format `openrgb3d.action` v4.
- Libraries start **empty** — install looks from [OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets), or author/Import in the Action Editor.

### Event edges

| Edge | Meaning | Pack loop notes |
|------|---------|-----------------|
| **Level** | `true` start / `false` end (lock, hold, display, battery, app focus) | `while_active` stops on `false` |
| **Pulse** | One-shot edge (device plug, logon, foreground_changed) | `while_active` is coerced to **Once** so playback ends |

### Clip types

| Category | Types |
|----------|--------|
| Basic | `solid`, `fade`, `pulse`, `wipe`, `chase`, `twinkle`, `alternating`, `strobe`, `spin`, `candle`, `dissolve`, `wave` |
| Pixel | `colorwash`, `plasma`, `snow`, `fire`, `balls`, `bars`, `scanner` |
| Volume | `spherewipe`, `orbit`, `ripple`, `meteor`, `noise3d`, `burst`, `field_ripple` (GPU field) |
| Media | `media_image`, `media_gif`, `media_text` — still/GIF/marquee mapped onto Device UV (PixelScreen / xLights matrix style) |

**Tools** (Basic / Pixel / Volume / Media) are **not** bundled — they load from `action-editor/toolbox/` after you install the presets pack. Editor chrome only: solid colors, gradients, and intensity/period curves in `ActionCatalog`.

### Action volume-field engine (Atmos sample)

World tools may declare `shader: <id>` (or `field:`) to bind a Volume-style `.fs` body (`volumeMain`). The Action applier prepares `SpatialVolumeFieldAssist` once per pack frame; `.fx` samples with `field_r` / `field_g` / `field_b` / `field_a` / `field_paint()` at the LED’s room-normalized XYZ. Bodies resolve from the toolbox tree (`<id>.fs` next to tools) or `effects/**/<id>.fs`. Ideas from reference VFX only — no vendored demos. Recipe map: [action-engine-recipe-map.md](action-engine-recipe-map.md).

## Pack file schema (v4)

Extension: `.action.json` (JSON, UTF-8). Version field required (`1`â€¦`4`).

Hand-authored packs: pack identity → playback → tracks → clip type / timing / brightness / colour / motion. `//` and `/* */` comments are accepted on load; saving from the in-app editor drops comments but keeps this field order. Templates and stock looks live in the [presets repo](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets). Folder map: [content-library.md](content-library.md).

```json
{
  "format": "openrgb3d.action",
  "version": 4,
  "id": "desk_ripple",
  "name": "Desk ripple",
  "duration_ms": 5000,
  "loop": "once",
  "priority": 10,
  "devices": ["Keyboard", "Mouse"],
  "tracks": [
    {
      "name": "Desk",
      "target": { "kind": "scene_zone", "scene_zone_name": "Desk" },
      "clips": [
        {
          "type": "ripple",
          "start_ms": 0,
          "end_ms": 2500,
          "direction": "right",
          "axis_space": "room",
          "axis_mode": "preset",
          "speed": 1.0,
          "intensity": 1.0,
          "gradient": [
            { "pos": 0.0, "color": "#00AAFF" },
            { "pos": 1.0, "color": "#FFFFFF" }
          ]
        }
      ]
    },
    {
      "name": "Desk All LEDs",
      "target": {
        "kind": "scene_zone",
        "scene_zone_name": "Desk",
        "flatten_leds": true
      },
      "clips": []
    }
  ]
}
```

### Fields

| Field | Notes |
| ----- | ----- |
| `loop` | `once` \| `forever` \| `while_active` |
| `duration_ms` | 1â€¦60000 |
| `priority` | Higher wins when multiple packs want the same LEDs |
| `devices` | Optional scene controller names; empty = all |
| `target.kind` | `all` \| `device` \| `zone` (OpenRGB HW zone) \| `leds` \| `scene_zone` (left-panel ZoneManager3D) |
| `target.scene_zone_name` | Required for `scene_zone` |
| `target.flatten_leds` | Optional; synthetic All LEDs row under a scene zone |
| `clips[].type` | See table above |
| `axis_space` | `device` \| `room` \| `sequence` â€” missing on All/scene_zone â†’ `room`; else v3+ â†’ `device`; v2 â†’ `room` |
| `axis_mode` | `preset` \| `custom` |
| `axis_yaw_deg` / `axis_pitch_deg` | Custom unit axis in the chosen space |
| `intensity_curve` | Optional `{pos,value}` envelope over **clip length** (0â€¦1) |
| `period_curve` | Optional `{pos,value}` waveform over each **period_ms** cycle (0â€¦1); multiplies with envelope |
| `period_ms` / `speed` | Cycle length and rate; `period_progress` available to `.fx` scripts |
| `min_intensity` / `max_intensity` | Script floor/ceiling (always editable in Brightness) |
| `media_path` | Image/GIF file for `media_image` / `media_gif` |
| `media_text` | Marquee string for `media_text` |
| `media_scroll` | Scroll media across Device/Room UV (direction + speed + period) |

## Triggers (v1)

```json
{
  "format": "openrgb3d.triggers",
  "version": 1,
  "bindings": [
    {
      "id": "bind_â€¦",
      "enabled": true,
      "source": "manual",
      "event": "fire",
      "pack_id": "desk_ripple"
    }
  ]
}
```

| Source | Events | Platform |
|--------|--------|----------|
| `manual` | `fire`, `hold` | All |
| `windows` | See table below | Windows only |
| `linux` | lock/unlock, sleep/resume | Linux (systemd-logind) |
| `macos` | lock/unlock, sleep/resume | macOS |

### Windows events (Sound control-panel aligned)

| Event id | UI label (â‰ˆ Windows Sounds) | Edge | How detected |
|----------|----------------------------|------|--------------|
| `session_lock` / `session_unlock` | Windows Lock / Unlock | Level | WTS session |
| `session_logon` / `session_logoff` | Windows Logon / Logoff | Pulse | WTS session |
| `device_connect` / `device_disconnect` / `device_fail` | Device Connect / Disconnect / Failed | Pulse | USB/HID `WM_DEVICECHANGE` |
| `battery_low` / `battery_critical` | Low / Critical Battery Alarm | Level | battery % (â‰¤10% / â‰¤5%; clears on recover) |
| `display_off` / `display_on` | Display off / on | Level | console display power |
| `remote_connect` / `remote_disconnect` | Remote desktop | Level | WTS remote |
| `foreground_changed` | Foreground window changed | Pulse | WinEvent hook |
| `app_activated` / `app_deactivated` | OpenRGB foreground | Level | process focus |

**Not available (yet / ever without hooks):** classic MessageBeep sounds â€” Asterisk, Critical Stop, Exclamation, Question, Default Beep. Those are played by apps via `MessageBeep`/`PlaySound` with no system-wide listener API short of injecting into every process. Toast / Action Center notifications need WinRT `UserNotificationListener` + user consent (deferred).

### Linux events (logind)

| Event id | UI label | Edge | How detected |
|----------|----------|------|--------------|
| `session_lock` / `session_unlock` | Session lock / unlock | Level | `org.freedesktop.login1.Session` `LockedHint` |
| `prepare_for_sleep` / `resume` | About to sleep / resumed | Level | logind `PrepareForSleep` |

Requires Qt DBus + systemd-logind on the system bus. No-op if logind is unavailable.

### macOS events

| Event id | UI label | Edge | How detected |
|----------|----------|------|--------------|
| `session_lock` / `session_unlock` | Screen lock / unlock | Level | `com.apple.screenIsLocked` / `Unlocked` |
| `prepare_for_sleep` / `resume` | About to sleep / resumed | Level | IOKit system power |

UI: Object Creator â†’ Triggers. Prefer **Once** packs for edge events (`device_connect`, `foreground_changed`, logon, â€¦).

## Runtime spine

```text
Event catalog (Manual / Windows / Linux / macOS â€” later games + apps)
        → triggers/triggers.json
Actions (.action.json)
        â†“ TriggerRuntime + pack player (priority order)
LED frames over time â†’ OpenRGB + 3D viewport (layout-aware)
```

## Build order

1. Pack format + player (incl. ~60s + loop + priority). **done**
2. Effects library UI (create/preview/save with minimal tools). **done**
3. Bindings UI â€” Manual + Windows session/focus/display events. **done** (toast notifications later)
4. Linux + macOS OS sources. **done**
5. Production clean: event edges, LogManager tracing, CONTRIBUTING chrome/legacy. **done**
6. Structural split of editor/timeline/pack eval mega-files. **done** (Pass 1)
7. Pack-eval dispatcher + ActionApplier peel. **done**
8. Later: Minecraft bindings / media cues / WinRT toasts / Discord.

## Non-goals for v1

- Endless timeline / full Christmas-show sequencing.
- Pixel-perfect multi-minute movie editors.
- Live E1.31 bridge as the main workflow.
- Cross-plugin in-process API.

## Code home

- `Effects3D/Actions/Action.*` — types, playback map, track color
- `Effects3D/Actions/ActionSerialize.cpp` — load/save JSON (`openrgb3d.action`, `.action.json`)
- `Effects3D/Actions/ActionLibrary.*` — scan `triggers/effects/` (no bundled seed content)
- `ui/ActionCatalog.h` — Action Editor Tools (`action-editor/toolbox/`)
- `ui/ActionsPanel.*` — Object Creator → Actions
- `ui/TriggersPanel.*` — Object Creator → Triggers
- `Effects3D/Triggers/*` — `triggers.json`, sources, TriggerRuntime
- `ui/ActionEditorDialog.*` — Action Editor
- `Effects3D/MovieSync/*` — Movie Sync player + catalog
- `PluginSettingsPaths` — feature folders under plugin data root
