# Effects engines

The player ships a **small set of engines** (power only). Authors drop **content** from the [presets repo](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets) under the feature’s own folder — like tracks for a player. Do not add a new engine per look, and do not bundle stock looks in the plugin.

**Folder name = feature / library category.** Full map: [content-library.md](content-library.md).

| Engine | Content lives in | Who extends it |
|--------|------------------|----------------|
| **Volume** | `effects/volume/*.fs` | Authors — thin `.fs` |
| **Audio** | `effects/audio/*.fs` | Authors — thin `.fs` on one FFT service |
| **Media** | `effects/media/*.fs` | Authors — image/GIF/texture `.fs` (not sound/movies) |
| **Shader Field** | `effects/shader-field/*.fs` | Authors — thin `.fs` |
| **Patterns** | `effects/patterns/*.kernel` | Authors — strip kernels |
| **Triggers / Actions** | `triggers/effects/*.action.json` | Action Editor → Save for Triggers |
| **Action Editor Tools** | `action-editor/toolbox/` | Authors — `.fx` tools; optional `shader:`/`field_*`, `particles:`/`particle_*`, or `ribbons:`/`ribbon_*`. LED apply runs **true** `volumeMain` at each LED on shared **`SpatialOffscreenGlPool`** (OpenGL 4.1; fail closed — no CPU dual, no second context, no gradient fake thumbs). Sibling tools share one header skeleton; `shader:`+`pulse` → `.fs` `u_params[2]`. Built-in GPU covers `plasma` / `fire` / `snow` / `colorwash` when no toolbox `shader:` is set. Media samples the full prepared frame at LED UVs. Clear-on-idle uses `ClearActionPreview`. |
| **Reactive** | Player codec (no Effect Library folder) | HID input + waves |
| **Ambilight** | Player codec (no Effect Library folder) | Screen Mirror capture |
| **Movie Sync** | Player codec + `movie-sync/effects/` | Script API + VLC sidecars |
| **Game** | Player codec (`integrations/minecraft/` mod sources) | Minecraft bridge |

Shared hard services (FFT, HID input, screen capture, game telemetry) stay in the player. Engines stay **in the plugin binary** for now.

### Registration (plugin Load)

1. `RegisterFolderVolumeEffects()` — Volume / Audio / Media from `effects/volume|audio|media`.
2. `RegisterPlayerEngines()` — Reactive, Screen Mirror (Ambilight), Movie Sync, Shader Field files (GPU UV point-eval for LEDs).
3. Minecraft — `REGISTER_EFFECT_3D` under **Game**.

**Movie Sync:** add the effect to the stack and enable Script API. Cue timelines are **sidecars next to the video** (`Movie.movie_sync.json`). Shared looks / VLC helper / authoring notes ship in the [presets repo](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets) under `movie-sync/`.

**Content pack:** the plugin does **not** ship Volume/Audio/Media looks, Action toolbox `.fx`, trigger actions, or Movie Sync catalogs. Install from [OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets) into AppData (same folder names). Reload OpenRGB so disk registrations pick up new files.

---

## Shared author rules

1. Pick an engine first. If none fits, that is a new **programmed** effect — not a fake Volume file.
2. One effect → one primary content file (plus kernels/assets it references).
3. No parallel CPU formula beside a GPU `volumeMain` / field program for the same look (fail closed if GL is unavailable).
4. Prefer shared globals (`speed`, `brightness`, `size`, `scale`, ...) over one-off knobs when >=2 effects need the same control.
5. Reject ports that need `iChannel`, `iMouse`, raymarch, webcam, or per-effect native hooks unless you are writing a programmed addon.
6. **GPU standard:** viewport, preview, Volume/Audio/Media/Shader Field LED point-eval, room-grid overlay atlas sampling, and strip kernels run **OpenGL 4.1 Core**. Engine wrappers inject `#version 410 core`. Author `.fs` bodies stay **versionless** and must use Core GLSL (`texture()`, no `texture2D` / `attribute` / `varying` / `gl_FragColor`). Avoid GLSL reserved identifiers (`layout`, `packed`, `flat`, `shared`, `half`, `fixed`, `filter`, ...); the presets smoke test checks them.

7. **Time and sliders:** `u_time` is **Speed-scaled seconds** in every engine (Shadertoy `iTime`; real seconds at Speed 100, frozen at Speed 0). Speed drives motion, Frequency drives hue cycling, **Size** is feature size, **Scale** is occupancy. Full slider and coordinate contract: wiki [Effect Controls](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/wiki/Effect-Controls) and [Coordinate Spaces](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/wiki/Coordinate-Spaces).
8. **Overlay:** every engine draws its own GPU room-grid overlay layer from the same data as its LEDs. No CPU overlay path.

