# Effects engines

The player ships a **small set of engines**. Authors drop **content** under `library/<category>/` — like tracks for a player. Do not add a new engine per look.

**Folder name = library category.** Full map: [content-library.md](content-library.md).

| Engine (library category) | Content lives in | Who extends it |
|--------|------------------|----------------|
| **Volume** | `library/volume/*.fs` | Authors — thin `.fs` |
| **Audio** | `library/audio/*.fs` | Authors — thin `.fs` on one FFT service |
| **Media** | `library/media/*.fs` | Authors — image/GIF/texture `.fs` (not sound/movies) |
| **Shader Field** | `library/shader-field/*.fs` | Authors — thin `.fs` |
| **Patterns** | `library/patterns/*.kernel` | Authors — strip kernels |
| **Packs** | `library/packs/*.oreffect.json` | Effect Pack maker / Event Bindings |
| **Reactive** | Player codec (`library/reactive/` for notes) | HID input + waves |
| **Ambilight** | Player codec (`library/ambilight/` for notes) | Screen Mirror capture |
| **Movie Cue** | Player codec + `library/movie-cue/` | Script API + VLC sidecars |
| **Game** | Player codec (`library/game/` for notes) | Minecraft bridge |

Shared hard services (FFT, HID input, screen capture, game telemetry) stay in the player. Engines stay **in the plugin binary** for now; see [extract-later.md](extract-later.md).

### Registration (plugin Load)

1. `RegisterFolderVolumeEffects()` — Volume / Audio / Media from `library/volume|audio|media`.
2. `RegisterPlayerEngines()` — Reactive, Screen Mirror (Ambilight), Movie Cue, Shader Field files.
3. Minecraft — `REGISTER_EFFECT_3D` under **Game**.

**Movie Cue:** add the effect to the stack and enable Script API. Cue timelines are **sidecars next to the video** (`Movie.movie_cue.json`). Shared looks: `library/movie-cue/`. VLC helper: `library/movie-cue/vlc/`. Authoring: [movie-cue-authoring.md](../presets/library/movie-cue/docs/movie-cue-authoring.md).

**Stock pack:** copy from [`presets/library/`](../presets/library/) into AppData `library/`, or use the presets repo. Reload OpenRGB so disk registrations pick up new files.

---

## Shared author rules

1. Pick an engine first. If none fits, that is a new **programmed** effect — not a fake Volume file.
2. One effect → one primary content file (plus kernels/assets it references).
3. No parallel CPU formula beside a GPU atlas for the same field.
4. Prefer shared globals (`speed`, `brightness`, `size`, `scale`, …) over one-off knobs when ≥2 effects need the same control.
5. Reject ports that need `iChannel`, `iMouse`, raymarch, webcam, or per-effect native hooks unless you are writing a programmed addon.
6. **GPU standard:** viewport, preview, and all field atlases run **OpenGL 4.1 Core**. Engine wrappers inject `#version 410 core`. Author `.fs` bodies stay **versionless** and must use Core GLSL (`texture()`, no `texture2D` / `attribute` / `varying` / `gl_FragColor`). Avoid GLSL reserved identifiers (`layout`, `packed`, `flat`, `shared`, …).

Porting helpers: [shader-conversion.md](shader-conversion.md).

---

## Volume engine

**Runs:** soft 3D fields via `SpatialVolumeFieldEngine` / FolderVolume on the shared **4.1 Core** offscreen pool (shares with the room viewport).  
**Library category:** Volume (`library/volume/`).

### Required header (FolderVolume)

```text
category: Volume
```

(`category:` in the file is optional for library grouping — registration uses the folder. Prefer `category: Volume` in new files.)

| Header keys | Meaning |
|-------------|---------|
| `media_layout:` | `projection` or `omni` selects the shared media prepare codec (not `class_name` forks). Legacy class ids still map if this key is omitted |
| `sample: room` | Wall/floor/ceiling UV (`SampleGpuRoomVolume01`); default is origin-local occupancy |

(See full Volume / Audio / Media / Shader Field sections below for controls — paths above supersede older `effects/spatial` wording.)

## Shader Field engine

**Library category:** Shader Field (`library/shader-field/`).

## Media engine

**Runs:** Volume engine + media texture (`u_media`) + browse/GIF host.  
**Library category:** Media (`library/media/`). Images/GIF/textures only — not audio, not Movie Cue.

## Audio engine

**Runs:** One FFT/bands/onset service (`AudioInputManager`) feeding Volume fields via FolderVolume (`drive: audio`, `finish: audio`).  
**Library category:** Audio (`library/audio/`).

## Reactive engine

**Player codec** registered at Load via `RegisterReactiveEngine()`.  
Library category **Reactive**. Optional notes in `library/reactive/`.

## Ambilight engine (Screen Mirror)

**Player codec** registered at Load via `RegisterScreenMirrorEngine()`.  
Library category **Ambilight**. Optional notes in `library/ambilight/`.

## Movie Cue engine

**Player codec** registered at Load via `RegisterMovieCueEngine()`.  
Content: `library/movie-cue/`. Sidecars beside the video.

## Game engine

Minecraft sub-effects under library category **Game**. Optional notes in `library/game/`.

## Quick chooser

```text
Soft 3D density / plasma / shells     → Volume
2D shadertoy-like look into the room  → Shader Field
Image / GIF / texture projection      → Media
Beat / FFT driven fields              → Audio
Desktop / monitor glow                → Ambilight
Keyboard / HID waves                  → Reactive
Film sidecar cues                     → Movie Cue
Minecraft Fabric telemetry            → Game
Timed pack sequences                  → Packs (library/packs)
```
