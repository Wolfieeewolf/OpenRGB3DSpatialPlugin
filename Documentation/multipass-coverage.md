# Multipass coverage (plugin)

**Rule:** anything not yet multipassed is **stale**. Presets tracker: [OpenRGB3DSpatialPresets `docs/multipass-coverage.md`](../../OpenRGB3DSpatialPresets/docs/multipass-coverage.md) (sibling repo).

File → engine map: [plugin-file-map.md](plugin-file-map.md).

Status: `pending` | `pass` | `keep` | `fixed` | `parked`.

## Full multipass (2026-10-31) — full skill gates

Plugin-wide catch-all (openrgb3d-multipass-review), **excluding `OpenRGB/`**. Passes 1–10 in scope: purpose, correctness/dead, **2b security**, fit/wiring (`ClearActionPreview`), seams (`GpuAnimMs` / once→black), UI integrity, structure (natural seam / no explosion), deslop, close-the-loop, skill self-check. No plugin rebuild (user builds locally). Assimp is built by the plugin target into `OUT_PWD/_deps` (CMake required; see README).

### Dump-to-presets audit

**keep / nothing to dump** — no stock `.fs`/`.fx`/packs/controllers in the plugin tree; content already lives only in OpenRGB3DSpatialPresets + AppData. See [content-library.md](content-library.md) § Dump-to-presets audit.

### Hotspot re-check (2b + seams)