Porting helpers: [shader-conversion.md](shader-conversion.md).

---

## Volume engine

**Runs:** soft 3D fields via `engines/volume-field` (`SpatialVolumeFieldEngine`) hosted by `engines/folder-volume` (FolderVolume) on shared **`SpatialOffscreenGlPool`** (**4.1 Core**; shares with the room viewport — no second context). LEDs use **true** `volumeMain` via `evaluateAtPoints` (PBO bake) then FolderVolume finish (HSV/surface/rainbow/…). Room-grid overlay samples the dense atlas on the GPU and applies the same finish family in `VolumeAtlasPoint` (no voxel CPU readback / no CPU overlay path). Fail closed if GL is unavailable.

**Colors:** Source **Look | Custom | Rainbow** × Style **Flat | Rich**. Flat keeps simple colour×intensity (and classic HSV rainbow); Rich adds shade/highlight materials and spectral rainbow. Default is Look+Rich; Flat/Basic stay first-class. Hue/Sat tint; Reset to Look restores authored `colors:`. Pack gloss in `out_color.b` for Rich tips (Flat ignores `.b`).

**Library category / AppData content:** Volume (`effects/volume/*.fs` — presets repo / plugin data root, not the C++ tree).

### Required header (FolderVolume)

```text
category: Volume
```

(`category:` in the file is optional for library grouping — registration uses the folder. Prefer `category: Volume` in new files.)

| Header keys | Meaning |
|-------------|---------|
| `media_layout:` | `projection` or `omni` selects the shared media prepare codec (required on those media looks — not inferred from `class_name`) |
| `instances:` | When >0, forces the effect instance count (e.g. searchlight cone default) |
| `sample: room` | Wall/floor/ceiling UV (`SampleGpuRoomVolume01`); default is origin-local occupancy |

Body: `void volumeMain(out vec4 out_color, in vec3 p01)`. `p01` is origin-local UV (anchor at 0.5) unless `sample: room`. `u_params[0]` = progress (Speed-scaled cycles); `u_time` = Speed-scaled seconds.

**Shared VFX lib (`SpatialVfxLib410`):** injected ahead of every Volume / particle / ribbon / strip / Shader Field body. Opt-in `vfx*` helpers — SDF primitives (sphere/box/torus/capsule/plane/octahedron/hex), smooth min/max, rotations, domain warp, ridge/fbm, curl, streak, pulse/once envelopes, heat/palette colours, `vfxCoreRim` / `vfxShell`, `vfxDetailWidth` (Detail 0..1 → calm soft → hologram sharp). Prefer core+rim over a single fat blob for showcase looks. Action particle/ribbon builtins use the lib. Existing library `.fs` that ignore `vfx*` are unchanged until rewritten.

**Volume overlay atlas:** `kMaxResolution` = 64 (LEDs still point-eval). Default assist resolution 32. Set `resolution:` in the `.fs` header up to 64 for denser room-grid preview.

## Shader Field engine

**Runs:** 2D `spatialMain` via `SpatialShaderEngine`. LED colours use **true GPU UV point-eval** (`BakeTrueGpuVolumeSamples` → `sampleAtUvs` with PBO readback) — same idea as Volume `evaluateAtPoints`; projection (floor/walls/sphere/…) stays on CPU. Overlay borrows a durable upright `SpatialOverlayHandoff` of the atlas (default 1024×576); fail closed when the GL pool is warm (CPU `exportAtlasRgba8` only if the pool is cold). Bodies load through `SpatialShaderCatalog::LoadShaderBodyFromPath` (same `#` / `key: value` header strip as Volume `.fs`).  
**Library category:** Shader Field (`effects/shader-field/`).

| Header keys (common) | Meaning |
|----------------------|---------|
| `class:` / `name:` / `description:` | Registration id + UI |
| `category:` | Library grouping (optional; folder wins) |

Author body: versionless GLSL with `spatialMain(out, frag_coord)`; uniforms `u_time` (Speed-scaled seconds), `u_resolution`, `u_params[0..3]` (zoom / contrast / hue / detail). `param:` lines beyond the first four feed `u_params[4..7]`; a matching `slider: name min max default [pct] | Label | tooltip` line shows it as an on-screen slider saved with the effect. Do not put `#version` in the file.

## Patterns engine

