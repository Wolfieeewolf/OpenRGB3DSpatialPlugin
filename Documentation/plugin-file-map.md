# Plugin file map (engine regroup inventory)

**Group by runtime engine**, not presets toolbox folders. Content stays in OpenRGB3DSpatialPresets.

Status: **phases 1–10 applied** — phase 1–9 on main; phase 10 dissolves root effect/zone/controller/scene/utils into `shared/`. Only plugin entry stays at repo root. Incomplete multipasses parked for Claude ~2026-11-23 — [multipass-coverage.md](multipass-coverage.md).

**Regroup hygiene (2026-10-31):** re-checked folders against multipass “natural seam / no file explosion.” Engine buckets stay. Crumb Tab shards merged; dead `SpatialRoomDefaults` removed; `EffectCustomHost` header-only. Do **not** collapse Minecraft one-effect folders, `Effect*Row` widgets, or ScreenMirror Settings/Render — those are named seams.

Flag **rogue** = unclear owner / wrong tree / merge fodder.  
Flag **hub** = many engines call it; move last or leave in `shared/`.  
Flag **split** = over ~1k lines *and* a confirmed natural seam — not crumbs for the line count.

## Engine buckets

| Bucket | Owns at runtime |
|--------|-----------------|
| `engines/volume-field` | GPGPU `volumeMain` sample + Action volume bridge |
| `engines/particle-field` | GPGPU particles |
| `engines/ribbon-field` | GPGPU ribbons |
| `engines/folder-volume` | Disk `.fs` content host (Volume/Audio/Media library) |
| `engines/shader-field` | Shader Field effect + 2D `spatialMain` engine/catalog |
| `engines/screen-mirror` | Ambilight capture → room map |
| `engines/audio` | Capture + FFT + audio-reactive helpers |
| `engines/reactive` | HID/key reactive effect + input session |
| `engines/game` | Minecraft effects + SHM telemetry bridge + Fabric mod sources |
| `engines/movie-sync` | Movie Sync effect / catalog / HTTP / cue runtime |
| `engines/triggers` | Trigger sources + binding + runtime |
| `engines/actions` | Action pack player / applier / script (calls field engines) |
| `shared` | GL pool, room lighting/samplers, effect base, zones, controllers, types |
| `ui` | Qt forms / viewport / layout (tree stays for now) |
| `n/a` | Distro packaging (`debian/` `fedora/` `flatpak/`), assets, build helpers — not `release/` / `.wiki-seed/` |

## Tree (phases 1–10 landed)

```text
engines/
  movie-sync/       ← DONE  (was Effects3D/MovieSync/*)
  triggers/         ← DONE  (was Effects3D/Triggers/*)
  reactive/         ← DONE  (was Effects3D/Reactive/* + Input/Reactive*)
  game/
    Game/           ← DONE  (was root Game/*); StripPatternSurface.h → shared/ (phase 8)
    Minecraft/      ← DONE  (was Effects3D/Games/Minecraft/*)
  screen-mirror/    ← DONE  (was Effects3D/ScreenMirror/* + root ScreenCapture* + DisplayPlaneCaptureLabels.h)
                    DisplayPlane3D.* stays shared (room object)
  shader-field/     ← DONE  (was Effects3D/ShaderField/* + Shaders/SpatialShader{Engine,Catalog,Uniforms}.*)
  volume-field/     ← DONE  (was Actions/ActionVolumeField.* + Shaders/SpatialVolumeField{Assist,Engine}.*)
  particle-field/   ← DONE  (was Actions/ActionParticleField.*)
  ribbon-field/     ← DONE  (was Actions/ActionRibbonField.*)
  folder-volume/    ← DONE  (was Effects3D/FolderVolume/*)
  audio/            ← DONE  (was Audio/* + Effects3D/AudioReactive{Common,Ui}.h)
  actions/          ← DONE  (was Effects3D/Actions/*; field bridges already in volume/particle/ribbon)
                    ActionBuiltinGpu.*  ActionMedia.* stay here (call volume assist)
integrations/minecraft/   unchanged (Fabric mod sibling)
shared/             ← DONE phases 8+10
  effects/          SpatialEffect3D* EffectInstance* EffectListManager* StackPreset* types/register
  zones/            Zone* GridSpaceUtils LEDPosition Geometry3D
  controllers/      VirtualController ControllerLayout MatrixWiring DeviceType
  scene/            DisplayPlane* SceneProp* VirtualReferencePoint*
  utils/            PluginLog QtCompat TransformJson MediaTextureEffectUtils
  StripPatternSurface.h PlayerEngines.* EffectStratumBlend/Color/UiSync SpatialKernelColormap
  SpatialPatternKernels/* SpatialLighting/* SpatialRoom/* SpatialSamplers/* Shaders/*
OpenRGB3DSpatialPlugin.*   # entry stays at repo root
ui/                 ← DONE phase 9 (affinity folders)
  actions/ effects/ viewport/ controllers/ screen/ audio/ shell/
  widgets/ custom-controller-grid/ forms/   # kept
```