| Hotspot | Status | Notes |
|---------|--------|-------|
| Movie Sync HTTP | fixed | `LocalHost` only; header/body caps; invalid Content-Length rejected; catalog size + `..` / absolute / under-root |
| ShaderField path id | fixed | Rejects `/` `\` `..`; shader body/spec ≤512 KiB |
| Mesh Resolve | fixed | Fail-closed under `MeshesRoot`; absolute fallback aligned with PluginSettingsPaths |
| Room sample SHM | fixed | Magic/seq/size validated; apply-boundary rgba byte check |
| Pack / bindings JSON | fixed | `kMaxPackJsonBytes` (2 MiB) on action packs, catalog, trigger bindings |
| Triggers idle clear | keep | `clearPreviewIfIdle` on tick end / stop / uncheck / level-off |
| Action Editor / Actions / MovieSync clear | keep | `ClearActionPreview` on every empty-player path |
| PluginSettingsPaths (ui/shared) | fixed | 2026-10-31 focus: `PathHasDotDot` / `PathStaysUnderRoot` / `IsSafePathLeaf`; stack leaf + controller/action I/O fail-closed |
| Action Editor preview dirty | fixed | Map/Pick controllers now `markPreviewDirtyIfPlaying` |

### Workspace cleanup (this run)

Deleted gitignored / leftover build trees (not source):

- `build/`, `_intermediate_release/`, `release/`, root `Makefile`, `.qmake.stash`, generated `OpenRGB3DSpatialPlugin.json`
- `dependencies/assimp-build*`, `dependencies/assimp-install*`, `dependencies/cmake*`, `cmake-win64.zip`
- `.qtcreator/`, `.wiki-seed/`
- `integrations/minecraft/build`, `.gradle`

Kept: `dependencies/assimp` submodule source only.

Follow-up from [Dead code orphan survey](f4596327-7008-489e-9f7e-1233f5211fa3): stripped five unused inline helpers; listed six live action headers in `.pro`; realigned wiki script / effects-engines rule / path docs.

### Families

| Family | Status | Date | Notes |
|--------|--------|------|-------|
| engines/actions | fixed | 2026-10-31 | Pack JSON 2 MiB cap; removed silent v4→v5 disk rewrite (in-memory defaults remain). Clocks / GpuAnimMs / once→black hold. ActionScript ~1.3k — no crumb-split. |
| engines/folder-volume | fixed | 2026-10-31 | Spec file size cap; stripped narrating audio-param comments. |
| engines/audio | keep | 2026-10-31 | Prior split hold; no new fail this pass. |
| engines/reactive | keep | 2026-10-31 | Prior split hold; no new fail this pass. |
| engines/shader-field | fixed | 2026-10-31 | Shader body/spec size caps; dropped dead `shader_path` save key. Path id fail-closed holds. |
| engines/screen-mirror | fixed | 2026-10-31 | Auto hysteresis; SDR DXGI; Y-flip; 1080p GPU scale→CPU LED. **Follow-up:** dropped Reach-origin option (always plane centre); hid Spatial Anchor + Room output for Ambilight; `needs_3d_origin=false`; UniqueConnection on layout refresh; zone add capped; skip origin stomp when `!needs_3d_origin`. |
| engines/volume\|particle\|ribbon-field | keep | 2026-10-31 | Shared-seq setParams + GpuAnimMs hold; leave named buckets. |
| engines/game | fixed | 2026-10-31 | SHM apply rejects rgba size ≠ grid; Minecraft one-effect stubs ignored (security-only). |
| engines/movie-sync | fixed | 2026-10-31 | Catalog size cap; Content-Length parse fail-closed. LocalHost + path escape hold. |
| engines/triggers | fixed | 2026-10-31 | Bindings JSON size cap; ClearActionPreview idle path OK. |
| shared/* | fixed | 2026-10-31 | Patterns kernel load fail-closed under `PatternsDir`; prior dead-defaults hold. |
| ui/shell Tab_* | fixed | 2026-10-31 | PluginSettingsPaths + stack/controller path fail-closed; prior crumb merges hold. |
| ui/widgets | keep | 2026-10-31 | `EffectCustomHost` / Effect*Row — no crumb merge this pass. |
| ui/viewport, controllers, actions, … | fixed | 2026-10-31 | Preview dirty + action save/import under-root; LEDViewport named shards left alone. |
| Object Creator / scene prop parity | fixed | 2026-10-31 | Scene prop Object Creator copy + Delete; Scene object **Edit scene prop…**; shared DisplayPlaneCaptureUi copy (no Windows-only strings). |
| Documentation/ | fixed | 2026-10-31 | Tracker + file-map + recipe-map + CONTRIBUTING tab note. |
| scripts/, debian\|fedora\|flatpak | keep | 2026-10-31 | Build helpers + distro packaging. `release/` / `.wiki-seed/` are gitignored local/build only — not tracked packaging. |
| .cursor/skills multipass | fixed | 2026-10-31 | Natural-seam / no-explosion + pass-2b **JSON/text size ceilings** row; synced to user store. |
| **UI accessibility (spin-over-slider + three-ways)** | fixed | 2026-11-01 | Pass 5 gates: `EffectSliderRow` / `EffectDoubleSliderRow` (+ promote helpers) for int and double/mm chrome; SceneTransform / Action Editor / grid / spacing / display plane / scene prop / controller cards on the shared rows; `PluginUiWireListDestructiveDelete` on stack/zones/triggers/Object Creator; ActionGradientBar + viewport selection context menus. EQ vertical bands stay exempt. `OpenRGB/` excluded. **Presets repo:** no Qt UI — content half of pass 5 tracked in sibling [OpenRGB3DSpatialPresets `docs/multipass-coverage.md`](../../OpenRGB3DSpatialPresets/docs/multipass-coverage.md) § UI accessibility split. |
| OpenRGB/ | n/a | | **Excluded** from this pass. |

### UI accessibility multipass (2026-10-31) — family detail

| Family | Status | Notes |
|--------|--------|-------|
| Effect Motion / Geometry / Output | fixed | Runtime `PromoteSliderLabelToEffectRow`; load sync via `SyncEffectSliderValue` |
| StratumBand / StripKernel / Grid overlay | fixed | `PromoteSliderLabelInGrid` |
| SceneTransform | fixed | `EffectDoubleSliderRow::adoptExistingSpinAndSlider` (unlink; `PositionAxisDragController` / rotation wire keep sync) |
| CaptureZones wave + audience bias | fixed | Wave rows + `ScreenMirrorCapturePanel` audience bias → `EffectSliderRow` |
| Lists (stack, zones, triggers, Object Creator) | fixed | `PluginUiWireListDestructiveDelete` (button + Del/Backspace + `QMenu`) |
| ActionGradientBar | fixed | Context menu + Del/Backspace; right-click no longer instant-deletes |
| LEDViewport3D selection | fixed | Context menu: Delete controller / Frame + W/E/R/Y/Q hints; Del/Backspace + F |
| EQ bands (`AudioEqBandColumn`) | keep | Vertical layout; accessible names/tooltips only — not `EffectSliderRow` |
| Action Editor internals | fixed | Int/double attach helpers → `EffectSliderRow` / `EffectDoubleSliderRow` (`adoptExistingSpin`) |
| Grid / spacing / display plane / scene prop / cards | fixed | Double mm fields → `EffectDoubleSliderRow` via `Stack`/`Make` promote helpers |

**Grep gates (exclude `OpenRGB/`):** `.ui` forms may still declare legacy `QSlider`+value `QLabel` when ctor promotes to `EffectSliderRow`; bare live pairs fail except EQ. `Qt::RightButton` must open a `QMenu` (or start orbit/drag), never erase immediately.

**skill:** applied — pass 2b now lists **JSON / text size ceilings** (packs, catalog, bindings, specs, `.fs`, settings blobs; fail closed; reuse shared constants). Plugin + user-store skills updated.

Engines family detail: [engines multipass](afe3e97e-e94b-4248-9f57-7751206541fb).
Shared/ui family detail: [shared/ui multipass](2a15319a-38a8-456d-aff2-2423dbeca070) — **skill: keep**.

### Prior full pass (2026-10-30)

Security/seams catch-all still stands for path/HTTP, ClearActionPreview, volume residual. Superseded only where the 2026-10-31 row says fixed.

## Parked (other)

| Item | Reason |
|------|--------|
| DualSense / ControllerTorch | `feat/dualsense-controller-torch` — wait for 9-axis mag |
| Sketchfab Soft glow / MeshUnlit | Stashed off main; park until a different approach |
| Claude judgment multipass (presets) | ~2026-11-23 usage reset |
| QRhi host spike | Explicit unpark only |
