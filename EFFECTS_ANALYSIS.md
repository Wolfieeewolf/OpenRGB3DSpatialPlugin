# OpenRGB 3D Spatial Plugin – Effects System Analysis

This document summarizes the effects architecture, the three effect types (Spatial, Audio, Ambilight), the **standard settings contract**, and findings on consistency, missing sliders, and bugs.

---

## 1. Effects system overview

### Registration and loading

- **Macro**: `REGISTER_EFFECT_3D(ClassName)` in `EffectRegisterer3D.h` registers with `EffectListManager3D::get()->RegisterEffect(...)` at static init.
- **Registry**: `EffectListManager3D` (singleton) holds `effects` map and `effect_order`; provides `CreateEffect(class_name)`, `GetAllEffects()`, `GetCategorizedEffects(category)`.
- **Effect stack**: `effect_stack` is a vector of `EffectInstance3D`. Each instance has `effect_class_name`, `effect` (the running effect), `saved_settings` (JSON), zone, blend mode, enabled.

### Base class and interface

- **Base**: All effects inherit `SpatialEffect3D` (`SpatialEffect3D.h/cpp`), which is a `QWidget`.
- **Pure virtuals**: `GetEffectInfo()`, `SetupCustomUI(parent)`, `UpdateParams(SpatialEffectParams&)`, `CalculateColor(x,y,z,time)`.
- **Optional**: `CalculateColorGrid(..., grid)` (default uses origin, boundary, then `CalculateColor`).
- **Persistence**: `SaveSettings()` / `LoadSettings(json)`; base implements the standard set; effects call base then add their own keys.

### Effect stack UI and sync

- **Tab**: `OpenRGB3DSpatialTab_EffectStack.cpp` – list of layers, add/remove, blend mode. Selecting a layer calls `DisplayEffectInstanceDetails(instance)`.
- **Flow**: Creates or reuses `instance->effect`, builds UI with `CreateCommonEffectControls` + `SetupCustomUI`, then `LoadSettings(instance->saved_settings)`.
- **Sync**: When the effect UI emits `ParametersChanged`, the tab calls `SaveSettings()` on the UI effect, stores result in `instance->saved_settings`, calls `instance->effect->LoadSettings(updated)`, and saves the stack. So the **running** effect and saved state stay in sync as long as Save/Load are complete.

---

## 2. Three effect types

| Type       | Category   | How they differ |
|-----------|------------|------------------|
| **Spatial** | `"Spatial"` | Use common controls + custom UI; origin/scale/rotation; full standard params. |
| **Audio**  | `"Audio"`  | Same base; often hide speed/FPS; use audio input (e.g. `AudioInputManager`, band data). |
| **Ambilight** | `"Ambilight"` | Only **ScreenMirror**. No common controls; fully custom UI and Save/Load (no base contract). |

Structure is the same (same base and virtuals); only category, visibility flags, and UI/persistence differ.

---

## 3. Standard settings contract (canonical list)

Defined in **`SpatialEffect3D`**: constructor defaults and `SaveSettings()` / `LoadSettings()` in `SpatialEffect3D.cpp`.

All effects that use `CreateCommonEffectControls` and call base `SaveSettings()`/`LoadSettings()` get these. Visibility is controlled per effect via `EffectInfo3D` in `GetEffectInfo()`.

| Parameter | Type | Default | Range / values | JSON key |
|-----------|------|---------|----------------|----------|
| Speed | unsigned int | 1 | 0–200 | `"speed"` |
| Brightness | unsigned int | 100 | 1–100 | `"brightness"` |
| Frequency | unsigned int | 1 | 0–200 | `"frequency"` |
| Size | unsigned int | 100 | 0–200 | `"size"` |
| Scale | unsigned int | 200 | 0–300 (200 = 100% room) | `"scale_value"` |
| Scale inverted | bool | false | - | `"scale_inverted"` |
| **FPS** | **unsigned int** | **30** | **1–120** | **`"fps"`** |
| Rainbow mode | bool | false | - | `"rainbow_mode"` |
| Intensity | unsigned int | 100 | 0–200 | `"intensity"` |
| Sharpness | unsigned int | 100 | 0–200 | `"sharpness"` |
| Edge profile | int | 2 (Round) | 0–4 | `"edge_profile"` |
| Edge thickness | unsigned int | 50 | 0–100 | `"edge_thickness"` |
| Glow level | unsigned int | 15 | 0–100 | `"glow_level"` |
| Axis scale X/Y/Z | unsigned int | 100 | 1–400 (%) | `"axis_scale_x"` etc. |
| Rotation yaw/pitch/roll | float | 0 | 0–360 (°) | `"rotation_yaw"` etc. |
| Axis scale rotation | float | 0 | -180–180 (°) | `"axis_scale_rotation_*"` |
| Path axis | int | 1 | 0=X, 1=Y, 2=Z | `"path_axis"` |
| Plane | int | 1 | 0–2 | `"plane"` |
| Surface mask | int | SURF_ALL | bitmask | `"surface_mask"` |
| Offset X/Y/Z | int | 0 | -100–100 (%) | `"offset_x"` etc. |
| Reference mode | int | 0 | REF_MODE_* | `"reference_mode"` |
| Global/custom ref | float | 0 | - | `"global_ref_*"`, `"custom_ref_*"`, `"use_custom_ref"` |
| Colors | array | [{r,g,b},...] | - | `"colors"` |