**Strip field note:** `SpatialStripField*` is a **hub** (FolderVolume strip colormap + pattern kernels). Prefer `shared/` unless a later pass proves FolderVolume-only.

**Volume field note:** `SpatialVolumeField*` is used by Action volume/particle/ribbon/builtin/media **and** FolderVolume — still one engine; bucket `engines/volume-field`, FolderVolume *calls* it.

---

## Inventory by bucket

### `engines/actions`

| Path | Role | Merge | Notes |
|------|------|-------|-------|
| `engines/actions/Action.{h,cpp}` | Clip / pack / clocks (`ClipSpan` / `GpuAnimMs`) | pending Claude ratchet | |
| `engines/actions/ActionDetail.h` | Internal detail | keep | |
| `engines/actions/ActionPlayer.h` | Player API | keep | |
| `engines/actions/ActionSerialize.cpp` | Pack JSON | keep | |
| `engines/actions/ActionScript.{h,cpp}` | Script tools | keep | ~1.3k — no natural second seam (2026-10-31) |
| `engines/actions/ActionLibrary.{h,cpp}` | Builtin library ids | keep | |
| `engines/actions/ActionClipEval.h` `ActionClipEvalAxis.cpp` | Axis / LED eval | keep | |
| `engines/actions/ActionApplier*` | Apply pack → LEDs | keep | |
| `engines/actions/ActionSpatial.cpp` | Spatial apply helpers | keep | |
| `engines/actions/ActionBuiltinGpu.*` | plasma/fire/snow/colorwash GPU | keep w/ actions | Uses volume assist |
| `engines/actions/ActionMedia.*` | Image/GIF clips | keep w/ actions | Uses volume/media path |

### `engines/volume-field` (phase 4)

| Path | Role | Merge | Notes |
|------|------|-------|-------|
| `engines/volume-field/ActionVolumeField.*` | Prepare/eval bridge | keep | |
| `engines/volume-field/SpatialVolumeFieldAssist.*` | Per-effect assist | keep | |
| `engines/volume-field/SpatialVolumeFieldEngine.*` | GPGPU engine | keep | ~1k — OK |

### `engines/particle-field` / `engines/ribbon-field` (phase 4)

| Path | Role | Merge |
|------|------|-------|
| `engines/particle-field/ActionParticleField.*` | Particle bridge | keep |
| `engines/ribbon-field/ActionRibbonField.*` | Ribbon bridge | keep |

(No separate GPGPU engine files yet — both reuse volume assist/program path.)

### `engines/folder-volume` (phase 5)

| Path | Role | Merge | Notes |
|------|------|-------|-------|
| `engines/folder-volume/FolderVolumeEffect.*` | `.fs` host + GPU sample | keep | seam landed 2026-10-31 (`_UI` + Spec) |
| `engines/folder-volume/FolderVolumeEffect_UI.cpp` | SetupCustomUI + media | keep | same class |
| `engines/folder-volume/FolderVolumeSpec.cpp` | ReadSpec + Register | keep | free helpers + register |

