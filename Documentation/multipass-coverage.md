# Multipass coverage (plugin)

**Rule:** anything not yet multipassed is **stale**. Presets tracker: [OpenRGB3DSpatialPresets `docs/multipass-coverage.md`](../../OpenRGB3DSpatialPresets/docs/multipass-coverage.md) (sibling repo).

File → engine map (regroup later): [plugin-file-map.md](plugin-file-map.md).

Status: `pending` | `pass` | `keep` | `fixed` | `parked`.

## Engines / surfaces

| Family | Status | Date | Notes |
|--------|--------|------|-------|
| Effects3D/Actions | pending | | Ratchet under new gates (clocks already touched Oct 2026) |
| Effects3D/FolderVolume | pending | | Oct multipass — ratchet |
| Effects3D/ShaderField | pending | | Oct multipass — ratchet |
| Effects3D/ScreenMirror | pending | | Oct multipass — ratchet |
| Effects3D/Reactive | fixed | 2026-10-27 | Removed legacy `ring_width_pct` LoadSettings remap; wiring only-while-running OK; **~1.5k lines — split later** |
| Effects3D/Games | fixed | 2026-10-27 | Dead `MakeRgb` + `RoomLedToSpatialCore` removed; include path fixed; strip-axis UI gap patched (standalone Hunger/Air/Dura now expose axis controls); RoomAmbilight description updated; per-strip labels corrected |
| Effects3D/MovieSync | fixed | 2026-10-27 | Curly-quote literals in tr() replaced with \u201c/\u201d escapes (CONTRIBUTING hygiene); LOG_WARNING text aligned to UI label Allow photosensitive looks; ec reuse across nested directory_iterator fixed with ec2 |
| Effects3D/Triggers | fixed | 2026-10-27 | LoadCatalog stale ec from is_regular_file(ec) fixed (use cached status); 7 of 16 WindowsTriggerSource::ListEvents display names normalised to Title Case |
| Effects3D/SpatialLighting + SpatialPatternKernels | pending | | |
| Audio/ | keep | 2026-10-27 | No printf/debug leftovers; **AudioInputManager ~1.4k — split later** |
| Input/Reactive* | keep | 2026-10-27 | Session start/stop gated by PlaybackUsesReactive |
| Input/ControllerTilt* | parked | | DualSense tilt on feat branch |
| Shaders/ | pending | | Shared GL — map to `shared/` |
| SpatialLighting/ SpatialRoom/ SpatialSamplers/ | pending | | Room core |
| Game/ + integrations/ | fixed | 2026-10-27 | Partial: GameTelemetryBridge wiring reviewed (keep); RoomSampleFrameProtocol / ShmWriter / ConfigReader kept; include path in MinecraftGame.cpp corrected to `Game/` prefix |
| ui/ | pending | | forms/widgets fresher; grid/viewport staler |
| Documentation/ | pending | | |

## Parked

| Item | Reason |
|------|--------|
| DualSense / ControllerTorch | `feat/dualsense-controller-torch` — wait for 9-axis mag |
