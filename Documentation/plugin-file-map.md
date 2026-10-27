# Plugin file map (engine regroup inventory)

**Group by runtime engine**, not presets toolbox folders. Content stays in OpenRGB3DSpatialPresets.

Status: **phase 1 applied** on branch `chore/engines-phase1-leaf` (movie-sync, triggers, reactive, game under `engines/`). Phases 2–8 still map-only. Incomplete multipasses parked for Claude ~2026-11-23 — [multipass-coverage.md](multipass-coverage.md).

Flag **rogue** = unclear owner / wrong tree / merge fodder.  
Flag **hub** = many engines call it; move last or leave in `shared/`.  
Flag **split** = over ~1k lines; split before or during regroup.

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
| `n/a` | Packaging, assets, build |

## Tree (phase 1 landed; later buckets still proposed)

```text
engines/
  movie-sync/       ← DONE  (was Effects3D/MovieSync/*)
  triggers/         ← DONE  (was Effects3D/Triggers/*)
  reactive/         ← DONE  (was Effects3D/Reactive/* + Input/Reactive*)
  game/
    Game/           ← DONE  (was root Game/*); StripPatternSurface.h stays here until shared/
    Minecraft/      ← DONE  (was Effects3D/Games/Minecraft/*)
  volume-field/     ActionVolumeField.*
                    Shaders/SpatialVolumeField{Assist,Engine}.*
  particle-field/   ActionParticleField.*
  ribbon-field/     ActionRibbonField.*
  folder-volume/    Effects3D/FolderVolume/*
                    (+ strip assist/engine if only FolderVolume-owned — see note)
  shader-field/     Effects3D/ShaderField/*
                    Shaders/SpatialShader{Engine,Catalog,Uniforms}.*
  screen-mirror/    Effects3D/ScreenMirror/*
                    ScreenCaptureManager.*  ScreenCaptureDownscale.h
                    DisplayPlaneCaptureLabels.h   # labels for capture UI
  audio/            Audio/*
                    Effects3D/AudioReactiveCommon.h  AudioReactiveUi.h
  actions/          Effects3D/Actions/*  except *Volume/Particle/Ribbon*Field*
                    ActionBuiltinGpu.*  ActionMedia.* stay here (call volume assist)
integrations/minecraft/   unchanged (Fabric mod sibling)
shared/
  Shaders/SpatialOffscreenGlPool.*   FieldFullscreenQuad410.*
  Shaders/SpatialFieldAssistBase.h   GlslUniformArray.h
  Shaders/SpatialStripField{Assist,Engine}.*   # strip atlas — FolderVolume + patterns; hub
  SpatialLighting/*  SpatialRoom/*  SpatialSamplers/*
  SpatialEffect3D*  SpatialEffectTypes.h  EffectRegisterer3D.h
  EffectInstance3D.*  EffectListManager3D.*  StackPreset3D.*  PlayerEngines.*
  Zone*  GridSpaceUtils.*  LEDPosition3D.h  Geometry3DUtils.h
  VirtualController*  ControllerLayout*  MatrixWiringOrder.*  DeviceTypeUtils.h
  DisplayPlane3D.*  DisplayPlaneManager.h     # room layout planes (capture id on plane)
  SceneProp*  VirtualReferencePoint*
  MediaTextureEffectUtils.h  TransformJson.h  PluginLog.h  QtCompat.h
  OpenRGB3DSpatialPlugin.*
  Effects3D/SpatialPatternKernels/*  Effects3D/SpatialLighting/RoomSpatialLightingUi.*
  Effects3D/Effect*.h (stratum/color/ui sync helpers)
ui/                 unchanged paths for now (affinity notes below)
```

**Strip field note:** `SpatialStripField*` is a **hub** (FolderVolume strip colormap + pattern kernels). Prefer `shared/` unless a later pass proves FolderVolume-only.