**Runs:** strip colormap — each `effects/patterns/*.kernel` adds a 1D kernel that colours LEDs along their strip (Colors and patterns section of any effect that supports it). Set `index:` so saved ids stay stable. Not a library category of its own.

## Media engine

**Runs:** Volume host (`engines/folder-volume`) + media texture (`u_media`) + browse/GIF host.  
**Library category / AppData content:** Media (`effects/media/*.fs`). Images/GIF/textures only — not audio, not Movie Sync.

## Audio engine

**Runs:** One FFT/bands/onset service (`engines/audio/AudioInputManager`) feeding Volume fields via `engines/folder-volume` (`drive: audio`, `finish: audio`).  
**Library category / AppData content:** Audio (`effects/audio/*.fs`).

## Reactive engine

**Player codec** registered at Load via `RegisterReactiveEngine()`.  
Library category **Reactive**. No Effect Library content folder.

## Ambilight engine (Screen Mirror)

**Player codec** registered at Load via `RegisterScreenMirrorEngine()`.  
Library category **Ambilight**. No Effect Library content folder.

**GPU vs CPU:** DXGI capture + quality downscale are GPU (D3D11); LED mapping (plane UV, zones, reach/falloff, wave, grade) stays CPU — same pattern as Action Media frame sampling, not FolderVolume `volumeMain`. Default capture quality is **1080p** so DXGI GPU-scales before CPU readback; **Native** is opt-in for full-res.

**Overlay:** stacked plane atlas via `SpatialOverlayHandoff` (GPU NEAREST scale into durable handoff; viewport borrows). Up to 4 planes, preview denseness up to 640×360. Temporal wave and smoothing are LED-only.

**Reach / wave origin:** always each **display plane’s centre**. Screen Mirror does not use the layer Spatial Anchor (that control is for Volume / Movie Sync / etc.).

**Screen UV (`SpatialMapToScreen`):** **outside / convex** sphere map (Newell dual with `pz − 1`) from the plane centre — light reads as an extension of the screen pushing **out** into the room (not a concave cave into mid-frame). Trade-off: clips that dive *into* a tunnel will feel wrong; ambilight is meant to send light out from the glass, not recreate into-the-screen perspective. Reach / falloff owns 360° brightness; wave / time-to-edge still adds travel delay.

## Movie Sync engine

**Player codec** registered at Load via `RegisterMovieSyncEngine()`.  
Content: `movie-sync/effects/`. Sidecars beside the video.

## Game engine

Minecraft under library category **Game**. Bridge mod sources: `integrations/minecraft/` (jar goes in Minecraft’s mods folder).

## GPU push (what runs where)

| Path | GPU | Notes |
|------|-----|--------|
| Volume / Audio / Media LEDs | Yes — `evaluateAtPoints` + display finish bake | Overlay-matching HSV/surface/hex/… when sample-only |
| Shader Field LEDs | Yes — `sampleAtUvs` + PBO | Projection UV on CPU |
| Action field / particle / ribbon | Yes — `EvaluateAtPoints` | |
| Action Media LEDs | Yes — `EvaluateMediaAtUvsGpu` | NEAREST + alpha; column-max for 1D; fail closed (no CPU dual) |
| Patterns strip bake | Yes bake, CPU sample | 512×1 — negligible |
| Room-grid Volume/Shader/Action overlay | Borrow `SpatialOverlayHandoff` | Volume zero-copy shared; Shader upright copy; fail closed when pool warm |
| Ambilight overlay atlas | GPU composite handoff | NEAREST scale + stack; fail closed when pool warm; CPU pack only if pool cold |
| Patterns overlay W×2 atlas | Pool `publishFromRgba32f` handoff | Fail closed when pool warm; LEDs still CPU `sample01` (512) |
| Ambilight capture | DXGI GPU | LED map intentionally CPU |
| Reactive waves | CPU (cheap) | Overlay wave tex GPU |

Target GPUs (Arc / 1050 Ti verified; typical 3060+) have headroom — prefer denser overlay atlases and point-eval over CPU field sampling.

## Quick chooser

```text
Soft 3D density / plasma / shells     → Volume
2D shadertoy-like look into the room  → Shader Field
Image / GIF / texture projection      → Media
Beat / FFT driven fields              → Audio
Desktop / monitor glow                → Ambilight
Keyboard / HID waves                  → Reactive
Film sidecar cues and looks           → Movie Sync (movie-sync/effects)
Minecraft Fabric telemetry            → Game
Timed trigger actions                 → Triggers (triggers/effects)
Strip chase / comet colormap          → Patterns (effects/patterns)
```
