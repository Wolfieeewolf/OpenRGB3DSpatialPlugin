# Effects

## Stack model

- Register with **`REGISTER_EFFECT_3D`** (`EffectRegisterer3D.h`) → `EffectListManager3D` at static init.
- Each layer: **`EffectInstance3D`** (`effect_class_name`, running `effect`, `saved_settings` JSON, zone, blend, enabled).
- UI: `OpenRGB3DSpatialTab_Effects.cpp` — library, add/remove, blend row, `DisplayEffectInstanceDetails`.
- Flow: `CreateCommonEffectControls` → effect `SetupCustomUI` → `LoadSettings(saved_settings)`.
- **`ParametersChanged`**: save panel effect → `LoadSettings` on `instance->effect` → persist stack (`effect_stack.json` + named presets).
- Unknown / removed class names on load: layer skipped with warning (`IsEffectRegistered`).

## Categories (library filters)

| Category | Count | Role |
|----------|------:|------|
| **Spatial** | 27 | Room-grid effects; common motion/geometry/color panels |
| **Audio** | 9 | Read `AudioInputManager`; shared `AudioReactiveUi` sections |
| **Media** | 2 | Textures/GIF/video — `TextureProjection`, `OmniShapeTexture`; height bands, not room sampler |
| **Ambilight** | 1 | **Screen Mirror** only — custom Save/Load, no base contract |
| **Game** | 8 | Minecraft hub + sub-layers (see [GAME.md](GAME.md)) |

### Spatial (registered)

Bouncer, Bouncing Ball, Breathing Sphere, Bubbles, Color Wheel, Depth Tone, DNA Helix, Explosion, Fireworks, Harmonic Pulse, Hex Lattice, Lightning, Matrix, Plasma, Pulse Ring, Rotating Cone Spotlights, Sharp Pulse, Shell Pattern, Spiral, Starfield, Sunrise, Surface Ambient, Tornado, Traveling Light, Wave, Wireframe Cube, Xor Field.

### Audio

Audio Pulse, Audio Level, Frequency Fill, Cube Layer, Band Scan, Spectrum Bars, Audio Strip Visualizer, Disco Flash, Shader Field (optional audio uniforms; category Audio).

### Media

Texture Projection, Omni Shape Texture — override **`UsesSpatialSamplingQuantization()` → false**; use **`CombineMediaSampling()`** with global Output → **Sampling**.

### Game

`MinecraftGameEffect3D` (hub) + sub-effects: Health, Hunger, Air, Durability, Damage, World Tint, Lightning.

## Common controls (section order)

For effects using the standard mount (`SpatialEffect3D::CreateCommonEffectControls`):

1. Surfaces → 2. Motion and pattern → 3. Output shaping (**Sampling** lives here) → 4. Effect geometry → 5. Colors (+ strip colormap when `supports_strip_colormap`) → 6. Height motion bands (`StratumBandPanel` when `supports_height_bands`) → 7. Effect-specific (`SetupCustomUI`).

Use **`EffectInfo3D`** flags — do not duplicate strip colormap or stratum controls in `SetupCustomUI`.

**`EffectSamplerPanel`** (compass / voxel mapping): only via **`AttachRoomMappingPanel`** — Minecraft game layers today, not media/texture effects.

## Rendering rules

- Stack and zone render paths call **`EvaluateColorGrid`** (applies spatial sampling quantization when enabled), not raw **`CalculateColorGrid`**, except inside the effect implementation.
- Default **`UsesSpatialSamplingQuantization() == true`**. Media texture effects return **false** and quantize in UV space themselves.
- Global **Sampling** (0–100) in Output shaping; media layers can add local detail; combined via **`CombineMediaSampling()`**.
- GIF/video smoothing: Output shaping **Smoothing**, not a separate media-only slider.
- Textures: **`MediaTextureEffectUtils.h`** for sample/lerp/ambience.

## Standard persisted settings

Base keys in `SpatialEffect3D` Save/Load include `speed`, `brightness`, `frequency`, `size`, `scale_value`, `fps` (1–120), colors, surfaces, path/plane, offsets, rotations. Visibility from **`GetEffectInfo()`** (`info_version = 2`).

**Semantics:** **Speed** = `time * GetScaledSpeed()`; **Size** = spatial scale; **Frequency** = pattern density / color cycle — avoid duplicate custom sliders for the same job (Traveling Light uses standard Size + Frequency).

## Adding an effect

1. `Effects3D/<Name>/` — `GetEffectInfo`, `SetupCustomUI`, `CalculateColorGrid` (or override quantization flags for media).
2. `SaveSettings` / `LoadSettings`: call base; store custom JSON; **sync custom widgets in `LoadSettings`** via `CustomSettingsPanelWidget()` + `EffectUiSync` (stable row `objectName`s) and/or `AudioReactiveUi::SyncSettingsToHost` for audio sections — no retired-key migration.
3. `OpenRGB3DSpatialPlugin.pro` + `REGISTER_EFFECT_3D`.
4. UI: **`EffectUiRows.h`** for custom rows; media effects add **`MediaTextureAmbienceBlock`** in code (no `<Name>EffectSettings.ui`).

### Legacy UI and settings — remove, do not preserve

Plugin policy ([CONTRIBUTING.md](../CONTRIBUTING.md#no-legacy-or-backward-compatibility-paths)): **no legacy effect UI** and **no backward-compatible JSON loaders** in effect code.

| Remove when found | Use instead |
|-------------------|-------------|
| Per-effect `*EffectSettings.ui` only used for old layouts | `EffectUiRows` + `EffectUiSync` on `CustomSettingsPanelWidget()` |
| `LoadSettings` branches that read retired JSON keys | Current keys only; skip layer with `LogManager` warning if class unknown |
| Duplicate sliders that mirror common Speed / Size / Frequency | `EffectInfo3D` visibility flags + common panels |
| Docs or comments treating “legacy `.ui`” as a supported second path | `EffectUiRows` + shared `ui/forms/Effect*.ui` |

**Viewport exception:** Qt 5.15 legacy OpenGL room drawing is not effect UI — see [VIEWPORT.md](VIEWPORT.md).

Log removals in [CODEBASE_AUDIT.md](CODEBASE_AUDIT.md) when you delete legacy paths.

## LoadSettings UI sync

After profile or stack reload, effect-specific sliders/combos should match saved JSON. Use:

- **`CustomSettingsPanelWidget()`** — root widget added in `SetupCustomUI`.
- **`EffectUiSync.h`** — `setSliderValue` / `setComboIndex` / `setCheckBox` by row `objectName` on the custom-settings host (rows from `EffectUiRows` or shared `Effect*.ui` sections — not retired per-effect settings `.ui` files).
- **`AudioReactiveUi::SyncSettingsToHost`** — frequency/drive/response rows built by `AudioReactiveUi` (caption-matched).
- **Member widget pointers** still fine for one-off controls (e.g. `Plasma::pattern_combo`, `CubeLayer::layer_edge_combo`).
