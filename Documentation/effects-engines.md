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
| **Action Editor Tools** | `action-editor/toolbox/` | Authors — `.fx` tools |
| **Reactive** | Player codec (no Effect Library folder) | HID input + waves |
| **Ambilight** | Player codec (no Effect Library folder) | Screen Mirror capture |
| **Movie Sync** | Player codec + `movie-sync/effects/` | Script API + VLC sidecars |
| **Game** | Player codec + `game-integrations/minecraft/` | Minecraft bridge |

Shared hard services (FFT, HID input, screen capture, game telemetry) stay in the player. Engines stay **in the plugin binary** for now; see [extract-later.md](extract-later.md).

### Registration (plugin Load)

1. `RegisterFolderVolumeEffects()` — Volume / Audio / Media from `effects/volume|audio|media`.
2. `RegisterPlayerEngines()` — Reactive, Screen Mirror (Ambilight), Movie Sync, Shader Field files.
3. Minecraft — `REGISTER_EFFECT_3D` under **Game**.

**Movie Sync:** add the effect to the stack and enable Script API. Cue timelines are **sidecars next to the video** (`Movie.movie_sync.json`). Shared looks / VLC helper / authoring notes ship in the [presets repo](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets) under `movie-sync/`.

**Content pack:** the plugin does **not** ship Volume/Audio/Media looks, Action toolbox `.fx`, trigger actions, or Movie Sync catalogs. Install from [OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets) into AppData (same folder names). Reload OpenRGB so disk registrations pick up new files.

---

## Shared author rules

1. Pick an engine first. If none fits, that is a new **programmed** effect â€” not a fake Volume file.
2. One effect â†’ one primary content file (plus kernels/assets it references).
3. No parallel CPU formula beside a GPU atlas for the same field.
4. Prefer shared globals (`speed`, `brightness`, `size`, `scale`, â€¦) over one-off knobs when â‰¥2 effects need the same control.
5. Reject ports that need `iChannel`, `iMouse`, raymarch, webcam, or per-effect native hooks unless you are writing a programmed addon.
6. **GPU standard:** viewport, preview, and all field atlases run **OpenGL 4.1 Core**. Engine wrappers inject `#version 410 core`. Author `.fs` bodies stay **versionless** and must use Core GLSL (`texture()`, no `texture2D` / `attribute` / `varying` / `gl_FragColor`). Avoid GLSL reserved identifiers (`layout`, `packed`, `flat`, `shared`, â€¦).

Porting helpers: [shader-conversion.md](shader-conversion.md).

---

## Volume engine

**Runs:** soft 3D fields via `SpatialVolumeFieldEngine` / FolderVolume on the shared **4.1 Core** offscreen pool (shares with the room viewport).  
**Library category:** Volume (`effects/volume/`).

### Required header (FolderVolume)

```text
category: Volume
```

(`category:` in the file is optional for library grouping â€” registration uses the folder. Prefer `category: Volume` in new files.)

| Header keys | Meaning |
|-------------|---------|
| `media_layout:` | `projection` or `omni` selects the shared media prepare codec (not `class_name` forks). Legacy class ids still map if this key is omitted |
| `sample: room` | Wall/floor/ceiling UV (`SampleGpuRoomVolume01`); default is origin-local occupancy |

(See full Volume / Audio / Media / Shader Field sections below for controls â€” paths above supersede older `effects/spatial` wording.)

## Shader Field engine

**Library category:** Shader Field (`effects/shader-field/`).

## Media engine

**Runs:** Volume engine + media texture (`u_media`) + browse/GIF host.  
**Library category:** Media (`effects/media/`). Images/GIF/textures only — not audio, not Movie Sync.

## Audio engine

**Runs:** One FFT/bands/onset service (`AudioInputManager`) feeding Volume fields via FolderVolume (`drive: audio`, `finish: audio`).  
**Library category:** Audio (`effects/audio/`).

## Reactive engine

**Player codec** registered at Load via `RegisterReactiveEngine()`.  
Library category **Reactive**. No Effect Library content folder.

## Ambilight engine (Screen Mirror)

**Player codec** registered at Load via `RegisterScreenMirrorEngine()`.  
Library category **Ambilight**. No Effect Library content folder.

## Movie Sync engine

**Player codec** registered at Load via `RegisterMovieSyncEngine()`.  
Content: `movie-sync/effects/`. Sidecars beside the video.

## Game engine

Minecraft under library category **Game**. Drop-in jar/notes: `game-integrations/minecraft/`.

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
