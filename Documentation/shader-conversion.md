# Shader conversion playbook (1D / 2D / 3D)

Room-scale 3D LED effects are rare compared to WLED matrix (2D) and 8³ LED cubes. This plugin’s edge is **Spatial Anchor + true `volumeMain` at LED sample UVs + GPU atlas room-grid overlay + strip kernels unfolded into the room** — not copying cube demos.

**Engine contracts (authoring rules):** [effects-engines.md](effects-engines.md) — read that first when adding or converting an effect.

## Contracts (lanes → engines)

| Lane | Entry point | Engine |
|------|-------------|--------|
| **1D** | `evalStripKernelSigned(kid, s01, phase, repeats, time)` | Patterns — `effects/patterns/*.kernel` |
| **2D** | `spatialMain(out, frag_coord)` + `u_time` / `u_resolution` / `u_params[0..3]` | Shader Field — `effects/shader-field/*.fs` |
| **3D** | `volumeMain(out, p01)` soft field | Volume — AppData `effects/volume/<id>.fs` (or audio/media) via `engines/folder-volume` |

Also: **Media** / **Audio** (Volume host + texture or FFT), **Reactive** / **Ambilight** (player codecs), **Games** (programmed packs). Details in [effects-engines.md](effects-engines.md).

## Triage

When porting external shaders, reject: `iChannel`, `iMouse`, raymarch, audio/webcam. Prefer soft plasma / noise / ripples / aurora for 2D; strip chase/comet for 1D; true density fields for 3D.

## Licences

Using an idea or technique from another shader is fine. Copying its code is not, unless its licence allows it:

- **Shadertoy** shaders are licensed CC BY-NC-SA 3.0 unless the author states otherwise. That licence is not compatible with this project (GPL-2.0-or-later), so do not copy their code. Write your own version of the effect, or use a shader whose author gives a permissive licence (MIT, BSD, CC0, public domain).
- When a file contains code from another project, add a header line naming the source, the author and the licence, for example `# Source: <url> by <author> (MIT)`.

## Adapters

**Shadertoy → 2D:** `mainImage` → `spatialMain`; `iTime` → `u_time` (both are seconds; ours is Speed-scaled, so Speed 100 matches Shadertoy and Speed 0 freezes); `iResolution` → `u_resolution`; map look to zoom/contrast/hue/detail (`u_params[0..3]`). Drop the file in `effects/shader-field/`. Plugin strips `#` / `key: value` headers via `LoadShaderBodyFromPath` before compile.

**Room fill:** LED UV is a **square** occupancy domain (Floor / walls), not the 16:9 overlay atlas. Prefer `vec2 p = (uv - 0.5) * (2.0 * zoom)` — do **not** multiply by `u_resolution.x/y` aspect (that darkens the room sides). Soft radial fades, if any, start past `r ≈ √2`. Same rule as Volume’s `(p01 - 0.5) * 2.0`.

**2D → 3D (only soft fields):** replace UV with a plane from `p01` (e.g. `p01.xz`); output intensity in R (optional palette in G). Install under AppData `effects/volume/` for **Volume** (or `effects/audio/` / `effects/media/`) with a FolderVolume `class:` header — not under the plugin `engines/` tree.

**Strip → 1D:** drop a `effects/patterns/<name>.kernel` file. Set `index` so saved ids stay stable. The plugin composes `evalStripKernelSigned` from the folder.

**Pattern labels:** a combo fed by `pattern:` lines in the effect `.fs` file. Order is the saved index. Text after `|` is the item tooltip.

## Shared VFX helpers (OpenGL 4.1)

Engines inject [`SpatialVfxLib410`](../shared/Shaders/SpatialVfxLib410.h) before author bodies. Use for hologram / magic / smoke language without reinventing soft blobs:

| Helper | Use |
|--------|-----|
| `vfxDetailWidth(detail01)` | Map Detail 0..1 soft → sharp (mid ≈ old Detail 100) |
| `vfxCoreRim` / `vfxShell` / `vfxStreak` | Bright core + rim / motion streaks |
| `vfxSd*` (sphere/box/torus/capsule/plane/octa/hex) | SDF features in room space |
| `vfxSmoothMin` / `vfxRot*` / `vfxDomainWarp` | Blend / spin / smoke warp |
| `vfxFbm3` / `vfxRidge3` / `vfxCurl2` | Smoke / twist motion |
| `vfxEnvOnce` / `vfxPulse01` / `vfxHeatRgb` / `vfxPalette3` | Timing + colour language |

Particle and ribbon Action bodies compile through the Volume field engine and already call these helpers.

## GPU vs CPU

Volume/Audio/Media/Shader Field LED point-eval, room-grid overlay handoff, strip kernel bake, and the room viewport use **OpenGL 4.1 Core** with `#version 410 core` wrappers. Author `.fs` bodies must not include `#version` or legacy GLSL (`texture2D`, `attribute`, `varying`, `gl_FragColor`). Do not use GLSL reserved words as identifiers (`layout`, `packed`, `flat`, `shared`, ...) — rename them (e.g. `place`, `pack_val`, `y_fall`).

GPU for room fields — including audio visual fields via `SpatialVolumeFieldAssist` — runs on shared **`SpatialOffscreenGlPool`** (no second context; fail closed if GL is unavailable — no parallel CPU formula for the same look).

**Screen Mirror / Ambilight split (intentional — do not fake a volumeMain dual):**

| Stage | Where | Notes |
|-------|--------|--------|
| Desktop duplication | **GPU** (DXGI D3D11) | Prefer SDR BGRA; HDR formats refuse → GDI |
| Capture downscale | **GPU** (D3D11 blit) when quality &lt; native | Cap (default 1080p) before CPU readback; publish the just-written staging buffer (Flush+Map) so LEDs are not stuck one present behind |
| Full-res Native / GDI / Linux·mac grab | **CPU** | BitBlt or `QScreen::grabWindow` + box scale |
| Plane UV, zones, falloff, wave delay, EMA, grade | **CPU** | Per-LED; OpenRGB write path is CPU colours |
| Viewport plane preview | **GPU** (GL upload of same CPU frame) | Display only |

Procedural Volume/FolderVolume use true-GPU point eval because the heavy work is GLSL. Ambilight’s heavy work is already the capture; LED sample is a few loads — keeping mapping on CPU is the correct ceiling until a dedicated surface sampler exists (not volumeMain). Reach/wave always radiate from **each display plane’s centre** (not the layer Spatial Anchor); screen UVs always come from that plane. Span/falloff/time-to-edge use the live grid AABB × `grid_scale_mm`. CPU also keeps **analysis** (FFT / bands / onset in `AudioInputManager`) and Minecraft.