**Visibility**: All standard controls are always shown for every effect that uses `CreateCommonEffectControls` (see `ApplyControlVisibility()` in base). Screen Mirror does not use common controls.

Edge profile/thickness/glow are in base state and Save/Load but **not** created in base UI; effects that need them add their own controls.

---

## 4. Spatial effects – list and settings summary

All under `Effects3D/`. Each uses the standard base (common controls + ApplyControlVisibility from GetEffectInfo) and adds custom UI in `SetupCustomUI` and custom keys in overridden Save/Load.

| Effect | Class | Custom params (Save/Load) | Visibility notes |
|--------|--------|---------------------------|------------------|
| Beam | Beam3D | mode, beam_thickness, beam_width, glow | path/plane, axis scale, rotation |
| Bouncing Ball | BouncingBall3D | ball_size, elasticity, ball_count | show_size_control=false |
| Breathing Sphere | BreathingSphere3D | breathing_mode, sphere_size | show_size_control=false |
| Color Wheel | ColorWheel3D | direction | show_plane_control |
| Crossing Beams | CrossingBeams3D | beam_thickness, glow | |
| DNA Helix | DNAHelix3D | helix_radius | |
| Explosion | Explosion3D | explosion_intensity, type, burst_count, loop, particle_amount | |
| Fireworks | Fireworks3D | particle_size, num_debris, firework_type, num_simultaneous, gravity_strength, decay_speed | |
| Lightning | Lightning3D | strike_rate, branches | |
| Matrix | Matrix3D | density, trail, char_*, head_brightness | |
| Moving Panes | MovingPanes3D | num_divisions | path_axis |
| Plasma | Plasma3D | pattern_type | |
| Pulse Ring | PulseRing3D | ring_style, ring_thickness, hole_size, pulse_*, direction_deg | |
| Rotating Beam | RotatingBeam3D | beam_width, glow | show_plane_control |
| Sky Lightning | SkyLightning3D | flash_rate, flash_duration | show_scale=false, show_size=false |
| Spiral | Spiral3D | num_arms, pattern_type, gap_size | |
| Spin | Spin3D | num_arms | |
| Starfield | Starfield3D | mode, star_size, num_stars, drift_amount, twinkle_speed | |
| Sunrise | Sunrise3D | time_mode, color_preset, day_length_minutes, weather_* | |
| Surface Ambient | SurfaceAmbient3D | style, height_pct, thickness | |
| Tornado | Tornado3D | core_radius, tornado_height | |
| Traveling Light | TravelingLight3D | mode (Comet, Chase, Marquee, ZigZag, KITT), tail_size, beam_width | path_axis. Replaces former Comet, Visor (KITT), ZigZag effects. |
| Wave | Wave3D | shape_type, edge_shape, wave_thickness | |
| Wave Surface | WaveSurface3D | wave_style, surface_thickness, wave_* | |
| Wipe | Wipe3D | wipe_thickness, edge_shape | |
| Wireframe Cube | WireframeCube3D | thickness, line_brightness | |

All spatial effects (except ScreenMirror) use `CreateCommonEffectControls` and base Save/Load, so they all share the same **data** contract; only **visibility** of standard sliders differs via `GetEffectInfo()`.

---

## 5. Gaps, inconsistencies, and bugs

### Fixed in this pass

- **FPS not saved**: Base `SaveSettings()` did not include `effect_fps`, and `LoadSettings()` did not restore it. **Fix**: Added `"fps"` to `SaveSettings()` and loading of `effect_fps` in `LoadSettings()` in `SpatialEffect3D.cpp`. FPS is now persisted and restored with the rest of the standard set.

### Known gaps / design choices

