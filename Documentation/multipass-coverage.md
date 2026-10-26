# Multipass coverage (plugin)

**Rule:** anything not yet multipassed is **stale**. Presets tracker: [OpenRGB3DSpatialPresets `docs/multipass-coverage.md`](../../OpenRGB3DSpatialPresets/docs/multipass-coverage.md) (sibling repo).

File → engine map (regroup later): [plugin-file-map.md](plugin-file-map.md).

Status: `pending` | `pass` | `keep` | `fixed` | `parked`.

## Engines / surfaces

| Family | Status | Date | Notes |
|--------|--------|------|-------|
| Effects3D/Actions | keep | 2026-10-27 | Ratchet: ClipSpan/ClipSweep/GpuAnimMs + per-clip setParams already wired; volume/particle/ribbon/builtin GPU prepare OK; no second GL pool; exhaustive `(void)` enum switches intentional |
| Effects3D/FolderVolume | keep | 2026-10-27 | Ratchet: header keys include drive/audio_*/shader/media_layout/instances; media UI copy OK; **~2.6k lines — split later** |
| Effects3D/ShaderField | fixed | 2026-10-27 | Dropped local ShaderBody (missed newer header keys); reuse `SpatialShaderCatalog::LoadShaderBodyFromPath`; projection labels use `\u2013` |
| Effects3D/ScreenMirror | fixed | 2026-10-27 | Ratchet: raw C-string tooltips / empty-monitor label → `QStringLiteral`; capture wiring kept |
| Effects3D/Reactive | fixed | 2026-10-27 | Removed legacy `ring_width_pct` LoadSettings remap; wiring only-while-running OK; **~1.5k lines — split later** |
| Effects3D/Games | fixed | 2026-10-27 | Dead `MakeRgb` + `RoomLedToSpatialCore` removed; include path fixed; strip-axis UI gap patched (standalone Hunger/Air/Dura now expose axis controls); RoomAmbilight description updated; per-strip labels corrected |
| Effects3D/MovieSync | fixed | 2026-10-27 | Curly-quote literals in tr() replaced with \u201c/\u201d escapes (CONTRIBUTING hygiene); LOG_WARNING text aligned to UI label Allow photosensitive looks; ec reuse across nested directory_iterator fixed with ec2 |
| Effects3D/Triggers | fixed | 2026-10-27 | LoadCatalog stale ec from is_regular_file(ec) fixed (use cached status); 7 of 16 WindowsTriggerSource::ListEvents display names normalised to Title Case |
| Effects3D/SpatialLighting + SpatialPatternKernels | fixed | 2026-10-27 | `EffectShaderPatterns` stopped early on newer header keys (`drive`, `audio_preset`, `audio_media`, `shader`, `media_layout`, `instances`) — added to recognized set; SpatialPatternKernels.cpp/SpatialPatternPalettes.cpp kept (role, wiring, palette coverage OK) |
| Audio/ | keep | 2026-10-27 | No printf/debug leftovers; **AudioInputManager ~1.4k — split later** |
| Input/Reactive* | keep | 2026-10-27 | Session start/stop gated by PlaybackUsesReactive |
| Input/ControllerTilt* | parked | | DualSense tilt on feat branch |
| Shaders/ | fixed | 2026-10-27 | Shared GL pool/engines reviewed; SpatialFieldAssistBase.h per-track param push OK; SpatialShaderCatalog header-key set aligned with LoadEffectShader |
| SpatialLighting/ SpatialRoom/ SpatialSamplers/ | fixed | 2026-10-27 | `AppendRoomWallOccluders`: left-wall corner typo fixed (`min_z`→`min_y`), duplicate floor replaced with correct front (Z=min_z) + back (Z=max_z) quads, dead `eps` variable removed; SpatialLayerCore.cpp: `(void)w0` suppressor removed (restructured to `p0/p1/sector_t`); RoomSampleMapping.cpp: redundant `static` removed from anonymous-namespace functions; SpatialRoomFrame/Defaults/Types kept |
| Game/ + integrations/ | fixed | 2026-10-27 | Partial: GameTelemetryBridge wiring reviewed (keep); RoomSampleFrameProtocol / ShmWriter / ConfigReader kept; include path in MinecraftGame.cpp corrected to `Game/` prefix |
| ui/ | keep | 2026-10-27 | `custom-controller-grid` + `viewport` reviewed (layout math, Rodrigues rotation comment, MeshImport AABB); no fail edits — keep |
| Documentation/ | fixed | 2026-10-27 | Shader Field contract filled in effects-engines; LoadShaderBodyFromPath noted in shader-conversion; recipe-map deferred list (multipass in flight + DualSense park); content-library / event-maker / SpatialMeasurement kept |

## Parked

| Item | Reason |
|------|--------|
| DualSense / ControllerTorch | `feat/dualsense-controller-torch` — wait for 9-axis mag |