### `engines/shader-field` (phase 3)

| Path | Role | Merge | Notes |
|------|------|-------|-------|
| `engines/shader-field/ShaderField.*` | Effect + projection | fixed (partial hygiene) | |
| `engines/shader-field/SpatialShaderEngine.*` | 2D atlas | keep | |
| `engines/shader-field/SpatialShaderCatalog.*` | `.fs` load / patterns | keep | shared header-key helper |
| `engines/shader-field/SpatialShaderUniforms.h` | Uniform blob | keep | |

### `engines/screen-mirror` (phase 2)

| Path | Role | Merge | Notes |
|------|------|-------|-------|
| `engines/screen-mirror/ScreenMirror*` | Effect / settings / render / monitor panel | fixed (partial) | |
| `engines/screen-mirror/ScreenCaptureManager.*` | DXGI/GDI capture | keep | **hub** for planes UI too |
| `engines/screen-mirror/ScreenCaptureDownscale.h` | Scale helper | keep | |
| `engines/screen-mirror/DisplayPlaneCaptureLabels.h` | Combo labels | keep | Thin; ui includes across |

`DisplayPlane3D.*` stays **shared** (room object); capture *source id* on the plane is the seam.

### `engines/audio` (phase 6)

| Path | Role | Merge | Notes |
|------|------|-------|-------|
| `engines/audio/AudioInputManager.*` | FFT / bands / PCM | keep | seam landed 2026-10-31 (`_Wasapi`) |
| `engines/audio/AudioInputManager_Wasapi.cpp` | WASAPI capturer + devices | keep | same class |
| `engines/audio/AudioReactiveCommon.h` | Shared audio effect helpers | maybe `.cpp` later | header-only hub |
| `engines/audio/AudioReactiveUi.h` | Audio UI helpers | audio + ui | |

### `engines/reactive` (phase 1)

| Path | Role | Merge | Notes |
|------|------|-------|-------|
| `engines/reactive/Reactive.*` | Tick / waves / settings | keep | seam landed 2026-10-31 (`_UI`) |
| `engines/reactive/Reactive_UI.cpp` | SetupCustomUI + UI sync | keep | same class |
| `engines/reactive/ReactiveInputManager.*` | Session / devices | keep | |
| `engines/reactive/ReactiveInputTypes.h` | Types | keep | |
| `engines/reactive/ReactiveKeyMap.*` | Key map | keep | |
| `Input/ControllerTilt*` | DualSense | parked | On `feat/dualsense-controller-torch` only |

### `engines/game` (phase 1)

| Path | Role | Merge | Notes |
|------|------|-------|-------|
| `engines/game/Minecraft/**` | Health/Hunger/Air/Dura/Damage/Ambilight/… | parked deep-pass | |
| `engines/game/Game/GameTelemetryBridge.*` | Plugin↔SHM | keep | |
| `engines/game/Game/RoomSampleFrameProtocol.h` | Frame layout | keep | |
| `engines/game/Game/RoomSampleFrameShmReader.*` | SHM read | keep | |
| `engines/game/Game/RoomSampleConfigPublisher.*` | Config → mod | keep | |
| `engines/game/Game/RoomSampleShmPaths.h` | Paths | keep | |
| `shared/StripPatternSurface.h` | Strip surface enum | keep | Promoted out of game in phase 8 |
| `integrations/minecraft/src/**` | Fabric sender mod | keep out of DLL | Build/artifacts ignored |

### `engines/movie-sync` (phase 1)

| Path | Role | Merge |
|------|------|-------|
| `engines/movie-sync/MovieSync.*` | Effect UI + wiring | keep |
| `engines/movie-sync/MovieSyncCatalog.*` | catalog + dir scan | keep |
| `engines/movie-sync/MovieSyncListener.*` | Loopback HTTP | keep |
| `engines/movie-sync/MovieSyncRuntime.*` | Cue play | keep |

