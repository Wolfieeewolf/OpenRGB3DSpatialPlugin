# Contributing to OpenRGB 3D Spatial Plugin

## Repository and workflow

This project follows the same model as [OpenRGB](https://gitlab.com/CalcProgrammer1/OpenRGB) and the [Effects plugin](https://gitlab.com/OpenRGBDevelopers/OpenRGBEffectsPlugin):

| Role | Location |
| ---- | -------- |
| **Source of truth** | [gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin](https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin) |
| **CI / releases / artifacts** | GitLab CI (`.gitlab-ci.yml`) |
| **GitHub mirror** | [github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin) — updated automatically when `GITHUB_MIRROR_TOKEN` is set in GitLab CI variables |

**Fork on GitLab**, branch from `main`, open a **merge request** on GitLab. Do not expect code review on GitHub pull requests (the mirror posts a redirect comment).

### GitLab CI variables (maintainers)

| Variable | Purpose |
| -------- | ------- |
| `GITHUB_MIRROR_TOKEN` | GitHub PAT with `contents: write` — pushes `main` and tags to the GitHub mirror |

Until the project namespace is finalized on `OpenRGBDevelopers`, the maintainer fork `wolfieeewolf1/OpenRGB3DSpatialPlugin` is also treated as upstream for automatic CI (see `.gitlab-ci.yml` rules).

## Issues and merge requests

We use **GitLab issue templates** and **merge request templates** under `.gitlab/`.

### Which repository?

| Topic | Open an issue in |
| ----- | ---------------- |
| Plugin bugs, effects, UI, screen mirror, game telemetry | **[OpenRGB3DSpatialPlugin on GitLab](https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin/-/issues)** |
| Controller layout JSON files | **[OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets/issues)** — format: [docs/controller-preset-format.md](docs/controller-preset-format.md) |

### Templates on GitLab (plugin)

| Template | Use for |
| -------- | ------- |
| Bug Report | Crashes, wrong behavior, regressions |
| Feature Request | New capabilities and UX improvements |

Game-integration requests can use Feature Request with a clear game/telemetry section, or a dedicated issue description on GitLab.

**Blank issues** may be disabled. Pick a template so triage has version info and reproduction steps.

Direct link: [new issue](https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin/-/issues/new)

### What makes a useful issue (common sense)

You do **not** need to be a developer. We do need **enough detail to act**, not a vent with no context.

| Do | Avoid |
|----|--------|
| **One topic per issue** (one bug, one feature, one game) | Lists of many games or devices in a single ticket |
| **Versions and steps** (plugin, OpenRGB, OS) for bugs | "It doesn't work" / "WTF" with no reproduction |
| **Plain language** about what you want | Vague demands with no example behavior |
| **"Not sure yet"** when you do not know the technical path | Pretending there is a data path when there is none described |
| **[Pull request](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/compare)** if you already have a fix or mod/bridge | Expecting others to guess your setup |

**Game integration** almost always depends on **how the plugin can learn game state** (mod, companion app, UDP, API, existing tool). Minecraft (Java mods), some **Unity** titles, and community tools are common examples; **Unreal** and others are often harder—say what you looked into. It is fine not to know; it is not fine to ask for twenty games with no lighting goals and no research.

Issues that ignore the form or read like low-effort rants may be **closed** so someone can resubmit with the fields filled in. That is not personal—it keeps triage fair for everyone.

After triage, apply labels such as `bug`, `enhancement`, or `game-integration` (create them under **Issues → Labels**). Issue forms intentionally omit auto-labels so templates stay visible even before labels exist.

## OpenRGB reference documentation

When changing plugin behavior—especially anything that touches devices, colors, zones, builds, or user-facing setup—stay consistent with upstream OpenRGB docs in the `OpenRGB/` submodule:

| Document | Use it for |
| -------- | ---------- |
| `OpenRGB/CONTRIBUTING.md` | Merge-request style, C++/Qt style guidelines, logging (`LogManager` not `QDebug`/`printf`), general contribution expectations. |
| `OpenRGB/Documentation/RGBControllerAPI.md` | LEDs, zones, modes, `colors` layout, `SetCustomMode` / `UpdateLEDs` expectations—any path that reads or writes controller state. |
| `OpenRGB/Documentation/OpenRGBSDK.md` | Network SDK packet layout and plugin-related IDs—only if you change or document SDK-facing behavior. |
| `OpenRGB/Documentation/Common-Modes.md` | Naming and meaning of **firmware** modes on devices; plugin spatial effects are software-driven but avoid confusing users with conflicting terminology where it overlaps UI copy. |
| `OpenRGB/Documentation/Compiling.md` | How OpenRGB itself is built; align user-facing build hints (Qt, qmake, deps) with this where relevant. |
| `OpenRGB/Documentation/UdevRules.md` | Linux permissions for USB/HID access—reference when documenting Linux install or troubleshooting detection. |
| `OpenRGB/Documentation/USBAccess.md` | USB setup and permission notes—user docs or issues involving USB devices. |
| `OpenRGB/Documentation/SMBusAccess.md` | SMBus/I2C setup (Windows/Linux/macOS)—user docs for RAM/motherboard RGB access issues. |
| `OpenRGB/Documentation/KernelParameters.md` | Kernel cmdline (e.g. ACPI/SMBus conflicts)—user docs, not plugin C++ unless you add explicit guidance text. |

**Minimum bar for this repo:** treat `OpenRGB/CONTRIBUTING.md` and `OpenRGB/Documentation/RGBControllerAPI.md` as binding for code style and anything that touches `RGBController` / LED indices / zones. The others apply when you document behavior, debug hardware access, or work near those subsystems.

## Scope

- Plugin rules apply to plugin-owned code in this repository (e.g. `Effects3D/`, `ui/`, `Game/`, root `.pro`).
- Do not modify files under `OpenRGB/` (upstream subtree) unless explicitly requested.

## Code quality

- No debug code or dead code.
- Before keeping a **non-virtual** public or private helper, grep the repo for call sites; unused batch paths and similar leftovers should be removed.

### No legacy or backward-compatibility paths

**Do not add** migration shims, dual-format loaders, fuzzy preset matching, or “try the old key if the new one is missing” logic in plugin-owned code.

**Remove** when you find it (grep for `legacy`, `migration`, `backward`, `compat`, `grid_pitch`, `old_` key fallbacks, and optional JSON branches that only exist for pre-v1 files):

| Area | Current contract | Do not keep |
|------|------------------|-------------|
| Layout profiles | `OpenRGB3DSpatialLayout` **version 6** — all sections and nested fields required | Defaults for missing `led_spacing_mm`, `granularity`, camera, or grid keys |
| Custom controllers | `OpenRGB3DSpatialCustomController` **version 1** — `spacing_mm_x/y/z` required | Alternate spacing key names; `Normalize*` / `TryRead*` import helpers |
| Effect settings | Keys defined by each effect’s `SaveSettings` / `LoadSettings` | Renamed-key migration inside `LoadSettings` unless the user explicitly asks for a one-release bridge |
| Effect UI | `EffectUiRows` + shared `ui/forms/Effect*.ui` panels | Per-effect legacy-only `.ui` trees or duplicate control paths “for old layouts” |
| Presets / scripts | Export/import uses current schema only | One-off `convert_*.py` migration tools in the repo |
| Plugin settings | `LEDSpacing` X/Y/Z in host settings JSON | `legacy_grid_pitch_mm` or other retired keys |

**Exception — viewport only:** the Qt 5.15 **legacy OpenGL room** (`paintGlLegacyScene`, `ViewportLegacyGL`, GPU label fallbacks) stays until OpenRGB ships Qt 6 as the default host. That is rendering infrastructure, not data-format backward compatibility. See [docs/VIEWPORT.md](docs/VIEWPORT.md) and [docs/VIEWPORT_QT515.md](docs/VIEWPORT_QT515.md).

When you delete legacy paths, note them in [docs/CODEBASE_AUDIT.md](docs/CODEBASE_AUDIT.md) (dead code removed table) so the next pass does not reintroduce them.
- Keep code simple: DRY, KISS, YAGNI, single-responsibility functions.
- Comments should be minimal and explain non-obvious intent only.

## Style

- Follow **existing style in the file first**, then `OpenRGB/CONTRIBUTING.md` where it does not conflict.
- 4 spaces, braces on their own lines, `snake_case` variables, `CamelCase` types/functions.
- Avoid `printf`, `std::cout`, and `QDebug`; use `LogManager`.
- Prefer explicit types when they improve readability; avoid unnecessary `auto`.
- Prefer indexed loops where index math matters; range loops are fine for simple read-only traversal.
- New/changed files: SPDX license identifier in the header; keep headers minimal unless the file warrants more.

## RGBController safety rules

Aligned with `OpenRGB/Documentation/RGBControllerAPI.md` and safe patterns for mapped devices:

- Always guard pointers before dereference (`controller`, `transform`, `virtual_controller`, `zone_manager`).
- Before `zones[idx]`, validate `idx < zones.size()`.
- Before color writes, validate global LED index against `colors.size()`.
- For mapped LEDs, validate mapping index against `led_positions.size()` before use.
- Prefer early-continue guards for invalid mappings or stale data.

## Game / Minecraft and similar effects

- Telemetry (e.g. UDP on localhost) is separate from the RGBController wire protocol; still apply logging, validation, and thread-safety patterns from the rest of the plugin.
- Effect UI that targets the stack (effect library, global settings, add-to-stack flow) must keep **list/combo/signal** state consistent—see **UI** below.

## UI

### Main spatial tab (Designer)

The **3D Spatial** plugin tab shell is built from `ui/forms/OpenRGB3DSpatialTab.ui` plus embedded panel widgets. Widgets are accessed via `ui->…` and panel getters (`OpenRGB3DSpatialTab::zonesList()`, etc.)—not duplicated pointer members on `OpenRGB3DSpatialTab`. Tab implementation is split under `ui/OpenRGB3DSpatialTab_{Setup,Layout,Scene,Effects,Settings,Audio}.cpp`; shared controller cards in `ui/ControllerCards.cpp`.

### Reuse shared UI (all effects)

Default rule for every effect: **reuse existing panels and helpers before writing new controls.** Do not copy-paste slider rows, color pickers, or section layouts into individual `.cpp` files when a shared widget or helper already covers that purpose.

**Use first (stack / library):**

- `SpatialEffect3D::CreateCommonEffectControls` and the `ui/widgets/*` panels (Surfaces, Motion, Output, Geometry, Colors, `StratumBandPanel`, strip colormap). Layout for Surfaces / Motion / Output / Geometry / Colors / layer banner lives in **`ui/forms/Effect*.ui`** (Qt Designer); C++ subclasses load with `setupUi` and expose the same widget pointers as before. Thin hosts (`EffectControlsRoot`, `EffectCustomHost`, tab effect-controls shell) are built in C++ with `QVBoxLayout` only.
- `EffectInfo3D` flags (`supports_height_bands`, `supports_strip_colormap`, `show_*_control`) so common sections appear instead of custom duplicates.
- `AddWidgetToParent` and the standard mount flow (`MountSettingsUi` / `createEffectSettingsUi`).

**Add new UI only when** the control is truly effect-specific or no shared helper exists yet—and if two or more effects need the same custom control, add or extend a shared helper (like `Effects3D/AudioReactiveUi.h`) instead of duplicating code.

Effect-specific settings: build with **`ui/widgets/EffectUiRows.h`** (`NewEffectPanel`, `AppendSliderRow`, `AppendComboRow`, …) and stable `objectName`s for `EffectUiSync`. Media layers use **`MediaTextureAmbienceBlock`** (its own small `.ui`). There are no per-effect `*EffectSettings.ui` files anymore — do not add a parallel “legacy” UI path ([EFFECTS.md](docs/EFFECTS.md#legacy-ui-and-settings--remove-do-not-preserve)). **Minecraft** game layers use dedicated settings under `Effects3D/Games/Minecraft/` (room VR tint samples the GPU panorama cubemap or optional CPU room raycasts). Texture / omni media layers use **height motion bands** (`StratumBandPanel`) instead of game telemetry.

### Effect settings tab order (stack / library)

`SpatialEffect3D::CreateCommonEffectControls` defines the **fixed section order** for every effect that uses the common mount (not Screen Mirror `CustomOnly` or Minecraft chrome overrides):

1. **Surfaces** → 2. **Motion and pattern** → 3. **Output shaping** → 4. **Effect geometry** → 5. **Colors and patterns** (rainbow/stops; strip colormap when `supports_strip_colormap`) → 6. **Height motion bands** (`StratumBandPanel` when `supports_height_bands`) → 7. **Effect-specific settings** (`SetupCustomUI` only).

Do **not** duplicate strip colormap or floor/mid/ceiling band controls in `SetupCustomUI`—set `supports_strip_colormap` / `supports_height_bands` on `EffectInfo3D` and use `GetStratumTuning()` / `GetStratumLayoutMode()` in render.

Within **Effect-specific settings**, stack rows in a **`QVBoxLayout`** via **`EffectUiRows`** (`AppendComboRow`, `AppendSliderRow`, …). Give each row a stable `objectName` when `LoadSettings` must resync after profile load (`EffectUiSync`). Prefer caching widget pointers (sliders/combos) when the effect already holds them; use `EffectUiSync` on the nested `NewEffectPanel` when controls live only inside that panel.

**Audio effects:** same reuse rule as above. Each algorithm is its own stack layer. Prefer **strip-first** effects (e.g. **Audio Strip Visualizer**) with **zone / single-controller** targeting for floor, wall, and ceiling strips; use full-room 3D audio effects when one layer should cover everything. Shared audio helpers live in `Effects3D/AudioReactiveUi.h` (`AppendStandardFrequencyBandSection`, `AppendStandardDriveSection`, `AppendStandardResponseSection`, `AppendStandardBeatWaveSection`, `AppendStandardSpectrumAnalyzerSections`). Global **16-band EQ** is on the **Audio Input** panel. Show that panel when any audio layer is on the stack.

- **Spectrum bin effects** (Spectrum Bars, Audio Strip Visualizer) use frequency band + response only—no drive-mode row; they sample raw FFT bins in the Hz range.
- **Level-driven effects** (Audio Level) use frequency band + drive + response.
- **Audio Pulse** uses the full beat-wave section for beat-triggered shockwaves from the origin.

Optional local notes may live in a gitignored **`docs/`** folder on your machine; they are not part of the published repo.

- Keep signal wiring/disconnect paths symmetrical for dynamically created effect UI widgets.
- Keep list, combo, and selection state synchronized.
- Clear/disable dependent controls when selection is invalid.
- **No custom UI chrome styling:** do not use `setStyleSheet` on plugin widgets, and do not override `QPalette` on panels, frames, `QGroupBox`, or other structural chrome—widgets must inherit the host OpenRGB / system theme. Layout, `QFrame` shape/shadow, `autoFillBackground`, and standard Qt widgets only. (Helpers like dimmed helper labels or RGB swatch icons are content, not theme overrides for containers.)
- **Exception — list/card selection:** controller and custom-device cards may tint `cardFrame` using `PluginUiPaletteColor` / `QPalette::Highlight` from the host theme only (see `SpatialControllerCardWidget`, `CustomControllerDeviceWidget`). Do not introduce new `setStyleSheet` or fixed RGB chrome colours.

## Pre-submit checklist (plugin-owned code)

Aligned with **this file**, **`OpenRGB/CONTRIBUTING.md`**, and the OpenRGB docs in **OpenRGB reference documentation** above (minimum: **`RGBControllerAPI.md`** when touching LEDs/zones/colors).

- [ ] **Scope:** changes only under plugin-owned paths; no edits in `OpenRGB/` unless intentional.
- [ ] **No legacy paths:** no new migration or old-format fallbacks; remove any you touch unless viewport Qt 5.15 GL (see **No legacy or backward-compatibility paths**).
- [ ] **Logging:** no `QDebug`, `printf`, or `std::cout`; use `LogManager`.
- [ ] **UI chrome:** no `setStyleSheet` on structural chrome; no custom theme `QPalette` on panels/`QGroupBox` (card selection exception in **UI** only).
- [ ] **Headers:** SPDX on new or substantially new `.cpp`/`.h`; match brace/naming style in the file.
- [ ] **Effects:** stack/render paths call `SpatialEffect3D::EvaluateColorGrid`, not `CalculateColorGrid`, except inside effect implementations.
- [ ] **RGB safety:** guard null pointers; validate zone and LED indices before `colors[]` / mapping writes (see **RGBController safety rules**).
- [ ] **Docs:** user-facing build/troubleshooting text matches `OpenRGB/Documentation/Compiling.md` and the relevant USB/SMBus/Udev/Kernel docs when applicable.
- [ ] **Strings:** user-visible text uses correct UTF-8 (prefer `QStringLiteral` with `\\uXXXX` escapes for special characters in source files).

## Effect rendering entry points

- Effect stack code should call **`SpatialEffect3D::EvaluateColorGrid`** (applies global **Sampling** / spatial quantization where enabled), not `CalculateColorGrid` directly.
- Implement per-effect color in **`CalculateColorGrid`**; override **`UsesSpatialSamplingQuantization()`** only when the effect already handles resolution in UV space (e.g. texture projection, screen mirror).
- **Texture / GIF effects:** reuse **`MediaTextureEffectUtils.h`** (`MediaTextureEffect` namespace) for bilinear sampling, ambience gain, and RGB lerp—avoid duplicating those helpers in individual `.cpp` files.
