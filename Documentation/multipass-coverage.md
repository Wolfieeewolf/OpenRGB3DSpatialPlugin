# Multipass coverage (plugin)

**Rule:** anything not yet multipassed is **stale**. Presets tracker: [OpenRGB3DSpatialPresets `docs/multipass-coverage.md`](../../OpenRGB3DSpatialPresets/docs/multipass-coverage.md) (sibling repo).

File → engine map (regroup later): [plugin-file-map.md](plugin-file-map.md).

Status: `pending` | `pass` | `keep` | `fixed` | `parked`.

## Animated Mesh / viewport mesh (2026-10-31)

Scoped catch-all on `engines/mesh-anim` + mesh overlay / MeshUnlit / bloom after GPU-only + Effect-tint work.

| Finding | Action |
|---------|--------|
| Fake (0,0) UVs → wrong texture corner | Fixed — omit UVs when Assimp has none |
| Silent triangle-cap truncate | Fail closed with error |
| Skin/Morph “yes” lied about support | UI now says **ignored** |
| Dead `Evaluate` / `EvaluateParts` / `BakePartSoups` | Deleted |
| Part emissive ignored on GPU mesh | Wired `emis_scale` → `u_bright` |
| `anim_speed` LoadSettings legacy shim | Removed |
| Unused `MeshAnimGpuCache::bright_scale` | Removed |
| Metallic/roughness unused | Loaded + PBR-lite; MR texture (G/B) + normal maps in MeshUnlit |
| Soft shadow for mesh overlay | Fixed — directional depth-to-color map + 3×3 PCF (not cascaded) |
| Full path tracing / cascaded shadows | **Hard ceiling** — OpenGL 4.1 plugin viewport; not in scope |
| LED Soft glow still per-point CPU | Fixed — `u_media` sample splat + `evaluateAtPoints`; CPU nearest only if GL fails |
| GPU node TRS for LED samples | Practical path: CPU pose → packed media; LED eval on GPU |
| Sketchfab neon / BLEND glass (placid_flux, technotron) | Fixed — KHR_texture_transform per-slot, emissiveStrength, specularFactor, glTF metal default 1, MeshUnlit for factor-only mats, AO |
| Soft glow / mesh still white after Assimp tip | Fixed — Assimp UVTRANSFORM order in MeshUnlit + Soft glow CPU; GL texture V-flip; empty sample → black not white |
| stranger_star MASK dark blob | Fixed — ignore Assimp remapped UVTRANSFORM; apply raw glTF KHR_texture_transform (JSON) with V-flip-aware xform; Emissive 0 no longer crushes MeshUnlit |
| Viewport goes white on any mesh import | Fixed — mesh soft-shadow FBO cleared with white ClearColor and never restored; paintGL resets black + shadow pass restores |
| Sculpture OK but colours/textures washed | Fixed — MeshUnlit prioritises albedo/emissive hue over PBR env wash; milder bloom; raw glTF emis factor/strength from JSON; Assimp BGRA→RGBA |
| Speckled white spiral (MASK look) | Fixed — sample with glTF UVs on unflipped upload; MASK keeps emissive-lit fragments |
| Sketchfab-like look ceiling | Fixed — MeshUnlit HDR emis into RGBA16F FBO; bloom then tonemap; cheap hemisphere IBL + Blinn spec |
| Soft glow LEDs ≠ viewport sculpture | Fixed — SampleMaterialViewportLook (MeshUnlit-ish) bakes Soft glow / surface LED colours; HDR emis no longer clamped to 1 before mix |
| Soft glow / bloom white soup | Fixed — peak tonemap (viewport + LED bake); Soft glow no longer multiplies baked look by emis then clamps; bake at neutral exposure |

Status: **fixed** (Sketchfab-ish MeshUnlit on GL 4.1). Path tracing / full cubemap IBL = later.

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
