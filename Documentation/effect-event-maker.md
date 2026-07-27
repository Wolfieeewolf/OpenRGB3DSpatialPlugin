# Effect / Event Maker (design)

Status: **production path** — effect pack authoring + Event Bindings (Manual + Windows + Linux + macOS). Pack format version **4** only; bindings format version **1** only.

Captures the agreed direction: plugin-wide user-authored effect packs bound to event catalogs (games, OS, manual, later media).

## End goal

User-authored **effect packs** (no coding) play through OpenRGB when something happens:

| Trigger source | Examples |
|----------------|----------|
| Games | Minecraft damage, low health, weather, biome, … |
| OS | Lock/unlock, sleep/wake; Windows also focus/display/device/battery |
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
- Targets: All (pack) → **scene zone** (Object Creator → Zone) → device → OpenRGB HW zone → LED.
- Scene zone rows mirror `ZoneManager3D` groups (Desk / Wall / …), each with **All LEDs** plus member controllers. Drag controllers in the gutter to reorder (drives **Sequence** space).
- Timeline marks appear **only on the row matching that target** (no ghosting onto child rows).
- Pack `devices` lists scene controllers in scope (empty = whole scene).
- Catalog categories: **Basic** | **Pixel** | **Volume** (toolbar and right-click share `EffectPackCatalog`).
- Dual add UX: right-click Add effect, or drag from the effect toolbar; drag colors / gradients / intensity curves onto blocks.
- Authoring entry points: **Object Creator** → Zone / Effect Pack / **Event Bindings** (removed from the Run left column).
- Bindings file: `{PluginRoot}/effect-bindings.json` (format `openrgb3d.effect_bindings` v1).
- Event sources shown only for this OS/build: **Manual** always; **Windows** / **Linux** / **macOS** OS catalogs on their platforms. Game/app bridges later.
- Spatial sampling (`axis_space`):
  - **Device** (default on device / HW-zone / LED rows): per-controller local axes / AABB.
  - **Room** (default on All / scene zone): one shared **world** AABB across all LEDs in the target — Wipe, Ripple, Sphere Wipe, Fire, etc. continue across controllers using the 3D layout.
  - **Sequence**: Wipe/Chase/Spin/Meteor/Bars march along controller→LED order (zone drag order); Volume/Pixel types still use room XYZ.
  - Preset directions or custom yaw/pitch in Device/Room space.
- Storage: `{PluginRoot}/effect-packs/*.oreffect.json` (format version **4** only).
- Empty library seeds three examples: rainbow wash, desk ripple (Room), sequence wipe.

### Event edges

| Edge | Meaning | Pack loop notes |
|------|---------|-----------------|
| **Level** | `true` start / `false` end (lock, hold, display, battery, app focus) | `while_active` stops on `false` |
| **Pulse** | One-shot edge (device plug, logon, foreground_changed) | `while_active` is coerced to **Once** so playback ends |

### Block types

| Category | Types |
|----------|--------|
| Basic | `solid`, `fade`, `pulse`, `wipe`, `chase`, `twinkle`, `alternating`, `strobe`, `spin`, `candle`, `dissolve`, `wave` |
| Pixel | `colorwash`, `plasma`, `snow`, `fire`, `balls`, `bars`, `scanner` |
| Volume | `spherewipe`, `orbit`, `ripple`, `meteor`, `noise3d`, `burst` |

Shared toolbar/props presets live in `EffectPackCatalog`: solid colors (incl. warm white / cyan / magenta / dim gray), gradients (`rainbow`, `red_blue`, `white_color`, `fire`, `ice`, `forest`, `sunset`, `cyber`), and intensity curves (`flat`, `triangle`, `ease_in`, `ease_out`, `pulse_curve`, `hold_peak`, `snap`).

## Pack file schema (v4)

Extension: `.oreffect.json` (JSON, UTF-8). Version field required (`1`…`4`).

