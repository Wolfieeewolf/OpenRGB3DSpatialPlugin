# Shader conversion playbook (1D / 2D / 3D)

Room-scale 3D LED effects are rare compared to WLED matrix (2D) and 8³ LED cubes. This plugin’s edge is **Spatial Anchor + volume atlas + strip kernels unfolded into the room** — not copying cube demos.

## Contracts

| Lane | Entry point | Engine |
|------|-------------|--------|
| **1D** | `evalStripKernelSigned(kid, s01, phase, repeats, time)` | Strip assist + volume GLSL ([SpatialStripKernelEvalGlsl.h](../Effects3D/SpatialPatternKernels/SpatialStripKernelEvalGlsl.h)); unfold via `stripUnfoldKernelInputs` ([StripUnfoldFieldGlsl.h](../Effects3D/SpatialPatternKernels/StripUnfoldFieldGlsl.h)). Shell Pattern composes both into `volumeMain`. Surface Look Pattern uses `SpatialEffect3D::PrepareStripColormapAssist` + `SampleEffectStripColormap01` (8-family GPU SoT; full CPU kernel table is fallback only). |
| **2D** | `spatialMain(out, frag_coord)` + `u_time` / `u_resolution` / `u_params[0..3]` | Shader Field |
| **3D** | `volumeMain(out, p01)` soft field | `SpatialVolumeFieldAssist` |

## Triage

```bash
python tools/shader_triage/triage_shaders.py "C:\Users\wolfi\Downloads\effect-shaders" --out tools/shader_triage/last_triage.csv
```

Reject: `iChannel`, `iMouse`, raymarch, audio/webcam. Prefer soft plasma / noise / ripples / aurora for 2D; strip chase/comet for 1D; true density fields for 3D.

## Adapters

**Shadertoy → 2D:** `mainImage` → `spatialMain`; `iTime` → `u_time`; `iResolution` → `u_resolution`; map look to zoom/contrast/hue/detail (`u_params[0..3]`).

**2D → 3D (only soft fields):** replace UV with a plane from `p01` (e.g. `p01.xz`); output intensity in R (optional palette in G).

**Strip → 1D:** reduce to `s01` + time; add CPU + GLSL kernel in sync; bump `kSpatialStripGpuKernelMaxId`.

## Curated batch (first ports)

Bundled Shader Field presets (also under `resources/spatial_shaders/`):

| Id | Source dump | Notes |
|----|-------------|--------|
| `lobe_plasma` | AnotherPlasma | Multi-lobe product plasma |
| `noise_contour` | 2dNoiseContour | Gradient-noise topo bands |
| `corner_waves` | 4RadialWave | Four-corner interference |
| `aurora_ridge` | AnotherAuroraBorealis | Horizon ridge (vs Soft Aurora curtains) |
| `neon_warp` | 003Warpy | Clamped neon tunnels |
| `soft_blobs` | 002Blobby | Clamped neon blobs |

Skipped this round: `BlueDots` (dFdx / screen derivatives), `70sStripes` (not a clean 1D kernel; kernels 0–43 already cover chase/comet/fire), heavy multipass / mouse dumps from triage reject lane.

## GPU vs CPU

GPU for room fields — including **audio visual fields** (Audio Level, Spectrum Bars, Strip Viz, Pulse) via `SpatialVolumeFieldAssist` + optional `u_media`. CPU keeps **analysis** (FFT / bands / onset in `AudioInputManager`), screen capture, Minecraft, and cheap color finish after atlas sample.
