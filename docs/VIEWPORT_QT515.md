# Viewport on Qt 5.15 (production plan)

OpenRGB ships **Qt 5.15** today. The Spatial plugin viewport is optimized for that stack until the host moves to Qt 6.

## What ships

| Layer | Technology | Notes |
|-------|------------|--------|
| **3D room** | Legacy OpenGL 2.1 (`glBegin`, fixed-function) | Stable; default for all users |
| **Axis / HUD labels** | Legacy `QPainter` on matrices **or** optional GPU texture overlay | GPU labels avoid painting on the GL widget surface |
| **Picking / gizmo** | Same matrices as legacy scene | HiDPI contract in [VIEWPORT.md](VIEWPORT.md) |
| **Shader room** | Disabled | `OPENRGB3D_VIEWPORT_GPU_SCENE=1` is ignored; see below |

## Optional GPU labels (Qt 5.15-safe)

GPU labels draw the **same text** as legacy, but:

1. Text is painted to an offscreen `QImage` (never `QPainter` on `QOpenGLWidget`).
2. A small shader pass composites that image after the legacy room draw.

### Enable without environment variables

In `OpenRGB.json` under the plugin settings key (`3DSpatialPlugin` / `plugins/settings/OpenRGB3DSpatialPlugin/`), set:

```json
"Viewport": {
  "GpuLabels": true
}
```

The tab saves this when you change settings (camera, grid snap, room grid, etc.) after `GpuLabels` has been set once.

### Enable for one session (PowerShell)

```powershell
$env:OPENRGB3D_VIEWPORT_GPU_LABELS = "1"
```

Environment variables override nothing else; they OR with the JSON flag.

## What waits for Qt 6

| Feature | Env / code | Status |
|---------|------------|--------|
| Shader/VBO room | `OPENRGB3D_VIEWPORT_GPU_SCENE` | Ignored on Qt 5.15 OpenRGB |
| Developer retry | `OPENRGB3D_VIEWPORT_GPU_SCENE_FORCE=1` | May crash (`Qt5Gui.dll` compositor) |

Code for shader scene, FBO, and `ui/viewport/*` stays in the tree for CI and future Qt 6 enablement. See [VIEWPORT_MODERN_GL.md](VIEWPORT_MODERN_GL.md).

## Do not use (on Qt 5.15 OpenRGB)

- `OPENRGB3D_VIEWPORT_GPU_SCENE=1` — no effect except a console warning; still uses legacy room.
- `OPENRGB3D_VIEWPORT_GPU_SCENE` + `GPU_LABELS` together expecting a shader room — you get **legacy room + GPU labels** only.

## When OpenRGB upgrades to Qt 6

1. Re-test with `OPENRGB3D_VIEWPORT_GPU_SCENE=1` (no `_FORCE`).
2. If stable, enable scene by default and keep `OPENRGB3D_VIEWPORT_LEGACY=1` escape hatch.
3. Remove duplicate legacy draw paths once parity is confirmed.
