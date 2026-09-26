# Effects engines

The player ships a **small set of engines**. Authors drop **content** that those engines know how to run. Do not add a new engine per effect.

| Engine | Content lives in | Who extends it |
|--------|------------------|----------------|
| **Volume** | `effects/spatial/*.fs` (and media/audio when hosted) | Authors — thin `.fs` |
| **Shader Field** | `effects/shader-field/*.fs` | Authors — thin `.fs` |
| **Kernel / Strip** | `patterns/*.kernel` | Authors — thin kernels |
| **Media** | `effects/media/*.fs` | Authors — thin `.fs` (+ browse UI from host) |
| **Audio** | `effects/audio/*.fs` (target) | Authors — thin `.fs` on one FFT service |
| **Reactive** | Built-in effect | Player only — one engine, not a content farm |
| **Ambilight** | Built-in Screen Mirror | Player only — one capture engine |
| **Games** | Per-game pack / bridge | Standalone programmed content when not covered above |

Shared hard services (FFT, HID input, screen capture, game telemetry) stay in the player. Unique games may ship as **standalone programmed addons**.

---

## Shared author rules

1. Pick an engine first. If none fits, that is a new **programmed** effect — not a fake Volume file.
2. One effect → one primary content file (plus kernels/assets it references).
3. No parallel CPU formula beside a GPU atlas for the same field.
4. Prefer shared globals (`speed`, `brightness`, `size`, `scale`, …) over one-off knobs when ≥2 effects need the same control.
5. Reject ports that need `iChannel`, `iMouse`, raymarch, webcam, or per-effect native hooks unless you are writing a programmed addon.

Porting helpers: [shader-conversion.md](shader-conversion.md).

---

## Volume engine

**Runs:** soft 3D fields via `SpatialVolumeFieldEngine` / FolderVolume.  
**Entry:** `void volumeMain(out vec4 out_color, in vec3 p01)`  
**Uniforms:** `u_time`, `u_params[]` filled from header `param:` lines (order = index).

### Required header (FolderVolume)

```text
name: Display Name
class: StableClassId
category: Spatial
description: One short line
global: speed brightness frequency detail size scale …
param: …
finish: depth | hex | hsv | spiral | atlas | surface | rgb
# Effect
void volumeMain(...)
```

### Rules

| Rule | Detail |
|------|--------|
| `class:` | Stable id for profiles; required to register |
| `global:` | Opt-in shared motion/UI flags only |
| `finish:` | How CPU turns atlas sample into LED color |
| `sample: room` | Wall/floor/ceiling UV (`SampleGpuRoomVolume01`); default is origin-local occupancy |
| `resolution:` | Atlas resolution hint |
| Output | Soft field in channels the chosen `finish:` expects (intensity / HSV / RGB) — not raymarched scenes |
| Scale vs Size | **Scale** = occupancy of the effect box; **Size** = feature size inside `u_params` |
| Outside box | Unlit — do not clamp UVs onto atlas faces |

### Folder

`effects/spatial/<id>.fs` (presets repo / plugin data `effects/`).

---

## Shader Field engine

**Runs:** 2D full-frame fields via `SpatialShaderEngine`, sampled into the room by projection.  
**Entry:** `void spatialMain(out vec4 out_color, in vec2 frag_coord)`  
**Uniforms:** `u_time`, `u_resolution`, `u_params[0..3]` (zoom / contrast / hue / detail).

### Rules

| Rule | Detail |
|------|--------|
| No `class:` FolderVolume header | Shader Field host owns the effect list |
| Soft imagery only | Plasma, aurora, waves — not UI or video decode |
| `# Name` / `name:` | Title for the preset combo |
| Folder | `effects/shader-field/<id>.fs` |

---

## Kernel / Strip engine

**Runs:** 1D pattern along strips / colormaps.  
**Entry:** kernel body composed into `evalStripKernelSigned(...)`.

### Rules

| Rule | Detail |
|------|--------|
| File | `patterns/<name>.kernel` |
| `index` | Keep stable — saved pattern ids depend on it |
| Use | Strip colormap / Shell `pattern_source: kernels` — not a full room volume by itself |

---

## Media engine

**Runs:** Volume engine + media texture (`u_media`) + browse/GIF host.  
**Entry:** same `volumeMain`; sample the bound texture in GLSL.

### Rules