**Volume field note:** `SpatialVolumeField*` is used by Action volume/particle/ribbon/builtin/media **and** FolderVolume — still one engine; bucket `engines/volume-field`, FolderVolume *calls* it.

---

## Inventory by bucket

### `engines/actions`

| Path | Role | Merge | Notes |
|------|------|-------|-------|
| `Effects3D/Actions/Action.{h,cpp}` | Clip / pack / clocks (`ClipSpan` / `GpuAnimMs`) | pending Claude ratchet | |
| `Effects3D/Actions/ActionDetail.h` | Internal detail | keep | |
| `Effects3D/Actions/ActionPlayer.h` | Player API | keep | |
| `Effects3D/Actions/ActionSerialize.cpp` | Pack JSON | keep | |
| `Effects3D/Actions/ActionScript.{h,cpp}` | Script tools | **split later** | ~1.2k cpp |
| `Effects3D/Actions/ActionLibrary.{h,cpp}` | Builtin library ids | keep | |
| `Effects3D/Actions/ActionClipEval.h` `ActionClipEvalAxis.cpp` | Axis / LED eval | keep | |
| `Effects3D/Actions/ActionApplier*` | Apply pack → LEDs | keep | |
| `Effects3D/Actions/ActionSpatial.cpp` | Spatial apply helpers | keep | |
| `Effects3D/Actions/ActionBuiltinGpu.*` | plasma/fire/snow/colorwash GPU | keep w/ actions | Uses volume assist |
| `Effects3D/Actions/ActionMedia.*` | Image/GIF clips | keep w/ actions | Uses volume/media path |
| `Effects3D/Actions/ActionVolumeField.*` | → **volume-field** | move | Bridge only |
| `Effects3D/Actions/ActionParticleField.*` | → **particle-field** | move | |
| `Effects3D/Actions/ActionRibbonField.*` | → **ribbon-field** | move | |

### `engines/volume-field`

| Path | Role | Merge | Notes |
|------|------|-------|-------|
| `Effects3D/Actions/ActionVolumeField.*` | Prepare/eval bridge | move from Actions | |
| `Shaders/SpatialVolumeFieldAssist.*` | Per-effect assist | move from Shaders | |
| `Shaders/SpatialVolumeFieldEngine.*` | GPGPU engine | move | ~1k — OK |

### `engines/particle-field` / `engines/ribbon-field`

| Path | Role | Merge |
|------|------|-------|
| `Effects3D/Actions/ActionParticleField.*` | Particle bridge | move |
| `Effects3D/Actions/ActionRibbonField.*` | Ribbon bridge | move |

(No separate GPGPU engine files yet — both reuse volume assist/program path.)

### `engines/folder-volume`

| Path | Role | Merge | Notes |
|------|------|-------|-------|
| `Effects3D/FolderVolume/FolderVolumeEffect.*` | `.fs` host + UI | pending Claude ratchet / **split** | ~2.6k |

### `engines/shader-field`

| Path | Role | Merge | Notes |
|------|------|-------|-------|
| `Effects3D/ShaderField/ShaderField.*` | Effect + projection | fixed (partial hygiene) | |
| `Shaders/SpatialShaderEngine.*` | 2D atlas | move w/ engine | |
| `Shaders/SpatialShaderCatalog.*` | `.fs` load / patterns | move | shared header-key helper |
| `Shaders/SpatialShaderUniforms.h` | Uniform blob | move | |

### `engines/screen-mirror`

| Path | Role | Merge | Notes |
|------|------|-------|-------|
| `Effects3D/ScreenMirror/*` | Effect / settings / render / monitor panel | fixed (partial) | |
| `ScreenCaptureManager.*` | DXGI/GDI capture | move | **hub** for planes UI too |
| `ScreenCaptureDownscale.h` | Scale helper | move | |
| `DisplayPlaneCaptureLabels.h` | Combo labels | move or ui | Thin |