### `engines/triggers` (phase 1)

| Path | Role | Merge |
|------|------|-------|
| `engines/triggers/TriggerSource.h` | Base + edges | keep |
| `engines/triggers/ManualTriggerSource.h` | Manual | keep |
| `engines/triggers/TriggerBinding.*` | JSON + catalog | keep |
| `engines/triggers/TriggerRuntime.*` | event → play | keep |
| `engines/triggers/TriggerSourceRegistry.*` | Platform builder | keep |
| `engines/triggers/{Windows,Linux,Mac}TriggerSource.*` | OS sources | keep |
| `engines/triggers/LinuxLoginWatcher.h` | D-Bus | keep |

### `shared` — GL / fields hub

| Path | Role | Notes |
|------|------|-------|
| `shared/Shaders/SpatialOffscreenGlPool.*` | **hub** — singular GL 4.1 context | Never duplicate |
| `shared/Shaders/FieldFullscreenQuad410.*` | Shared fullscreen quad | |
| `shared/Shaders/SpatialFieldAssistBase.h` | Assist template | |
| `shared/Shaders/GlslUniformArray.h` | Uniform upload | |
| `shared/Shaders/SpatialStripFieldAssist.*` `SpatialStripFieldEngine.*` | Strip atlas **hub** | |

### `shared` — room / lighting / sample

| Path | Role | Notes |
|------|------|-------|
| `shared/SpatialLighting/*` | Occluders, AO, emitters, scene provider | |
| `shared/SpatialRoom/*` | Types, defaults, frame depth | |
| `shared/SpatialSamplers/*` | Cubemap map, layer core, basis | |
| `shared/SpatialLighting/RoomSpatialLightingUi.*` | JSON param bridge | keep |
| `shared/SpatialPatternKernels/*` | Kernel load + palettes | keep |

### `shared` — effect / stack / plugin core

| Path | Role | Notes |
|------|------|-------|
| `shared/effects/SpatialEffect3D.*` `_Eval` `_Settings` | Effect base | phase 10 |
| `shared/effects/SpatialEffectTypes.h` `EffectRegisterer3D.h` | Types / register | phase 10 |
| `shared/effects/EffectInstance3D.*` `EffectListManager3D.*` `StackPreset3D.*` | Stack | phase 10 |
| `shared/PlayerEngines.*` | Registers Reactive/ScreenMirror/MovieSync/ShaderField | keep |
| `shared/EffectStratumBlend.h` `EffectColorUtils.h` `EffectUiSync.h` `SpatialKernelColormap.h` | Cross-effect helpers | keep |
| `OpenRGB3DSpatialPlugin.*` | Entry | stays repo root |
| `shared/utils/PluginLog.h` `QtCompat.h` `TransformJson.h` `MediaTextureEffectUtils.h` | Utilities | MediaTexture = **hub** |

### `shared` — room objects / controllers / zones

| Path | Role | Notes |
|------|------|-------|
| `shared/zones/Zone3D.*` `ZoneGrid3D.*` `ZoneManager3D.*` `GridSpaceUtils.*` | Zones / grid | phase 10 |
| `shared/zones/LEDPosition3D.h` `Geometry3DUtils.h` | LED / math | phase 10 |
| `shared/controllers/VirtualController3D.*` `ControllerLayout3D.*` `MatrixWiringOrder.*` `DeviceTypeUtils.h` | Controllers | phase 10 |
| `shared/scene/DisplayPlane3D.*` `DisplayPlaneManager.h` | Display planes | Capture id seam → screen-mirror |
| `shared/scene/SceneProp3D.*` `ScenePropManager.h` `VirtualReferencePoint3D.*` | Props / refs | phase 10 |

### `ui` — affinity (phase 9)

