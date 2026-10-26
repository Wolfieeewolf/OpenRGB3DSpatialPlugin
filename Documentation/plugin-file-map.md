# Plugin file map (engine regroup inventory)

Filled during multipass. **Group by runtime engine**, not presets toolbox folders. Content stays in OpenRGB3DSpatialPresets.

Status of regroup: map only until a dedicated branch. Flag **rogue** = unclear owner / wrong tree / merge fodder.

## Engine buckets

| Bucket | Owns |
|--------|------|
| `engines/volume-field` | GPGPU volume sample / Action volume field |
| `engines/particle-field` | GPGPU particles |
| `engines/ribbon-field` | GPGPU ribbons |
| `engines/folder-volume` | FolderVolume content host |
| `engines/shader-field` | ShaderField |
| `engines/screen-mirror` | Screen Mirror |
| `engines/audio` | Audio input + audio-reactive glue |
| `engines/reactive` | Key/button reactive + Input reactive |
| `engines/game` | Minecraft / game telemetry |
| `engines/movie-sync` | Movie Sync |
| `engines/triggers` | Trigger sources/runtime |
| `engines/actions` | Action player/applier/script (calls field engines) |
| `shared` | Clocks, GL pool, room/grid/LED types, cross-engine helpers |
| `ui` | Forms, widgets, viewport, layout |

## Inventory (seed — expand per multipass)

