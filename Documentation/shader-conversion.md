# Shader conversion playbook (1D / 2D / 3D)

Room-scale 3D LED effects are rare compared to WLED matrix (2D) and 8³ LED cubes. This plugin’s edge is **Spatial Anchor + volume atlas + strip kernels unfolded into the room** — not copying cube demos.

**Engine contracts (authoring rules):** [effects-engines.md](effects-engines.md) — read that first when adding or converting an effect.

## Contracts (lanes → engines)

| Lane | Entry point | Engine |
|------|-------------|--------|
| **1D** | `evalStripKernelSigned(kid, s01, phase, repeats, time)` | Kernel — `patterns/*.kernel` |
| **2D** | `spatialMain(out, frag_coord)` + `u_time` / `u_resolution` / `u_params[0..3]` | Shader Field — `effects/shader-field/*.fs` |
| **3D** | `volumeMain(out, p01)` soft field | Volume — `effects/spatial/<id>.fs` (or audio/media) via FolderVolume |

Also: **Media** / **Audio** (Volume host + texture or FFT), **Reactive** / **Ambilight** (player codecs), **Games** (programmed packs). Details in [effects-engines.md](effects-engines.md).

## Triage

When porting external shaders, reject: `iChannel`, `iMouse`, raymarch, audio/webcam. Prefer soft plasma / noise / ripples / aurora for 2D; strip chase/comet for 1D; true density fields for 3D.

## Adapters

**Shadertoy → 2D:** `mainImage` → `spatialMain`; `iTime` → `u_time`; `iResolution` → `u_resolution`; map look to zoom/contrast/hue/detail (`u_params[0..3]`). Drop the file in `effects/shader-field/`.

**2D → 3D (only soft fields):** replace UV with a plane from `p01` (e.g. `p01.xz`); output intensity in R (optional palette in G). Drop the file in `effects/spatial/` for **Volume** (or `effects/audio/` / `effects/media/`) with a FolderVolume `class:` header.

**Strip → 1D:** drop a `patterns/<name>.kernel` file. Set `index` so saved ids stay stable. The plugin composes `evalStripKernelSigned` from the folder.

**Pattern labels:** a combo fed by `pattern:` lines in the effect `.fs` file. Order is the saved index. Text after `|` is the item tooltip.

## GPU vs CPU

All field atlases (Volume / Strip / Shader Field) and the room viewport use **OpenGL 4.1 Core** with `#version 410 core` wrappers. Author `.fs` bodies must not include `#version` or legacy GLSL (`texture2D`, `attribute`, `varying`, `gl_FragColor`). Do not use GLSL reserved words as identifiers (`layout`, `packed`, `shared`, …) — rename them (e.g. `place`, `pack_val`).

GPU for room fields — including audio visual fields via `SpatialVolumeFieldAssist`. **Screen Mirror** stays on CPU: DXGI/GDI capture, `SpatialMapToScreen`, nearest-texel sample, optional LED EMA, then color grade. Falloff/wave origin is Spatial Anchor (or a layout point); screen UVs always come from the plane. Span/falloff/time-to-edge use the live grid AABB × `grid_scale_mm`. CPU also keeps **analysis** (FFT / bands / onset in `AudioInputManager`) and Minecraft.