| Affinity | Paths |
|----------|-------|
| actions / triggers | `ui/actions/*` |
| effects stack | `ui/effects/*` + `ui/widgets/Effect*` |
| viewport / scene | `ui/viewport/*` |
| controllers / grid | `ui/controllers/*` + `ui/custom-controller-grid/*` |
| screen / capture | `ui/screen/*` |
| audio | `ui/audio/*` + `ui/widgets/Audio*` |
| game | `ui/widgets/GameTelemetryStatusPanel.*` |
| shell / tab | `ui/shell/*` |
| forms | `ui/forms/*` (unchanged) |
| **parked Claude** | grid + viewport multipass; residual tab/dialog/widgets — see coverage |
### `n/a`

| Path | Notes |
|------|-------|
| `scripts/` `resources/` `images/` | Build helpers / assets (tracked) |
| `debian/` `fedora/` `flatpak/` | Distro packaging (tracked) |
| `release/` `.wiki-seed/` `.qtcreator/` | **Not** packaging — local/build/wiki seed only (gitignored; wiped in multipass cleanup) |

---

## Suggested move phases

1. **Leaf engines first** — **DONE** on `chore/engines-phase1-leaf` (merged to main).
2. **Screen-mirror** — **DONE** on `chore/engines-phase2-screen-mirror` (merged to main).
3. **Shader-field** — **DONE** on `chore/engines-phase3-shader-field` (ShaderField + SpatialShader*).
4. **Volume/particle/ribbon** — **DONE** on `chore/engines-phase4-fields` (bridges + SpatialVolumeField*).
5. **Folder-volume** — **DONE** on main (`engines/folder-volume`).
6. **Audio** — **DONE** on main (`engines/audio`).
7. **Actions core** — **DONE** on `chore/engines-phase7-actions` (`Effects3D/Actions` → `engines/actions`).
8. **shared polish** — **DONE** on `chore/engines-phase8-shared` (`shared/` hubs; effect base / zones / controllers still at root until phase 10).
9. **ui** — **DONE** on `chore/engines-phase9-ui` (affinity folders under `ui/`; bare includes via INCLUDEPATH).
10. **shared core dissolve** — **DONE** on `chore/engines-phase10-shared-core` (root effect/zone/controller/scene/utils → `shared/effects|zones|controllers|scene|utils`; plugin entry stays root).

Each phase: update `.pro`/CMake, fix includes, build, smoke one look per engine.

---

## Rogues / open calls

| Item | Issue | Lean |
|------|-------|------|
| `shared/StripPatternSurface.h` | Was under game | **DONE** phase 8 |
| `engines/screen-mirror/ScreenCaptureManager` | Used by Screen Mirror + plane UI | phase 2 done; ui includes across |
| `shared/Shaders/SpatialStripField*` | FolderVolume + patterns | **DONE** phase 8 |
| `SpatialVolumeField*` | Actions + FolderVolume | phase 4 done under `engines/volume-field` |
| `ActionBuiltinGpu` / `ActionMedia` | Live under Actions but touch volume assist | stay `engines/actions` |
| `shared/PlayerEngines.*` | Multi-engine register | **DONE** phase 8 |
| `DisplayPlane3D` vs capture | Plane is room; capture is screen-mirror | split by type, not file force-merge |
| Root effect base / zones / controllers / scene / utils | Was at repo root | **DONE** phase 10 → `shared/*` |

---

## Presets → engine (toolbox)

| Engine | Toolbox tools |
|--------|----------------|
| volume-field | `field_aurora`, `field_burst`, `field_dual_vortex`, `field_electric`, `field_flash`, `field_portal`, `field_ripple`, `field_room_glow`, `field_strata` |
| particle-field | `particle_embers`, `particle_fireflies`, `particle_meteors`, `particle_rain`, `particle_swarm`, `spark_burst` |
| ribbon-field | `laser_sweep`, `neon_cables`, `neon_ribbon`, `ribbon_helix` |
| folder-volume (scripted `.fx`) | confetti, helix, orbit, … |
| actions builtins | plasma / fire / snow / colorwash (no toolbox `shader:`) |
