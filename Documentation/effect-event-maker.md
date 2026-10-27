# Effect / Event Maker (design)

Status: **production path** - Actions authoring + Triggers (Manual + Windows + Linux + macOS). Pack format version **5** (v4 loads and auto-upgrades on open); bindings format version **1** only.

Captures the agreed direction: plugin-wide user-authored Actions bound to event catalogs (games, OS, manual, later media).

## End goal

User-authored **Actions** (no coding) play through OpenRGB when something happens:

| Trigger source | Examples |
|----------------|----------|
| Games | Minecraft damage, low health, weather, biome, ... |
| OS | Lock/unlock, sleep/wake; Windows also focus/display/device/battery |
| Manual | Preview / test from the plugin UI |
| Media (later) | VLC / scripted Movie Syncs by timestamp |

Same packs everywhere. Only the **event catalog + bindings** change per source.

Inspired by amBX / Chroma-style "event -> look", but editable without an SDK. Not a full xLights/Vixen show suite.

## Why not author in xLights / Vixen

- Would force a **second layout** (devices already live in this plugin's 3D map).
- Keyboards/mice/etc. don't map cleanly as E1.31 fixtures.
- Wrong timescale (season shows vs short event clips).

Optional later: import **baked** channel data. Primary authoring stays in-plugin against the existing LED map.

## Plugin packaging

- **v1:** authoring + player + bindings live in **3D Spatial**.
- **Portable contract:** Actions files + playback rules so other OpenRGB plugins could play the same packs later.
- Do **not** rely on plugins calling each other live; share **files + a player**.

## Actions (current)

- Timeline length: short clips up to about **60 seconds**.
- Modes: one-shot, loop forever, or loop **while event active**.
- Targets: All (pack) -> **scene zone** (Object Creator -> Zone) -> device -> OpenRGB HW zone -> LED.
- Scene zone rows mirror `ZoneManager3D` groups (Desk / Wall / ...), each with **All LEDs** plus member controllers. Drag controllers in the gutter to reorder (drives **Sequence** space).
- Timeline marks appear **only on the row matching that target** (no ghosting onto child rows).
- Pack `devices` lists scene controllers in scope (empty = whole scene).
- Catalog categories: **Basic** | **Pixel** | **Volume** (toolbar and right-click share `ActionCatalog`).
- Dual add UX: right-click Add effect, or drag from the **Tools** tab; drag colors / gradients / intensity curves onto clips.
- Authoring entry points: **Object Creator** -> Zone / Actions / **Triggers** (removed from the Run left column).
- Triggers file: `{PluginRoot}/triggers/triggers.json` (format `openrgb3d.triggers` v1).
- Event sources shown only for this OS/build: **Manual** always; **Windows** / **Linux** / **macOS** OS catalogs on their platforms. Game/app bridges later.
- Spatial sampling:
  - **Play on** (`play_bounds`): `track_lights` (this row's LEDs) or `room_box` (full room). Media defaults to track lights.
  - **Travel** (`axis_space`): Device / Room / Sequence  -  coordinate frame for direction and wipes.
  - **Origin** (`origin_ref`): layout reference point name, or empty = room center.
  - Preset directions or custom yaw/pitch; Left/Right (+/-X), Down/Up (+/-Y), Forward/Back (+/-Z).
- Storage: `{PluginRoot}/triggers/effects/*.action.json` (Triggers) or `{PluginRoot}/movie-sync/effects/<category>/` (Movie Sync). Format `openrgb3d.action` v5 (v4 accepted and rewritten on load).
- Libraries start **empty**  -  install looks from [OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets), or author/Import in the Action Editor.

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
| Volume | `spherewipe`, `orbit`, `ripple`, `meteor`, `noise3d`, `burst`, `field_ripple` / `field_strata` / `field_burst` (GPU field), `spark_burst` / `particle_rain` / `particle_fireflies` (GPGPU particles), `neon_ribbon` / `ribbon_helix` (GPGPU ribbons), `plasma` / `fire` / `snow` / `colorwash` (built-in GPU when no toolbox `shader:`) |
| Media | `media_image`, `media_gif`, `media_text`  -  still/GIF/marquee mapped onto track UV (PixelScreen / xLights matrix style) |

**Tools** (Basic / Pixel / Volume / Media) are **not** bundled  -  they load from `action-editor/toolbox/` after you install the presets pack. Editor chrome only: solid colors, gradients, and intensity/period curves in `ActionCatalog`.

### Play space (Action-native, not stack Spatial Anchor)

Actions are for Movie Sync cues, triggers, and event packs. They do **not** use the effect-stack Spatial Anchor combo. Each clip carries:

| Control | Values | Meaning |
|---------|--------|---------|
| **Play on** (`play_bounds`) | `track_lights` (default on device rows / media) \| `room_box` | AABB used to normalize UV / world samples  -  keyboard marquee stays on the keyboard when track-local |
| **Travel** (`axis_space`) | `device` \| `room` \| `sequence` | Coordinate frame for direction / wipes |
| **Origin** (`origin_ref`) | Layout reference point **name**, or empty = room center | Cue centre  -  same viewport refs as the rest of the plugin |

Stack Spatial Anchor and Action `origin_ref` both read the **same layout reference list**. They do not overwrite each other: a Movie Sync layer on `me` does not rewrite an Action wash authored at room center.

**Named refs = surround calibration.** Prefer short role names that travel across rooms: `me`, `chair`, `lounge`, `screen`, `tv`, `monitor`, `front`, `rear`, `left`, `right`, `ceiling`, `floor`, `top left`, ... Place matching names on your grid; packs resolve by name. Opening a pack lists required refs and any missing ones. Map Controllers remains the escape hatch for Advanced device-name remaps.

**Content tiers (same movie, different ambition):** ship Basic / Intermediate / Advanced packs (folders/names only  -  no special engine). Basic ≈ room washes + simple L↔R; Intermediate ≈ more refs/zones; Advanced ≈ device tracks + layout share.

### Action volume-field engine (Roomscape sample)

World tools may declare `shader: <id>` (or `field:`) to bind a Volume-style `.fs` body (`volumeMain`). The Action applier prepares the program once per pack frame, then **evaluates `volumeMain` at each LED’s exact room-normalized XYZ** (OpenGL 4.1 required — true-GPU fail-closed, no CPU dual). `.fx` `field_*` / `field_paint()` sample the same GPU path. Bodies resolve from the toolbox tree (`<id>.fs` next to tools) or `effects/**/<id>.fs`.

World tools may declare `particles: burst` for a room particle field. Particle motion lives in GLSL `volumeMain` (GPGPU); LED apply runs that shader at each LED. GL required — if unavailable the clip paints nothing (fail closed).

World tools may declare `ribbons: arc` for a room ribbon/tube. Curve sampling lives in GLSL `volumeMain`; same true point-eval / fail-closed rules. Ideas from reference VFX only — no vendored demos. Recipe map: [action-engine-recipe-map.md](action-engine-recipe-map.md).

**GL pool / thumbs:** field, particle, ribbon, and built-in GPU clips share **`SpatialOffscreenGlPool`** with the room viewport (no second offscreen context). Action Editor timeline thumbs use the same pool — deferred, cached, fail closed to black (no gradient / CPU fake).

**Clocks / progress:** once fades and envelopes use `ClipSpanProgress`; script `progress` uses `ClipSweepProgress`; forever / `while_active` ambient time uses `GpuAnimMs` / `GpuAnimMsOr` (continuous player elapsed — not wrapping `local_ms`).

**Clear-on-idle:** every path that empties the action player (Stop, uncheck, level-off, natural end, Movie Sync / Triggers stop) must call `ClearActionPreview` — not only the timer-finish path.

**Toolbox headers:** same-engine sibling `.fx` tools share one header skeleton (icons / knobs / paint). If an `.fx` has `shader:` and a `pulse` knob, the paired `.fs` must read size via `param: size` / `u_params[2]`. UI copy stays human-readable (no raw ids / placeholder junk).

**Seamless play:** looping packs should animate from `u_time` (continuous); once packs should finish at black. See [action-engine-recipe-map.md](action-engine-recipe-map.md#playback-seams-once-vs-loop).

**Placement (world / field / particle / ribbon):** clip `origin_ref` (layout reference point name, or empty = room center), `origin_offset_x/y/z` (% of half-room), and `effect_size` (0.05–2.0). Scripts can read `ox` / `oy` / `oz` / `effect_size`. Sample normalize uses `play_bounds` (track LED AABB vs full room box).

## Pack file schema (v5)

Extension: `.action.json` (JSON, UTF-8). Version field required (`4` or `5`). Saves write `5`. Loading a `4` pack fills missing `play_bounds` (RoomBox on All/scene_zone, TrackLights on device/media) and rewrites the file as `5`. Versions outside that range are rejected.

Hand-authored packs: pack identity -> playback -> tracks -> clip type / timing / brightness / colour / motion. `//` and `/* */` comments are accepted on load; saving from the in-app editor drops comments but keeps this field order. Templates and stock looks live in the [presets repo](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets). Folder map: [content-library.md](content-library.md).

```json
{
  "format": "openrgb3d.action",
  "version": 5,
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
          "play_bounds": "room_box",
          "origin_ref": "me",
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
| `duration_ms` | 1...60000 |
| `priority` | Higher wins when multiple packs want the same LEDs |
| `devices` | Optional scene controller names; empty = all |
| `target.kind` | `all` \| `device` \| `zone` (OpenRGB HW zone) \| `leds` \| `scene_zone` (left-panel ZoneManager3D) |
| `target.scene_zone_name` | Required for `scene_zone` |
| `target.flatten_leds` | Optional; synthetic All LEDs row under a scene zone |
| `clips[].type` | See table above |
| `axis_space` | `device` \| `room` \| `sequence`  -  travel frame; default Room on All/scene_zone, else Device |
| `play_bounds` | `track_lights` \| `room_box`  -  UV/world canvas; default RoomBox on All/scene_zone, else TrackLights |
| `origin_ref` | Layout reference point name; empty -> room center |
| `axis_mode` | `preset` \| `custom` |
| `axis_yaw_deg` / `axis_pitch_deg` | Custom unit axis in the chosen travel frame |
| `intensity_curve` | Optional `{pos,value}` envelope over **clip length** (0...1) |
| `period_curve` | Optional `{pos,value}` waveform over each **period_ms** cycle (0...1); multiplies with envelope |
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
      "id": "bind_...",
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

| Event id | UI label (~ Windows Sounds) | Edge | How detected |
|----------|----------------------------|------|--------------|
| `session_lock` / `session_unlock` | Windows Lock / Unlock | Level | WTS session |
| `session_logon` / `session_logoff` | Windows Logon / Logoff | Pulse | WTS session |
| `device_connect` / `device_disconnect` / `device_fail` | Device Connect / Disconnect / Failed | Pulse | USB/HID `WM_DEVICECHANGE` |
| `battery_low` / `battery_critical` | Low / Critical Battery Alarm | Level | battery % (<=10% / <=5%; clears on recover) |
| `display_off` / `display_on` | Display off / on | Level | console display power |
| `remote_connect` / `remote_disconnect` | Remote desktop | Level | WTS remote |
| `foreground_changed` | Foreground window changed | Pulse | WinEvent hook |
| `app_activated` / `app_deactivated` | OpenRGB foreground | Level | process focus |

**Not available (yet / ever without hooks):** classic MessageBeep sounds  -  Asterisk, Critical Stop, Exclamation, Question, Default Beep. Those are played by apps via `MessageBeep`/`PlaySound` with no system-wide listener API short of injecting into every process. Toast / Action Center notifications need WinRT `UserNotificationListener` + user consent (deferred).

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

UI: Object Creator -> Triggers. Prefer **Once** packs for edge events (`device_connect`, `foreground_changed`, logon, ...).

## Runtime spine

```text
Event catalog (Manual / Windows / Linux / macOS  -  later games + apps)
        -> triggers/triggers.json
Actions (.action.json)
        v TriggerRuntime + pack player (priority order)
LED frames over time -> OpenRGB + 3D viewport (layout-aware)
```

## Build order

1. Pack format + player (incl. ~60s + loop + priority). **done**
2. Effects library UI (create/preview/save with minimal tools). **done**
3. Bindings UI  -  Manual + Windows session/focus/display events. **done** (toast notifications later)
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

- `Effects3D/Actions/Action.*`  -  types, playback map, track color
- `Effects3D/Actions/ActionSerialize.cpp`  -  load/save JSON (`openrgb3d.action`, `.action.json`)
- `Effects3D/Actions/ActionLibrary.*`  -  scan `triggers/effects/` (no bundled seed content)
- `ui/ActionCatalog.h`  -  Action Editor Tools (`action-editor/toolbox/`)
- `ui/ActionsPanel.*`  -  Object Creator -> Actions
- `ui/TriggersPanel.*`  -  Object Creator -> Triggers
- `engines/triggers/*`  -  `triggers.json`, sources, TriggerRuntime
- `ui/ActionEditorDialog.*`  -  Action Editor
- `engines/movie-sync/*`  -  Movie Sync player + catalog
- `PluginSettingsPaths`  -  feature folders under plugin data root