| Issue | Where | Detail |
|-------|--------|--------|
| **ScreenMirror** | Ambilight | Does not call `SpatialEffect3D::SaveSettings()` / `LoadSettings()`. All standard params (speed, brightness, scale, colors, etc.) are custom or unused. Intentional for this effect. |
| *(Removed)* Legacy path/plane keys | Base LoadSettings | Legacy keys (`comet_axis`, `sweep_axis`, etc.) were removed; only `path_axis` and `plane` are used. |
| **Edge controls** | Base UI | Edge profile/thickness/glow are in base state and Save/Load but are **not** created in `CreateCommonEffectControls`. Effects that need edge add their own. So “standard” edge is in the data contract only. |

### Custom slider sync when loading (inconsistency)

- **Problem**: When the stack loads a layer, it calls `ui_effect->LoadSettings(settings)`. Base class updates **its** sliders (speed, brightness, etc.) from the loaded values. Many effects only set their **member** variables in overridden `LoadSettings()` and do **not** update their custom UI widgets (e.g. beam_width, glow), because those widgets are created in `SetupCustomUI` and often not stored as member pointers.
- **Result**: After loading a profile, the **effect state** (and the running effect) is correct, but the **custom sliders** in the panel can still show the default values until the user moves them.
- **Correct pattern**: Store custom slider/widget pointers as members and in `LoadSettings()` set their values (and labels) from the loaded state, like **BouncingBall3D** does (`size_slider`, `elasticity_slider`, `count_slider` + `setValue` in `LoadSettings`).
- **Effects that already sync custom sliders in LoadSettings**: BouncingBall3D (and any others that keep slider pointers and update them in LoadSettings).
- **Effects that only set members** (custom sliders may show wrong value after load): e.g. Beam3D, RotatingBeam3D, Starfield3D, TravelingLight3D, and others that create sliders as locals in `SetupCustomUI` and do not update them in `LoadSettings`. Recommended follow-up: for each such effect, add member pointers for custom controls and set them in `LoadSettings()`.

### Minor / optional

- **Starfield3D**: LoadSettings clamps `num_stars` to 40–200; UI slider range is 40–120. Saved value can be up to 200 but slider max is 120. Consider aligning range or documenting.
- **FPS tooltip**: Base FPS slider tooltip says “1–60”; range is 1–120. Consider updating tooltip.
- **BouncingBall3D**: Uses custom “Ball Size” (`ball_size`, 10–150) and hides standard “Size” (`show_size_control=false`). Two different semantics; no JSON key conflict.

### Connection and flow

- **ParametersChanged → Save → Load on instance**: When the effect UI emits `ParametersChanged`, the tab saves from the UI effect and loads into `instance->effect`. As long as Save/Load are complete and symmetric, the running effect and saved state stay in sync.
- **DisplayEffectInstanceDetails**: Uses a **new** `CreateEffect()` widget for the panel (`ui_effect`); the running instance uses `instance->effect`. Both receive the same `LoadSettings(settings)`. So the panel and runner are in sync after load; the only gap is the custom-slider **display** when those sliders are not updated in LoadSettings.

---

## 6. Recommendations

1. **FPS**: Done – FPS is now part of the standard persisted set.
2. **Custom sliders**: For each spatial (and audio) effect that has custom UI, consider storing slider/widget pointers and updating them in `LoadSettings()` so the panel matches the loaded state.
3. **ScreenMirror**: No change needed if full custom schema is intended; document that it does not use the base settings contract.
4. **Standard visibility**: All spatial effects already use the same base and the same visibility model (`info_version = 2` + flags). No effect is missing the “standard set” in data; only visibility and custom params differ.
5. **Tooltip**: Update base FPS slider tooltip to “1–120” if the UI range is 1–120.

---

## 7. Quick reference – standard visibility by effect type

- **Speed**: Shown for most spatial; often hidden for audio (e.g. AudioPulse, FreqFill, DiscoFlash).
- **Brightness**: Shown for all that use common controls.
- **Frequency**: Shown only where it makes sense (e.g. DNA Helix, Plasma, Tornado, Wave, Explosion, Spiral, Surface Ambient).
- **Size**: Hidden where effect uses its own size (e.g. Bouncing Ball, Comet, Breathing Sphere, Sky Lightning).
- **Scale**: Hidden for Sky Lightning (and ScreenMirror); shown for others that use common controls.
- **FPS**: Shown for most spatial; often hidden for audio.
- **Path/plane/position offset/surfaces**: Set per effect via `show_path_axis_control`, `show_plane_control`, `show_position_offset_control`, `show_surface_control`.

All spatial effects set `info_version = 2`, so visibility is consistently driven by these flags.