| Path | Role | Engine | Merge | Notes |
|------|------|--------|-------|-------|
| `Effects3D/Actions/*` | Action orchestration | engines/actions | keep/split later | 2026-10-27 ratchet keep: clocks + field bridges OK |
| `Effects3D/Actions/ActionVolumeField.*` | Volume field bridge | engines/volume-field | keep | ClipSpan + GpuAnimMs prepare |
| `Effects3D/Actions/ActionParticleField.*` | Particle field bridge | engines/particle-field | keep | per-clip setParams on shared seq |
| `Effects3D/Actions/ActionRibbonField.*` | Ribbon field bridge | engines/ribbon-field | keep | per-clip setParams on shared seq |
| `Effects3D/FolderVolume/*` | Content host | engines/folder-volume | keep / **split later** | ~2.6k lines; header parse OK |
| `Effects3D/ShaderField/*` | Shader field effect | engines/shader-field | fixed | 2026-10-27: shared `LoadShaderBodyFromPath`; `\u2013` labels |
| `Effects3D/ScreenMirror/*` | Ambilight / capture | engines/screen-mirror | fixed | 2026-10-27: QStringLiteral tooltips/empty label |
| `Effects3D/Reactive/Reactive.cpp` | Reactive effect body | engines/reactive | **split** UI vs wave tick later | ~1563 lines; legacy ring_width remap removed 2026-10-27 |
| `Effects3D/Reactive/Reactive.h` | Reactive API | engines/reactive | keep with cpp | |
| `Effects3D/Games/*` | Minecraft stack | engines/game | | 2026-10-27: MinecraftGame.cpp dead code removed (`MakeRgb`, `RoomLedToSpatialCore`); include path fixed; strip-axis UI gap patched; MinecraftRoomAmbilightEffect3D description updated |
| `Effects3D/MovieSync/*` | Movie Sync | engines/movie-sync | | 2026-10-27: curly-quote escape fix (MovieSync.cpp); log msg aligned to UI (MovieSyncRuntime.cpp); ec2 for nested iterator (MovieSyncCatalog.cpp) |
| `Effects3D/MovieSync/MovieSync.h` | Effect host header | engines/movie-sync | keep | |
| `Effects3D/MovieSync/MovieSync.cpp` | Effect UI + wiring | engines/movie-sync | keep | fixed: \u201c/\u201d escape sequences in tr() |
| `Effects3D/MovieSync/MovieSyncCatalog.h` | Catalog types + API | engines/movie-sync | keep | |
| `Effects3D/MovieSync/MovieSyncCatalog.cpp` | catalog.json loader + dir scanner | engines/movie-sync | keep | fixed: ec2 for inner directory_iterator |
| `Effects3D/MovieSync/MovieSyncListener.h` | HTTP listener API | engines/movie-sync | keep | |
| `Effects3D/MovieSync/MovieSyncListener.cpp` | Loopback HTTP /v1/* handler | engines/movie-sync | keep | |
| `Effects3D/MovieSync/MovieSyncRuntime.h` | CueRuntime API | engines/movie-sync | keep | |
| `Effects3D/MovieSync/MovieSyncRuntime.cpp` | pulse/level play engine | engines/movie-sync | keep | fixed: log msg "strobe effects" -> "Allow photosensitive looks" |
| `Effects3D/Triggers/*` | Triggers | engines/triggers | | 2026-10-27: LoadCatalog ec fix (TriggerBinding.cpp); Title Case display names (WindowsTriggerSource.cpp) |
| `Effects3D/Triggers/TriggerSource.h` | EventSource base + EventEdge enum | engines/triggers | keep | |
| `Effects3D/Triggers/ManualTriggerSource.h` | Manual fire/hold source | engines/triggers | keep | |
| `Effects3D/Triggers/TriggerBinding.h` | Binding/Document types | engines/triggers | keep | |
| `Effects3D/Triggers/TriggerBinding.cpp` | JSON de/serialise + catalog loader | engines/triggers | keep | fixed: is_regular_file() without ec (stale-ec bug) |
| `Effects3D/Triggers/TriggerRuntime.h` | TriggerRuntime API | engines/triggers | keep | |
| `Effects3D/Triggers/TriggerRuntime.cpp` | event -> play engine | engines/triggers | keep | |
| `Effects3D/Triggers/TriggerSourceRegistry.h` | Registry API | engines/triggers | keep | |
| `Effects3D/Triggers/TriggerSourceRegistry.cpp` | Platform source builder | engines/triggers | keep | |
| `Effects3D/Triggers/LinuxLoginWatcher.h` | D-Bus watcher header | engines/triggers | keep | |
| `Effects3D/Triggers/LinuxTriggerSource.h` | Linux source header | engines/triggers | keep | |
| `Effects3D/Triggers/LinuxTriggerSource.cpp` | logind lock/sleep events | engines/triggers | keep | |
| `Effects3D/Triggers/WindowsTriggerSource.h` | Windows source header | engines/triggers | keep | |
| `Effects3D/Triggers/WindowsTriggerSource.cpp` | WTS/power/device/focus events | engines/triggers | keep | fixed: 7 display names to Title Case |
| `Effects3D/Triggers/MacTriggerSource.h` | macOS source header | engines/triggers | keep | |
| `Effects3D/Triggers/MacTriggerSource.cpp` | CF/IOKit lock/sleep events | engines/triggers | keep | |
| `Effects3D/SpatialLighting/RoomSpatialLightingUi.*` | Room spatial light param struct + JSON save/load | shared / room | keep | 2026-10-27: role and wiring kept (thin JSON bridge only) |
| `Effects3D/SpatialPatternKernels/SpatialPatternKernels.*` | Strip kernel disk loader + GLSL atlas builder | shared | keep | 2026-10-27: kept; correctness OK |
| `Effects3D/SpatialPatternKernels/SpatialPatternPalettes.*` | CPU palette lookup for strip colormap | shared | keep | 2026-10-27: kept; palette set matches kernel ids |
| `Effects3D/AudioReactiveCommon.h` | Audio effect shared helpers | engines/audio | keep / maybe split .cpp later | Large header-only |
| `Effects3D/AudioReactiveUi.h` | Audio UI helpers | engines/audio + ui | | |
| `Audio/AudioInputManager.*` | Capture + FFT bands | engines/audio | **split** capture vs analyze later | ~1378 lines |
| `Input/Reactive*` | Reactive input | engines/reactive | keep | Gated by effect running |
| `Input/ControllerTilt*` | DualSense tilt | parked | | Feature branch |
| `Shaders/SpatialOffscreenGlPool.*` | Singleton shared GL context (4.1 Core) for all field assists | shared | keep | 2026-10-27: first-registered host wins guard OK; RAII Session correct |
| `Shaders/SpatialFieldAssistBase.h` | Template fail-latch + lazy-create + per-frame prepare gate | shared | keep | 2026-10-27: per-track param push noted in comment; atlas vs program path correct |
| `Shaders/SpatialVolumeFieldAssist.*` | Per-effect volume field assist (thin owner over Engine) | shared | keep | 2026-10-27: evaluateAtPoints path, mediaDirty refresh, prepareProgram path all correct |
| `Shaders/SpatialStripFieldAssist.*` | Per-effect strip field assist | shared | keep | 2026-10-27: width clamp, sample01/sampleKernelSigned delegates correct |
| `Shaders/SpatialVolumeFieldEngine.*` | GPGPU volumeMain + point-eval + media tex + PBO readback | shared | keep | 2026-10-27: role correct; ~1k lines — within limit |
| `Shaders/SpatialStripFieldEngine.*` | Offscreen 1D strip atlas | shared | keep | 2026-10-27: role correct |
| `Shaders/SpatialShaderEngine.*` | 2D spatialMain renderer for Shader Field | engines/shader-field | keep | 2026-10-27: role, compile, FBO, uniforms correct |
| `Shaders/SpatialShaderCatalog.*` | `.fs` disk loader + pattern reader | engines/shader-field | fixed | 2026-10-27: shared `LoadShaderBodyFromPath` + `IsEffectShaderHeaderKey` (single key set for body strip + pattern scan) |
| `Shaders/FieldFullscreenQuad410.*` | Shared VAO/VBO fullscreen quad for all field atlases | shared | keep | 2026-10-27: correct; Destroy always clears CPU handles |
| `Shaders/GlslUniformArray.h` | Float array uniform upload helper | shared | keep | 2026-10-27: fallback location resolution correct |
| `Shaders/SpatialShaderUniforms.h` | Plain struct for Shader Field uniforms | shared | keep | |
| `SpatialLighting/SpatialLightingEngine.*` | Occluder build + segment tests + AO + LED shading | shared | fixed | 2026-10-27: `AppendRoomWallOccluders` left-wall corner typo fixed; duplicate floor→front+back Z quads; dead `eps` removed |
| `SpatialLighting/BlockerGridOccluder.*` | Dense blocker grid + room blocker field | shared | keep | 2026-10-27: DDA ray walk contracts kept |
| `SpatialLighting/OccluderSpatialIndex.*` | Uniform grid AABB spatial index for sub-linear queries | shared | keep | 2026-10-27: Build+CollectSegmentCandidates correct |
| `SpatialLighting/EmitterRelayMirror.*` | Emitter surface splat + bilinear sample + receiver shading | shared | keep | 2026-10-27: SplatSampleDominant, falloff, occlusion path correct |
| `SpatialLighting/EmitterLocalSampling.*` | Combined emitter canvas builder | shared | keep | 2026-10-27: header reviewed; role clear |
| `SpatialLighting/SpatialLightingSceneProvider.*` | Singleton scene state + cached AO shade | shared | keep | 2026-10-27: cache keying, geometry epoch, first-host guard correct |
| `SpatialRoom/SpatialRoomTypes.h` | Room taxonomy enums + capability flags + frame context | shared | keep | 2026-10-27: types clear; exhaustive switch not needed (no switch on these enums in this file) |
| `SpatialRoom/SpatialRoomDefaults.*` | DefaultCapabilitiesForMode (mode→caps mapping) | shared | keep | 2026-10-27: switch has default: break for OriginField; EmissiveRelay + RoomMappedPattern both set CapSkipSampleWarp correctly |
| `SpatialRoom/SpatialRoomFrame.*` | Per-frame overlay pass depth tracker | shared | keep | 2026-10-27: nested depth tracking correct; EndEffectRenderFrame reset is defensive (not a bug) |
| `SpatialSamplers/RoomSampleMapping.*` | Cubemap LED→color lookup from SHM frame | shared | fixed | 2026-10-27: redundant `static` removed from anon-namespace functions |
| `SpatialSamplers/SpatialLayerCore.*` | Compass stratum + vertical layer math | shared | fixed | 2026-10-27: `(void)w0` suppressor removed; restructured to p0/p1/sector_t; trailing blank line removed |
| `SpatialSamplers/SpatialBasisUtils.h` | Orthonormal basis build + ToLocal | shared | keep | 2026-10-27: math correct; inline-only header fine |
| `Game/*` `integrations/*` | Game SHM / telemetry | engines/game | | 2026-10-27: reviewed; wiring, seqlock, SHM paths kept; no changes needed |
| `ui/custom-controller-grid/*` | Custom controller LED grid editor | ui | keep | 2026-10-27: layout math / hit / fit reviewed; tip em-dashes escaped |
| `ui/viewport/*` | Viewport GL math + mesh import | ui | keep | 2026-10-27: Rodrigues fix already landed; MeshImport AABB OK |
| `ui/ActionsPanel.*` | Actions list + pack preview | ui | keep | 2026-10-27: ClearActionPreview on stop/finish; human copy OK |
| `ui/ActionEditorDialog.*` `ui/ActionEditorProps.*` `ui/ActionEditorTimelineBridge.*` | Action Editor shell / props / timeline bridge | ui | fixed | 2026-10-27: `\uXXXX` status/tooltips; live-preview dirty wiring kept |
| `ui/ActionToolBar.*` | Editor toolbox tabs (tools/colors/gradients/curves) | ui | fixed | 2026-10-27: em-dash tooltips → `\u2014` |
| `ui/ActionTimelineWidget.*` `ui/ActionTimelinePaint.*` `ui/ActionTimelineInteract.*` | Timeline paint / interact | ui | fixed | 2026-10-27: minus/degree/ellipsis escapes; stopPreviewRequested kept |
| `ui/ActionEffectFiles.*` | Toolbox `.fx` loader | ui | fixed | 2026-10-27: nested directory_iterator `ec2` |
| `ui/TriggersPanel.*` | Trigger bindings + manual fire | ui | fixed | 2026-10-27: edit/delete clear preview when idle; arrow `\u2192` |
| `ui/EffectLibraryPanel.*` | Effect library bind shell | ui | fixed | 2026-10-27: `bound_` once-guard (match sibling panels) |
| `ui/*` | Plugin UI (forms/widgets/tabs) | ui | pending | remaining: LEDViewport3D*, SpatialTab_*, CustomControllerDialog*, Zones/ObjectCreator, Effect stack, widgets/* |
| Root `SpatialEffect3D.*` etc. | Effect base / types | shared | | Map individually as touched |

## Presets → engine (from volume toolbox multipass)

| Engine | Toolbox tools |
|--------|----------------|
| volume-field | `field_aurora`, `field_burst`, `field_dual_vortex`, `field_electric`, `field_flash`, `field_portal`, `field_ripple`, `field_room_glow`, `field_strata` |
| particle-field | `particle_embers`, `particle_fireflies`, `particle_meteors`, `particle_rain`, `spark_burst` |
| ribbon-field | `laser_sweep`, `neon_ribbon`, `ribbon_helix` |
| scripted volume (FolderVolume eval) | remaining volume `.fx` (confetti, helix, orbit, …) |