```json
{
  "format": "openrgb3d.effect_pack",
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
      "blocks": [
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
      "blocks": []
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
| `target.kind` | `all` \| `device` \| `zone` (OpenRGB HW zone) \| `leds` \| `scene_zone` (left-panel ZoneManager3D) |
| `target.scene_zone_name` | Required for `scene_zone` |
| `target.flatten_leds` | Optional; synthetic All LEDs row under a scene zone |
| `blocks[].type` | See table above |
| `axis_space` | `device` \| `room` \| `sequence` — missing on All/scene_zone → `room`; else v3+ → `device`; v2 → `room` |
| `axis_mode` | `preset` \| `custom` |
| `axis_yaw_deg` / `axis_pitch_deg` | Custom unit axis in the chosen space |
| `intensity_curve` | Optional `{pos,value}` points 0…1 |

## Event bindings (v1)

```json
{
  "format": "openrgb3d.effect_bindings",
  "version": 1,
  "bindings": [
    {
      "id": "bind_…",
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

| Event id | UI label (≈ Windows Sounds) | Edge | How detected |
|----------|----------------------------|------|--------------|
| `session_lock` / `session_unlock` | Windows Lock / Unlock | Level | WTS session |
| `session_logon` / `session_logoff` | Windows Logon / Logoff | Pulse | WTS session |
| `device_connect` / `device_disconnect` / `device_fail` | Device Connect / Disconnect / Failed | Pulse | USB/HID `WM_DEVICECHANGE` |
| `battery_low` / `battery_critical` | Low / Critical Battery Alarm | Level | battery % (≤10% / ≤5%; clears on recover) |
| `display_off` / `display_on` | Display off / on | Level | console display power |
| `remote_connect` / `remote_disconnect` | Remote desktop | Level | WTS remote |
| `foreground_changed` | Foreground window changed | Pulse | WinEvent hook |
| `app_activated` / `app_deactivated` | OpenRGB foreground | Level | process focus |

**Not available (yet / ever without hooks):** classic MessageBeep sounds — Asterisk, Critical Stop, Exclamation, Question, Default Beep. Those are played by apps via `MessageBeep`/`PlaySound` with no system-wide listener API short of injecting into every process. Toast / Action Center notifications need WinRT `UserNotificationListener` + user consent (deferred).

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

UI: Object Creator → Event Bindings. Prefer **Once** packs for edge events (`device_connect`, `foreground_changed`, logon, …).

## Runtime spine

```text
Event catalog (Manual / Windows / Linux / macOS — later games + apps)
        ↓ effect-bindings.json
Effect pack (.oreffect.json)
        ↓ BindingRuntime + pack player (priority order)
LED frames over time → OpenRGB + 3D viewport (layout-aware)
```

## Build order

1. Pack format + player (incl. ~60s + loop + priority). **done**
2. Effects library UI (create/preview/save with minimal tools). **done**
3. Bindings UI — Manual + Windows session/focus/display events. **done** (toast notifications later)
4. Linux + macOS OS sources. **done**
5. Production clean: event edges, LogManager tracing, CONTRIBUTING chrome/legacy. **done**
6. Structural split of editor/timeline/pack eval mega-files. **done** (Pass 1)
7. Pack-eval dispatcher + EffectPackApplier peel. **done**
8. Later: Minecraft bindings / media cues / WinRT toasts / Discord.

## Non-goals for v1

- Endless timeline / full Christmas-show sequencing.
- Pixel-perfect multi-minute movie editors.
- Live E1.31 bridge as the main workflow.
- Cross-plugin in-process API.

## Code home

- `Effects3D/EffectPacks/EffectPack.*` — core pack types, playback map, track color
- `Effects3D/EffectPacks/EffectPackSerialize.cpp` — load/save JSON
- `Effects3D/EffectPacks/EffectPackBlockEvalAxis.cpp` — axis / LED block eval
- `Effects3D/EffectPacks/EffectPackExamples.cpp` — seed packs
- `Effects3D/EffectPacks/EffectPackSpatial.cpp` — curves, world/volume evaluate
- `Effects3D/EffectPacks/EffectPackPlayer.h` — playback clock
- `Effects3D/EffectPacks/EffectPackApplier*.cpp` — hardware + viewport apply (frame / match / spatial)
- `Effects3D/EffectPacks/EffectPackLibrary.*` — scan/seed `effect-packs/`
- `ui/EffectPackCatalog.h` — shared Basic/Pixel/Volume catalog
- `ui/EffectPackPanel.*` — Object Creator → Effect Pack list + Preview/Stop / New / Edit
- `ui/ZonesPanel.*` — Object Creator → Zone list + Create / Edit / Delete
- `ui/EventBindingsPanel.*` — Object Creator → Event Bindings list + Fire/Hold/Stop
- `Effects3D/EventBindings/*` — binding file, sources, BindingRuntime
- `ui/EffectPackEditorDialog.*` (+ `EffectPackEditorProps`, `EffectPackEditorTimelineBridge`) — modeless editor
- `ui/EffectPackTimelineWidget.*` (+ `EffectPackTimelinePaint`, `EffectPackTimelineInteract`) — ruler / rows / blocks / playhead
- `ui/EffectPackToolBar.*` — effect/color/gradient/curve strips
- `ui/EffectPackGradientBar.*` — gradient stops UI
- `PluginSettingsPaths::EffectPacksDir` — `{PluginRoot}/effect-packs`
