# Multipass coverage (plugin)

**Rule:** anything not yet multipassed is **stale**. Presets tracker: [OpenRGB3DSpatialPresets `docs/multipass-coverage.md`](../../OpenRGB3DSpatialPresets/docs/multipass-coverage.md) (sibling repo).

File → engine map (regroup later): [plugin-file-map.md](plugin-file-map.md).

Status: `pending` | `pass` | `keep` | `fixed` | `parked`.

## Parked for Claude reset (~2026-11-23)

Other-models / Claude usage exhausted 2026-10-27. **Do not resume these with a weaker model** — judgment passes (ratchet, grid/viewport, residual UI). Resume order when Claude is available again:

1. **Effects3D/Actions** — full ratchet (clocks/setParams already from earlier Oct; agent skim only after token stop)
2. **Effects3D/FolderVolume** — full ratchet (~2.6k; split later after pass)
3. **ui/custom-controller-grid + ui/viewport** — real multipass (agent stopped; local “keep” was not a finish)
4. **ui residual** — LEDViewport3D*, OpenRGB3DSpatialTab_*, CustomControllerDialog*, Zones/ObjectCreator, Effect stack, `ui/widgets/*`
5. Optional: Game/integrations deep pass (was partial); ShaderField/ScreenMirror full ratchet (hygiene-only pickup already landed)

Skill: `.cursor/skills/openrgb3d-multipass-review/SKILL.md`. Tracker was wrong once — verify with commits, not row status alone.

## Engines / surfaces

| Family | Status | Date | Notes |
|--------|--------|------|-------|
| Effects3D/Actions | parked | 2026-10-27 | Resume ~2026-11-23 — ratchet incomplete (token stop) |
| Effects3D/FolderVolume | parked | 2026-10-27 | Resume ~2026-11-23 — ratchet incomplete (token stop) |
| Effects3D/ShaderField | fixed | 2026-10-27 | Picked up locally after agent stop: shared `LoadShaderBodyFromPath`; `\u2013` labels (not a full agent ratchet) |
| Effects3D/ScreenMirror | fixed | 2026-10-27 | Picked up locally after agent stop: `QStringLiteral` tooltips/empty label (hygiene; not full agent ratchet) |
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
| Game/ + integrations/ | parked | 2026-10-27 | Partial only — optional deep pass on Claude resume list |
| ui/ | parked | 2026-10-27 | Action stack done. Grid/viewport + residual UI on Claude resume list above |
| Documentation/ | fixed | 2026-10-27 | Shader Field contract filled; recipe-map / coverage park notes |

## Parked (other)

| Item | Reason |
|------|--------|
| DualSense / ControllerTorch | `feat/dualsense-controller-torch` — wait for 9-axis mag |
| Incomplete multipasses | See **Parked for Claude reset** — resume ~2026-11-23 |
