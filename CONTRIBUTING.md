# Contributing to OpenRGB 3D Spatial Plugin

Thanks for helping. You don't need to be an expert: a clear bug report or a single new look is a real contribution.

**Quick start**

- **Found a bug or want a feature?** Open an issue on [GitHub](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/issues/new/choose) and fill in the template.
- **Want to make a new look?** It's a content file, not C++. It goes in the [presets repo](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets). Start with the wiki page [Writing Effects](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/wiki/Writing-Effects).
- **Want to change the plugin code?** Fork on GitHub, branch from `main`, open a pull request. Read [Code rules](#code-rules) and run the [checklist](#before-you-open-a-pull-request) first.

## Where things live

| What | Where |
| ---- | ----- |
| Source of truth, PRs, CI, releases | [github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin) |
| GitLab mirror (read-only copy) | [gitlab.com/Wolfieeewolf/OpenRGB3DSpatialPlugin](https://gitlab.com/Wolfieeewolf/OpenRGB3DSpatialPlugin). Don't push work here; the mirror overwrites it. |
| Looks, layouts, Action tools, Movie Sync looks | [OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets). Never add stock content to the plugin tree. |
| User guide and developer reference (grid, coordinates, sliders, engines) | [Wiki](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/wiki) |
| Contracts reviewed with PRs | [`Documentation/`](Documentation/) (table below) |

| Doc | Read it before |
| --- | -------------- |
| [effects-engines.md](Documentation/effects-engines.md) | Adding or porting a look; touching an engine (Volume / Audio / Media / Shader Field / Patterns / Reactive / Ambilight / Game) |
| [PluginSpatialMeasurement.md](Documentation/PluginSpatialMeasurement.md) | Changing layout math, mm ↔ grid units, reference points, LED placement |
| [SpatialMeasurement.md](Documentation/SpatialMeasurement.md) | Changing the Minecraft / telemetry bridge (SHM, scale, mod mapping) |
| [effect-event-maker.md](Documentation/effect-event-maker.md) | Actions, Triggers, pack format, once / loop playback |
| [content-library.md](Documentation/content-library.md) | Adding a content folder or changing where files load from |
| [shader-conversion.md](Documentation/shader-conversion.md) | Porting a Shadertoy / strip / 3D shader |

## Issues

Plugin bugs, engines, UI, Screen Mirror and game telemetry go in **this repo**. Controller layouts, `.fs` / `.kernel` files, Action tools and timeline content go in the **[presets repo](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets/issues)**.

Pick the **Bug Report** or **Feature Request** template. Game-integration ideas use Feature Request.

| Helpful | Not helpful |
| ------- | ----------- |
| One topic per issue | A list of twenty games or devices in one ticket |
| Versions, GPU and steps to reproduce | "It doesn't work" with no details |
| What you expected vs what happened | Vague demands with no example |
| "Not sure yet" when you don't know the technical path | Pretending a data path exists when none is described |

**Game integrations** depend on how the plugin can learn the game's state (a mod, companion app, UDP, an API). Say what you looked into; it's fine not to know. Issues that skip the template may be closed so they can be resubmitted with the details. Maintainers label after triage (`bug`, `enhancement`, `game-integration`).

## Pull requests and AI

This plugin is built largely with AI coding tools, and that's welcome here. Crediting the AI in commits (for example a `Co-Authored-By:` line) is fine in this repo and the presets repo.

Whatever wrote it, **you own what you submit**: you've built it, run it in OpenRGB, and understand what it does.

## Code rules

### Scope

- Plugin rules cover plugin-owned code: `engines/`, `shared/`, `ui/`, the root `.pro` and plugin entry.
- Don't edit `OpenRGB/` (the upstream submodule) unless that's the point of the change.

### Engines, not one-off effects

The plugin is a **player**: library categories are engine names, and each engine plays the content files it finds. At Load (`OpenRGB3DSpatialPlugin.cpp`):

1. `RegisterFolderVolumeEffects()` scans `effects/volume|audio|media/**/*.fs` (**Volume** / **Audio** / **Media**).
2. `RegisterPlayerEngines()` (`shared/PlayerEngines.*`) adds **Reactive**, **Ambilight** (Screen Mirror), **Movie Sync** and one row per `effects/shader-field/*.fs`.
3. Minecraft layers (Health, Hunger, Air, Durability, Damage, Room Ambilight) use `REGISTER_EFFECT_3D` under **Game**.

A new look is a **file on an existing engine**. Don't add `REGISTER_EFFECT_3D` for anything except a new programmed Game pack.

### Effect contracts

Full detail lives in the wiki ([Coordinate Spaces](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/wiki/Coordinate-Spaces), [Effect Controls](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/wiki/Effect-Controls), [Engines](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/wiki/Engines)). The short version:

- **Anchor:** get it from `GetEffectOriginGrid(grid)`. Normalize with `SampleVolumeLocalUv01` (origin-local, honours rotation and per-axis scale), `SampleGpuVolumeOriginLocal01` (origin-local room axes) or `SampleGpuRoomVolume01` (room UV). Never `(x - grid.min_x) / span` by hand.
- **Units:** effects sample in grid units. For mm, use `grid.grid_scale_mm` or `GridUnitsToMM`, never a literal `10`. New distance logic goes through `GridSpaceUtils`.
- **Sliders:** Speed drives motion (`GetMotionHz`, `CalculateProgress`, `GetMotionSeconds`). Frequency drives hue cycling (`GetColorCycleHz`, `GetColorCycleSeconds`). **Size** is feature size, **Scale** is occupancy. Speed 0 freezes motion; Frequency 0 freezes hue.
- **`u_time`** is Speed-scaled seconds in every engine (Shadertoy `iTime`; real seconds at Speed 100).
- **Rendering entry point:** stack code calls `SpatialEffect3D::EvaluateColorGrid` (applies Sampling quantization), never `CalculateColorGrid` directly. Effects implement `CalculateColorGrid`. Texture / GIF effects reuse `MediaTextureEffectUtils.h`.

### GPU only, no CPU fallbacks

- The viewport, LED evaluation and the room-grid overlay run on **OpenGL 4.1 Core** on the shared `SpatialOffscreenGlPool`. Requires Qt 6.8+, built with the same Qt as the OpenRGB host (CI uses 6.8.3).
- Volume / Audio / Media and the Action field engines evaluate the real `volumeMain` at each LED (`evaluateAtPoints` / `BakeTrueGpuVolumeSamples`). Strip colormaps (`PrepareStripColormapAssist`) bake a 1D atlas from `effects/patterns/*.kernel`.
- **Every engine draws its own GPU overlay layer** (`TryFillGpuRoomGridOverlayLayer`) from the same data as its LEDs. Changing `GpuRoomGridOverlayLayer` needs a full rebuild; keep `abi_size` / `OverlayLayerAbiOk`.
- **Never add a CPU formula beside a GPU path** for the same look. If GL is unavailable, fail closed (dark), don't fake it.
- GLSL bodies are versionless Core GLSL with no reserved words (`layout`, `packed`, `flat`, `shared`, `half`, `fixed`, `filter`, ...). Embedded GLSL raw strings stay under 16 KB each (MSVC limit); split adjacent literals if needed.

### No legacy or backward-compatibility paths

Don't add migration shims, dual-format loaders, fuzzy matching or "try the old key" fallbacks. Remove them when you find one, and mention it in the PR so it isn't reintroduced.

| Area | Current contract | Don't keep |
| ---- | ---------------- | ---------- |
| OpenRGB profile payload | Plugin payload **version 1** with required `layout` and `effects` objects (`effects.playing` = resume after load). Never return an empty payload; keep layers whose effect is missing | Standalone layout / effect profiles, auto-load, quick-save, parallel session files |
| Layout data | `OpenRGB3DSpatialLayout` **version 7** only. Camera isn't written and is ignored if present | Defaults for missing required fields; layout version 6 |
| Custom controllers | `OpenRGB3DSpatialCustomController` **version 1** with `spacing_mm_x/y/z`, `column_widths_mm`, `row_heights_mm`, `layer_depths_mm`, `layer_names`, `leds_per_cluster` | Alternate key names; optional-array fallbacks; `Normalize*` / `TryRead*` import helpers |
| Effect settings | Each effect's `SaveSettings` / `LoadSettings`; embedded effects **version 9** only. Room output role is `Direct` (0) or `EmitterRelay` (1) | Other versions; obsolete role ints; renamed-key migration |
| Effect UI | `EffectUiRows` + shared `ui/forms/Effect*.ui` panels | Per-effect legacy `.ui` trees or duplicate control paths |
| Presets / scripts | Current schema only | One-off `convert_*.py` / generator scripts in the repo |
| Plugin settings | `LEDSpacing` X/Y/Z in host settings JSON | `legacy_grid_pitch_mm` and other retired keys |

### Style

We follow [OpenRGB's coding rules](OpenRGB/CONTRIBUTING.md), plus a few of our own (the rules on this page). Where they differ, ours win, and the existing style in the file you're editing comes first of all.

- 4 spaces; braces on their own lines; no space before the parenthesis (`if(x)`, not `if (x)`).
- `snake_case` variables, `CamelCase` types and functions.
- Log with `LogManager`. No `printf`, `std::cout` or `QDebug`.
- Avoid `auto` where the type isn't obvious; use indexed loops when index maths matters.
- SPDX licence identifier at the top of new or substantially rewritten files.
- No debug or dead code. Before keeping a non-virtual helper, grep for call sites.
- Keep it simple (DRY, KISS, YAGNI). Comments only for non-obvious *why*, not *what*.
- User-visible text is UTF-8; prefer `QStringLiteral` with `\uXXXX` escapes for special characters in source.

### RGBController safety

Follow [`OpenRGB/Documentation/RGBControllerAPI.md`](OpenRGB/Documentation/RGBControllerAPI.md):

- Guard pointers before use (`controller`, `transform`, `virtual_controller`, `zone_manager`).
- Check `idx < zones.size()` before `zones[idx]`, global LED index against `colors.size()` before writing, and mapping index against `led_positions.size()`.
- Prefer early `continue` for invalid mappings or stale data.

### UI

**Reuse shared UI before writing new controls.**

- `SpatialEffect3D::CreateCommonEffectControls` mounts the common sections for every effect, in this order: layer banner (Start / Stop) → **Motion and pattern** → **Effect-specific settings** (`SetupCustomUI`) → **Colors and patterns** (rainbow / stops, plus the strip colormap when `supports_strip_colormap`) → **Advanced** (Surfaces, Output shaping, Effect geometry, Height motion bands when `supports_height_bands`).
- Turn on shared sections with `EffectInfo3D` flags (`supports_strip_colormap`, `supports_height_bands`, `show_*_control`); don't rebuild them in `SetupCustomUI`.
- Build effect-specific rows with `ui/widgets/EffectUiRows.h` (`NewEffectPanel`, `AppendSliderRow`, `AppendComboRow`, ...). Give rows a stable `objectName` so `EffectUiSync` can restore them after a profile load.
- Numeric settings use `EffectSliderRow` / `EffectDoubleSliderRow` (spin over a thin slider). No bare `QSlider` + `QLabel` pairs.
- Every add / remove / edit action works three ways: mouse, keyboard and a menu. Right-click opens a menu; it never deletes instantly.
- If two effects need the same custom control, make a shared helper (like `engines/audio/AudioReactiveUi.h`) instead of copying it.
- **No custom theme:** no `setStyleSheet` on plugin widgets and no `QPalette` overrides on panels or group boxes, so the host theme applies. Exception: controller / custom-device cards may tint with the host's `QPalette::Highlight` (`PluginUiPaletteColor`).
- Keep signal connect / disconnect symmetrical for dynamic effect UI, keep list / combo / selection state in sync, and disable dependent controls when the selection is invalid.
- The main tab is `ui/forms/OpenRGB3DSpatialTab.ui`, implemented in `ui/shell/OpenRGB3DSpatialTab_*.cpp` by concern; reach widgets through `ui->…` and panel getters.

### Game integrations

- Telemetry (localhost UDP for pose / vitals / damage) is separate from the RGBController protocol; still validate input, log with `LogManager` and keep it thread-safe.
- Minecraft Room Ambilight reads a **sparse cubemap over SHM** from the Fabric mod in `integrations/minecraft/` (LED texels only). Treat the bridge as alpha: vanilla-shaped, modpacks largely untested.

## Before you open a pull request

- [ ] Builds, and you've run it in OpenRGB (note your OS and Qt version)
- [ ] Changes stay in plugin-owned paths (no casual `OpenRGB/` edits)
- [ ] New looks are content on an existing engine; no new `REGISTER_EFFECT_3D` except Game packs
- [ ] LEDs and the grid overlay show the same thing; no CPU fallback added
- [ ] No legacy / migration paths added; any you touched are removed
- [ ] `LogManager` for logging; SPDX header on new files; file style matched
- [ ] No `setStyleSheet` or theme palette overrides on structural UI
- [ ] Null pointers guarded; zone and LED indices checked before `colors[]` / mapping writes
- [ ] Docs updated if behaviour or wording changed (engine names match `Documentation/effects-engines.md`)

## Release smoke test

Before tagging a release, or after GL / FolderVolume / timeline changes:

1. **Presets static check** (no OpenRGB needed): in the presets checkout run `python scripts/smoke_presets.py`. Add `--compile-glsl` (needs `pip install moderngl` and OpenGL 4.1) to compile every shader.
2. **CI:** the GitHub Build workflow must be green on Windows, Linux and macOS.
3. **Manual pass with the grid overlay on:** one Volume look each of `finish: depth|hex|hsv|spiral|rgb`, one Audio look, Omni and Texture Projection, one Shader Field, one Reactive and one Screen Mirror layer, then a short timeline using Chase / Twinkle / Confetti / Blink / Alternating.
4. **Sync your data folder** from the presets repo so your AppData copy isn't older than `master`.

## OpenRGB reference docs

Upstream docs in the `OpenRGB/` submodule. The first two are binding for code style and anything touching LEDs, zones or colours; the rest matter when you document or debug hardware access.

| Doc | Use it for |
| --- | ---------- |
| `OpenRGB/CONTRIBUTING.md` | C++ / Qt style, logging, merge-request expectations |
| `OpenRGB/Documentation/RGBControllerAPI.md` | LEDs, zones, modes, `colors` layout, `UpdateLEDs` |
| `OpenRGB/Documentation/Compiling.md` | Build hints (Qt, qmake, deps) in user-facing docs |
| `OpenRGB/Documentation/OpenRGBSDK.md` | Network SDK packets and plugin IDs |
| `OpenRGB/Documentation/Common-Modes.md` | Firmware mode names, so UI wording doesn't clash |
| `OpenRGB/Documentation/UdevRules.md`, `USBAccess.md`, `SMBusAccess.md`, `KernelParameters.md` | Linux / USB / SMBus permission and detection help |

## Maintainers

**GitHub secret `GITLAB_MIRROR_TOKEN`:** a GitLab token with `read_repository` and `write_repository`, used by the **Mirror to GitLab** workflow. If the mirror job warns "HTTP Basic: Access denied", create a new token in GitLab (Preferences → Access Tokens), paste it into GitHub → Settings → Secrets and variables → Actions, and re-run the workflow.

**GitLab CI** is a backup. Build jobs stay manual unless the pipeline variable `RUN_CI=true` is set, or you start a pipeline from the web UI. Releases come from GitHub Actions on `v*` tags.

**Wiki:** edit the wiki checkout (`OpenRGB3DSpatialPlugin.wiki`) directly and push it.
