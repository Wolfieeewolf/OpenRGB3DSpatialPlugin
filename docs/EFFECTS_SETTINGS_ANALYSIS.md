# Standard Settings Deep Dive: How Each Effect Uses Speed, Size, and Frequency

This document describes how the **standard** sliders (Speed, Brightness, Size, Frequency, Scale, FPS) are used across spatial effects, so we can keep behavior consistent and avoid duplicating them with effect-specific controls.

---

## Standard parameters (base `SpatialEffect3D`)

| Parameter   | Storage        | Normalized / scaled helpers              | Typical meaning                          |
|------------|-----------------|------------------------------------------|------------------------------------------|
| **Speed**  | `effect_speed`  | `GetNormalizedSpeed()`, `GetScaledSpeed()` | Animation rate; drives `CalculateProgress(time)` |
| **Brightness** | `effect_brightness` | Applied in `PostProcessColorGrid()`   | Final output multiplier                  |
| **Size**   | `effect_size`   | `GetNormalizedSize()` = (size/200)*3      | Spatial scale of the effect (tails, beams, radius, pattern scale) |
| **Frequency** | `effect_frequency` | `GetNormalizedFrequency()`, `GetScaledFrequency()` | Pattern/color rate (waves, pulse, color cycle) |
| **Scale**  | `effect_scale`  | `GetNormalizedScale()`                   | Extra spatial scale / zoom              |
| **FPS**    | `effect_fps`    | `GetTargetFPS()`                         | Update rate                              |

- **Speed** is used the same everywhere: `progress = time * GetScaledSpeed()`. No effect-specific speed sliders.
- **Brightness** is applied in the base class; no per-effect brightness sliders.

---

## How each effect uses **Size** (`GetNormalizedSize()`)

| Effect           | Use of Size |
|------------------|-------------|
| **BreathingSphere** | Sphere radius scale (with custom `sphere_size`). `sphere_radius = half_diag * base_scale * size_multiplier * (1 + 0.25*sin(...))`. |
| **Plasma**       | Pattern scale divisor. `scale = actual_frequency * 0.004f / size_multiplier` → larger size = larger blobs. |
| **Wave**         | Wave wavelength. `freq_scale = actual_frequency * 0.1f / size_multiplier`. |
| **Spiral**       | Spiral tightness. `freq_scale = actual_frequency * 0.003f / size_multiplier`. |
| **Explosion**    | Size multiplier for blast radius / pattern. |
| **DNAHelix**     | Size multiplier for helix scale. |
| **Tornado**      | `size_m` for funnel/pattern scale. |
| **BouncingBall** | Ball radius / trail size. |
| **Matrix**       | Size multiplier for rain/columns. |
| **Lightning**    | Bolt thickness / spread. |

**Unified meaning:** Size controls “how big” the effect is in space (tail length, beam width, radius, pattern scale). Effects should use the **standard Size** slider for this and avoid a separate “tail size” or “beam size” slider that does the same thing.

---

## How each effect uses **Frequency** (`GetScaledFrequency()`)

| Effect           | Use of Frequency |
|------------------|------------------|
| **BreathingSphere** | Pulse/ripple rate (`progress * actual_frequency * 0.2`), ripple in space (`distance * (actual_frequency / half_diag)`), and color variation (rainbow uses `progress * 30`). |
| **Plasma**       | Pattern scale (`scale = actual_frequency * 0.004f / size_multiplier`) and **color cycle** (`hue = plasma_value * 360 + progress * 60`). Higher frequency = finer pattern + faster hue shift. |
| **Wave**         | Wave frequency: `freq_scale = actual_frequency * 0.1f / size_multiplier`; wave value `sin(position * freq_scale - progress)`. |
| **Spiral**       | Angular frequency: `freq_scale = actual_frequency * 0.003f / size_multiplier`; spiral angle uses `radius * freq_scale`. |
| **Explosion**    | Pattern / animation rate. |
| **DNAHelix**     | Helix winding rate. |
| **Tornado**      | Pattern rate. |
| **SurfaceAmbient** | `freq` for animation rate. |

**Unified meaning:** Frequency controls “how fast / how many” in time or space: pattern density, pulse rate, or **color cycle rate** (e.g. plasma-style color swirl). Effects that want a “color size/frequency” or “color swirl” like Plasma/BreathingSphere should use the **standard Frequency** slider for that (e.g. hue offset = `progress * GetScaledFrequency() * k`) and not add a separate color-speed slider.

---

## Color cycle (plasma / breathing style)

- **Plasma:** `hue = plasma_value * 360 + progress * 60` (rainbow); `GetColorAtPosition(plasma_value)` (gradient). So **Speed** moves the pattern in time; the fixed `60` gives a constant color cycle; we could make that term use `GetScaledFrequency()` so the **Frequency** slider speeds up the color swirl.
- **BreathingSphere:** Rainbow uses `distance * 30 + progress * 30` and pulse uses `progress * 60`; sphere radius and ripple use `GetScaledFrequency()`.

So “color size and frequency” can be achieved by:
- **Size** → spatial scale of the effect (already standard).
- **Frequency** → rate of color change over time (and/or pattern density). Use `GetScaledFrequency()` in the color path (e.g. hue offset or gradient position) so one slider drives “how fast colors change” like in Plasma/BreathingSphere.

No new sliders are required; standard **Size** and **Frequency** cover it if every effect uses them consistently.

---

## Traveling Light: before vs after

**Before (duplication):** Custom sliders for “Tail/beam size”, “Beam width”, “Wipe thickness”, “Beam thickness” duplicated the idea of “how big” the effect is. No Frequency → no standard color cycle.

**After (aligned with standard):**
- **Size:** One standard Size slider scales tail length, beam width, wipe thickness, and beam thickness (via `GetNormalizedSize()` and a single scale factor).
- **Frequency:** Standard Frequency slider enabled; used as color cycle rate (hue/gradient offset from `progress * GetScaledFrequency()`) in all styles so Traveling Light gets the same kind of plasma/breathing-style color change.
- **Effect-specific only:** Style (mode), Wipe edge (round/sharp/square), Panes divisions, Glow (for beam modes). No extra size-like or color-speed sliders.

This keeps the UI consistent, avoids doubling up with standard settings, and lets every style use the same Speed, Size, and Frequency in a predictable way.
