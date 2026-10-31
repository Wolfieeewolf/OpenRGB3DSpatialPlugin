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

Shared hard services (FFT, HID input, screen capture, game telemetry) stay in the player. Engines stay **in the plugin binary** for now; see [extract-later.md](extract-later.md).

### Registration (plugin Load)

1. `RegisterFolderVolumeEffects()` — Volume / Audio / Media from `effects/volume|audio|media`.
2. `RegisterPlayerEngines()` — Reactive, Screen Mirror (Ambilight), Movie Sync, Shader Field files.
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
6. **GPU standard:** viewport, preview, Volume/Audio/Media LED+overlay point eval, and remaining field engines (strip kernels, Shader Field atlas where used) run **OpenGL 4.1 Core**. Engine wrappers inject `#version 410 core`. Author `.fs` bodies stay **versionless** and must use Core GLSL (`texture()`, no `texture2D` / `attribute` / `varying` / `gl_FragColor`). Avoid GLSL reserved identifiers (`layout`, `packed`, `flat`, `shared`, ...).

Porting helpers: [shader-conversion.md](shader-conversion.md).

---

## Volume engine

**Runs:** soft 3D fields via `engines/volume-field` (`SpatialVolumeFieldEngine`) hosted by `engines/folder-volume` (FolderVolume) on shared **`SpatialOffscreenGlPool`** (**4.1 Core**; shares with the room viewport — no second context). LED coloring and room-grid overlay both use **true** `volumeMain` at each sample UV (`evaluateAtPoints` / batched bake); fail closed if GL is unavailable.  
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

(See full Volume / Audio / Media / Shader Field sections below for controls — paths above supersede older `effects/spatial` wording.)

## Shader Field engine

**Runs:** 2D `spatialMain` → offscreen atlas via `SpatialShaderEngine`, then sample onto LEDs by projection mode (floor/walls/sphere/…). Bodies load through `SpatialShaderCatalog::LoadShaderBodyFromPath` (same `#` / `key: value` header strip as Volume `.fs`).  
**Library category:** Shader Field (`effects/shader-field/`).

| Header keys (common) | Meaning |
|----------------------|---------|
| `class:` / `name:` / `description:` | Registration id + UI |
| `category:` | Library grouping (optional; folder wins) |

Author body: versionless GLSL with `spatialMain(out, frag_coord)`; uniforms `u_time`, `u_resolution`, `u_params[0..3]` (zoom / contrast / hue / detail). Do not put `#version` in the file.

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

**Reach / wave origin:** always each **display plane’s centre**. Screen Mirror does not use the layer Spatial Anchor (that control is for Volume / Movie Sync / etc.).

## Movie Sync engine

**Player codec** registered at Load via `RegisterMovieSyncEngine()`.  
Content: `movie-sync/effects/`. Sidecars beside the video.

## Game engine

Minecraft under library category **Game**. Bridge mod sources: `integrations/minecraft/` (jar goes in Minecraft’s mods folder).

## Quick chooser

```text
Soft 3D density / plasma / shells     → Volume
2D shadertoy-like look into the room  → Shader Field
Image / GIF / texture projection      → Media
Beat / FFT driven fields              → Audio
Desktop / monitor glow                → Ambilight
Keyboard / HID waves                  → Reactive
Film sidecar cues                     → Movie Sync
Minecraft Fabric telemetry            → Game
Timed trigger actions                 → Triggers (triggers/effects)
Film looks                            → Movie Sync (movie-sync/effects)
```
