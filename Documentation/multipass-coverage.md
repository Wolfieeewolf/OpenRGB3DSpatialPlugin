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
| Effects3D/Reactive | pending | | Stale ~Sep 2026 |
| Effects3D/Games | pending | | Stale |
| Effects3D/MovieSync | pending | | |
| Effects3D/Triggers | pending | | |
| Effects3D/SpatialLighting + SpatialPatternKernels | pending | | |
| Audio/ | pending | | Stale |
| Input/ | pending | | DualSense tilt parked on feat branch |
| Shaders/ | pending | | Shared GL — map to `shared/` |
| SpatialLighting/ SpatialRoom/ SpatialSamplers/ | pending | | Room core |
| Game/ + integrations/ | pending | | |
| ui/ | pending | | forms/widgets fresher; grid/viewport staler |
| Documentation/ | pending | | |

## Parked

| Item | Reason |
|------|--------|
| DualSense / ControllerTorch | `feat/dualsense-controller-torch` — wait for 9-axis mag |