`DisplayPlane3D.*` stays **shared** (room object); capture *source id* on the plane is the seam.

### `engines/audio`

| Path | Role | Merge | Notes |
|------|------|-------|-------|
| `Audio/AudioInputManager.*` | Capture + FFT | **split** later | ~1.4k |
| `Effects3D/AudioReactiveCommon.h` | Shared audio effect helpers | maybe `.cpp` later | header-only hub |
| `Effects3D/AudioReactiveUi.h` | Audio UI helpers | audio + ui | |

### `engines/reactive` (phase 1)

| Path | Role | Merge | Notes |
|------|------|-------|-------|
| `engines/reactive/Reactive.*` | Effect body | **split** UI vs tick | ~1.5k |
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
| `engines/game/Game/StripPatternSurface.h` | Strip surface enum | shared-or-game | Used beyond game — leave here until phase 7 `shared/` |
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
| `Shaders/SpatialOffscreenGlPool.*` | **hub** — singular GL 4.1 context | Never duplicate |
| `Shaders/FieldFullscreenQuad410.*` | Shared fullscreen quad | |
| `Shaders/SpatialFieldAssistBase.h` | Assist template | |
| `Shaders/GlslUniformArray.h` | Uniform upload | |
| `Shaders/SpatialStripFieldAssist.*` `SpatialStripFieldEngine.*` | Strip atlas **hub** | |

### `shared` — room / lighting / sample

| Path | Role | Notes |
|------|------|-------|
| `SpatialLighting/*` | Occluders, AO, emitters, scene provider | |
| `SpatialRoom/*` | Types, defaults, frame depth | |
| `SpatialSamplers/*` | Cubemap map, layer core, basis | |
| `Effects3D/SpatialLighting/RoomSpatialLightingUi.*` | JSON param bridge | Thin; could live under SpatialLighting/ |
| `Effects3D/SpatialPatternKernels/*` | Kernel load + palettes | |

### `shared` — effect / stack / plugin core

| Path | Role | Notes |
|------|------|-------|
| `SpatialEffect3D.*` `_Eval` `_Settings` | Effect base | |
| `SpatialEffectTypes.h` `EffectRegisterer3D.h` | Types / register | |
| `EffectInstance3D.*` `EffectListManager3D.*` `StackPreset3D.*` | Stack | |
| `Effects3D/PlayerEngines.*` | Registers Reactive/ScreenMirror/MovieSync/ShaderField | Glue — shared |
| `Effects3D/EffectStratumBlend.h` `EffectColorUtils.h` `EffectUiSync.h` `SpatialKernelColormap.h` | Cross-effect helpers | |
| `OpenRGB3DSpatialPlugin.*` | Entry | |
| `PluginLog.h` `QtCompat.h` `TransformJson.h` `MediaTextureEffectUtils.h` | Utilities | MediaTexture = **hub** |

### `shared` — room objects / controllers / zones

| Path | Role | Notes |
|------|------|-------|
| `Zone3D.*` `ZoneGrid3D.*` `ZoneManager3D.*` `GridSpaceUtils.*` | Zones / grid | |
| `LEDPosition3D.h` `Geometry3DUtils.h` | LED / math | |
| `VirtualController3D.*` `ControllerLayout3D.*` `MatrixWiringOrder.*` `DeviceTypeUtils.h` | Controllers | |
| `DisplayPlane3D.*` `DisplayPlaneManager.h` | Display planes | Capture id seam → screen-mirror |
| `SceneProp3D.*` `ScenePropManager.h` `VirtualReferencePoint3D.*` | Props / refs | |

### `ui` — affinity (paths stay; regroup later if needed)

