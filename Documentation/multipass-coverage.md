# Multipass coverage (plugin)

**Rule:** anything not yet multipassed is **stale**. Presets tracker: [OpenRGB3DSpatialPresets `docs/multipass-coverage.md`](../../OpenRGB3DSpatialPresets/docs/multipass-coverage.md) (sibling repo).

File → engine map (regroup later): [plugin-file-map.md](plugin-file-map.md).

Status: `pending` | `pass` | `keep` | `fixed` | `parked`.

## Full multipass (2026-10-30)

Plugin-wide catch-all (openrgb3d-multipass-review). Focus: pass 2b path/HTTP, seams (ClearActionPreview / preview dirty / once clocks), volume residual, CONTRIBUTING hygiene. No rebuild (user builds locally). FolderVolume / AudioInputManager / Reactive still oversized — **split later**, not blocked on pass status.

| Family | Status | Date | Notes |
|--------|--------|------|-------|
| engines/actions | keep | 2026-10-30 | MapPlaybackTime once/forever/while_active OK; ClearActionPreview on stop/finish/editor/triggers; preview_pack_dirty wired via clipEdited/Created/props; GpuAnimMs continuous clock OK. No high-conviction code fix this run. |
| engines/folder-volume | fixed | 2026-10-30 | Header-hint ignore comment deslop; scans stay under PluginSettingsPaths roots; media_path is user-chosen (intentional). **~2.6k — split later.** |
| engines/shader-field | fixed | 2026-10-30 | `FindEffectShaderPath` rejects `/` `\` `..` in id (fail closed). |
| engines/screen-mirror | keep | 2026-10-30 | Prior hygiene still holds; no new fails. |
| engines/reactive | keep | 2026-10-30 | No legacy LoadSettings remap; ring_width_pct is live thickness only. **~1.5k — split later.** |
| engines/game/Minecraft + Game/ | keep | 2026-10-30 | UDP loopback; SHM magic/seq/size validated; no new fails. |
| engines/movie-sync | fixed | 2026-10-30 | HTTP body/header size caps; JSON-safe effect_id ack; catalog `action` rejects `..`/absolute + stays under effects dir. |
| engines/triggers | keep | 2026-10-30 | Prior ec / Title Case fixes hold; ClearActionPreview on idle tick. |
| shared/SpatialLighting + SpatialPatternKernels | keep | 2026-10-30 | Room wall occluders already correct (6 faces); no new fails. |
| Audio/ | keep | 2026-10-30 | **AudioInputManager ~1.4k — split later.** |
| engines/reactive/ReactiveInput* | keep | 2026-10-30 | Session gated by PlaybackUsesReactive. |
| Input/ControllerTilt* | parked | | DualSense tilt on feat branch |
| Shaders/ | keep | 2026-10-30 | Prior catalog header-key alignment holds. |
| SpatialLighting/ SpatialRoom/ SpatialSamplers/ | keep | 2026-10-30 | Re-checked; no new fails. |
| ui/viewport + ScenePropMeshPaths | fixed | 2026-10-30 | Mesh Resolve fail-closed under MeshesRoot; emitGizmoDragCompleted / NotifyControllerTransformChanged brace deslop. Volume absolute-origin path from 2026-10-30 scoped pass still good. |
| ui/custom-controller-grid | keep | 2026-10-30 | Role/wiring skim OK; no high-conviction fix. |
| ui/ residual (shell widgets controllers actions) | keep | 2026-10-30 | Action Editor dirty flag + ClearActionPreview paths OK; no garbage-string / dead-connect finds. |
| ui/ effect-volume boxes (`feat/effect-volume-boxes`) | fixed | 2026-10-30 | Prior scoped pass + this full pass re-check. |
| Documentation/ | fixed | 2026-10-30 | This tracker updated for full pass. |

## Parked (other)

| Item | Reason |
|------|--------|
| DualSense / ControllerTorch | `feat/dualsense-controller-torch` — wait for 9-axis mag |
| File-size splits | FolderVolume ~2.6k, Reactive ~1.5k, AudioInputManager ~1.4k — extract later, not pass blockers |