| Rule | Detail |
|------|--------|
| Header | Volume rules + `global: media` (or `media: yes`) + `finish: rgb` |
| Folder | `effects/media/<id>.fs` |
| Host owns | File browse, GIF timing, `setMediaTexture` |
| Content owns | UV / shape / ambience math in GLSL + declared knobs |

---

## Audio engine

**Runs:** One FFT/bands/onset service (`AudioInputManager`) feeding Volume fields via FolderVolume (`drive: audio`, `finish: audio`).  
**Content:** thin `effects/audio/*.fs` with `class:` + `audio_preset:`.

### Rules

| Rule | Detail |
|------|--------|
| Never fork FFT | All audio looks use the shared analysis service |
| Header | Volume rules + `drive: audio` + `audio_preset:` + `finish: audio` |
| Host owns | Smoothing, band/fill packing, pulse queue, note pack, AudioReactiveUi, color resolve |
| Content owns | `volumeMain` field math + unique sliders; do not apply `peak_boost` inside GLSL if `audio_media` is set |
| `audio_preset:` | `level` · `spectrum` · `beat` · `low_punch` · `high_sparkle` |
| `audio_media: bands` | Host builds N×1 noise-gated band texture; shader reads `u_media`; no second peak_boost in shader |
| `audio_media: spectrogram` | Host maintains 64-col × 72-row history; uploads bars (64×1) or spectrogram (64×72) depending on `display_mode` combo |
| Pulse queue | Auto-enabled when `audio_preset` is `beat` or `low_punch`; host packs up to 5 `{age, strength}` pairs into `u_params[14..23]`; shader outputs `(energy, gradient, pulse_idx01)` |
| Note pack | Auto-enabled when `audio_preset` is `high_sparkle`; host packs 4 notes (amp ≥ 0.10) into `u_params[12..19]` + count/time at 20/21; shader outputs `(intensity, note_hue01, gradient)` |
| `shader:` override | A `.fs` header may delegate GLSL to another effect's shader (e.g. `bass-punch.fs` reuses `audio-pulse.fs`) |
| Converted | Audio Level, Audio Paintbrush, Spectrum Bars, Audio Strip Visualizer, Audio Pulse, Bass Punch, Note Sparkle |

### Anti-flicker rules (content)

- Do not multiply band values inside GLSL when `audio_media` is set — the host applies `AudioVisualNoiseGate` and `peak_boost` before upload.
- Use softstep gates in the shader for any secondary gating (0.06–0.18 range for bars, 0.08–0.16 for sparkle).
- Keep sweep / radial multipliers in the range **0.92–1.0** (not 0.7 floor which caused ambient flash).
- `height_fade` for pulse: min 0.0 (not 0.15) so the top of the zone stays dark with no beat.
- Note sparkle gate range **0.10–0.16** to suppress ambient hiss cloud.

### Folder

`effects/audio/<id>.fs`

---

## Reactive engine

**Built-in.** HID → LED origins → waves → LED color.

### Rules

| Rule | Detail |
|------|--------|
| One engine / one effect | Not a folder of author reactive variants |
| Input stays in player | `ReactiveInputManager` / key map |
| Optional later | Look skins as data on top of the same wave buffer — still one engine |

---

## Ambilight engine (Screen Mirror)

**Built-in.** Capture → display-plane map → LEDs (CPU path).

### Rules

| Rule | Detail |
|------|--------|
| One engine / one effect | Not a content farm |
| Capture stays in player | DXGI/GDI + plane math |
| Do not | Reimplement capture inside a Volume `.fs` |

---

## Games / programmed addons

Use when no standard engine covers the behavior (title-specific telemetry, custom bridges).

### Rules

| Rule | Detail |
|------|--------|
| Prefer shared telemetry | `GameTelemetryBridge` / room sample protocol when possible |
| Per-game pack | Mappings, sub-effects, UI for that title |
| Fat native | Only when the title cannot speak the shared protocol |
| Do not | Stuff game logic into Volume/Shader Field files |

---

## Choosing an engine (quick)

```text
Soft 3D density / plasma / shells     → Volume
2D shadertoy-like look into the room  → Shader Field
Chase / comet along strips            → Kernel
Image or GIF in the room              → Media
Driven by music FFT/bands             → Audio (+ Volume content)
Key/mouse/gamepad pulses              → Reactive (built-in)
Desktop / monitor glow                → Ambilight (built-in)
Game vitals / world hooks             → Games pack
Anything else                         → Programmed addon (new host work)
```