| Affinity | Paths |
|----------|-------|
| actions / triggers | `ActionsPanel.*` `ActionEditor*` `ActionTimeline*` `ActionToolBar.*` `ActionEffectFiles.*` `ActionGradient*` `ActionCurve*` `ActionUser*` `TriggersPanel.*` |
| effects stack | `EffectLibraryPanel.*` `EffectControlsHostPanel.*` `EffectGlobalSettingsPanel.*` `EffectStackPanel.*` `ui/widgets/Effect*` |
| viewport / scene | `LEDViewport3D*` `Gizmo3D*` `ui/viewport/*` `SceneProp*` `SceneTransform*` `SceneObject*` |
| controllers / grid | `CustomController*` `ui/custom-controller-grid/*` `ControllerCards.*` `ControllerListPanel.*` |
| screen / capture | `CaptureZonesWidget.*` `DisplayPlane*` |
| audio | `AudioInputPanel.*` `AudioAnalyzerSettings.*` `ui/widgets/Audio*` |
| game | `ui/widgets/GameTelemetryStatusPanel.*` |
| shell / tab | `OpenRGB3DSpatialTab*` `ObjectCreatorTabPanel.*` `ZonesPanel.*` `GridSettingsPanel.*` `PluginSettingsPaths.*` `PluginUiUtils.*` |
| **parked Claude** | grid + viewport multipass; residual tab/dialog/widgets — see coverage |

### `n/a`

| Path | Notes |
|------|-------|
| `scripts/` `resources/` `images/` | Assets |
| `debian/` `fedora/` `flatpak/` `release/` | Packaging |
| `.wiki-seed/` `.qtcreator/` | Dev only |

---

## Suggested move phases

1. **Leaf engines first** — **DONE** on `chore/engines-phase1-leaf` (movie-sync, triggers, reactive, game; integrations stay sibling).
2. **Screen-mirror** — ScreenMirror + ScreenCaptureManager; keep DisplayPlane in shared.
3. **Shader-field** — ShaderField + SpatialShader*.
4. **Volume/particle/ribbon** — move bridges + SpatialVolumeField*; update Actions includes.
5. **Folder-volume** — after volume-field stable (calls assist).
6. **Actions core** — remaining Actions/* into `engines/actions`.
7. **shared polish** — SpatialLighting/Room/Samplers, strip field hub, effect base, zones/controllers; promote `StripPatternSurface.h`; optional `Effects3D/` dissolve.
8. **ui** — path renames only if include churn is already paid; else leave.

Each phase: update `.pro`/CMake, fix includes, build, smoke one look per engine.

---

## Rogues / open calls

| Item | Issue | Lean |
|------|-------|------|
| `engines/game/Game/StripPatternSurface.h` | Used outside game | → `shared` in phase 7 |
| `ScreenCaptureManager` | Used by Screen Mirror + plane UI | → `engines/screen-mirror`, ui includes across |
| `SpatialStripField*` | FolderVolume + patterns | → `shared` (hub) |
| `SpatialVolumeField*` | Actions + FolderVolume | → `engines/volume-field` |
| `ActionBuiltinGpu` / `ActionMedia` | Live under Actions but touch volume assist | stay `engines/actions` |
| `PlayerEngines.cpp` | Multi-engine register | → `shared` |
| `DisplayPlane3D` vs capture | Plane is room; capture is screen-mirror | split by type, not file force-merge |
| Root `Effects3D/*` helpers | Orphan headers at Effects3D root | → `shared/effects/` or keep until dissolve |

---

## Presets → engine (toolbox)

| Engine | Toolbox tools |
|--------|----------------|
| volume-field | `field_aurora`, `field_burst`, `field_dual_vortex`, `field_electric`, `field_flash`, `field_portal`, `field_ripple`, `field_room_glow`, `field_strata` |
| particle-field | `particle_embers`, `particle_fireflies`, `particle_meteors`, `particle_rain`, `spark_burst` |
| ribbon-field | `laser_sweep`, `neon_ribbon`, `ribbon_helix` |
| folder-volume (scripted `.fx`) | confetti, helix, orbit, … |
| actions builtins | plasma / fire / snow / colorwash (no toolbox `shader:`) |
