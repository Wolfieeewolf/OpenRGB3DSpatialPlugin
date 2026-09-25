# Shader conversion playbook (1D / 2D / 3D)

Room-scale 3D LED effects are rare compared to WLED matrix (2D) and 8³ LED cubes. This plugin’s edge is **Spatial Anchor + volume atlas + strip kernels unfolded into the room** — not copying cube demos.

## Contracts

| Lane | Entry point | Engine |
|------|-------------|--------|
| **1D** | `evalStripKernelSigned(kid, s01, phase, repeats, time)` | `pattern-kernels/*.kernel`, composed by `SpatialPatternKernelShader()`. Shell Pattern loads `spatial-effects/shell-pattern.fs`. Surface Look Pattern uses `PrepareStripColormapAssist` + `SampleEffectStripColormap01`. CPU fallback is a single sine. |
| **2D** | `spatialMain(out, frag_coord)` + `u_time` / `u_resolution` / `u_params[0..3]` | Shader Field loads `spatial-shaders/*.fs` |
| **3D** | `volumeMain(out, p01)` soft field | `spatial-effects/<id>.fs` via `SpatialVolumeFieldAssist` |

## Triage

When porting external shaders, reject: `iChannel`, `iMouse`, raymarch, audio/webcam. Prefer soft plasma / noise / ripples / aurora for 2D; strip chase/comet for 1D; true density fields for 3D.

## Adapters

**Shadertoy → 2D:** `mainImage` → `spatialMain`; `iTime` → `u_time`; `iResolution` → `u_resolution`; map look to zoom/contrast/hue/detail (`u_params[0..3]`). Drop the file in `spatial-shaders/`.

**2D → 3D (only soft fields):** replace UV with a plane from `p01` (e.g. `p01.xz`); output intensity in R (optional palette in G). Drop the file in `spatial-effects/`.

**Strip → 1D:** drop a `pattern-kernels/<name>.kernel` file. Set `index` so saved ids stay stable. The plugin composes `evalStripKernelSigned` from the folder.

**Pattern labels:** a combo fed by `pattern:` lines in the effect `.fs` file. Order is the saved index. Text after `|` is the item tooltip.

## GPU vs CPU

GPU for room fields — including **audio visual fields** (Audio Level, Spectrum Bars, Strip Viz, Pulse) via `SpatialVolumeFieldAssist`. **Screen Mirror** stays on CPU: DXGI/GDI capture, `SpatialMapToScreen` (3D direction from the display plane, both hemispheres), nearest-texel sample, optional LED EMA, then color grade. Falloff/wave origin is Spatial Anchor (or a layout point); screen UVs always come from the plane. Span/falloff/time-to-edge use the live grid AABB × `grid_scale_mm`. CPU also keeps **analysis** (FFT / bands / onset in `AudioInputManager`) and Minecraft.
