# Viewport modern OpenGL (future / Qt 6)

Shader/VBO room code lives in the repo for CI and the **Qt 6** cutover. It is **not** the production path on **Qt 5.15 OpenRGB**.

**Production viewport policy:** [VIEWPORT_QT515.md](VIEWPORT_QT515.md)

## Qt 5.15 summary

- **Legacy room** — always used in release OpenRGB 5.15 builds.
- **GPU labels** — optional; legacy room + texture HUD ([VIEWPORT_QT515.md](VIEWPORT_QT515.md)).
- **Shader room** — disabled unless `OPENRGB3D_VIEWPORT_GPU_SCENE_FORCE=1` (developer only; compositor crash `0x2ebb10` on many hosts).

## Environment variables (reference)

| Variable | Qt 5.15 OpenRGB |
|----------|-----------------|
| `OPENRGB3D_VIEWPORT_GPU_LABELS=1` | Supported with legacy room |
| `OPENRGB3D_VIEWPORT_GPU_SCENE=1` | Ignored (warning logged) |
| `OPENRGB3D_VIEWPORT_GPU_SCENE_FORCE=1` | Experimental shader scene |

## Qt 6 checklist (when host is ready)

1. Build plugin against Qt 6 OpenRGB.
2. Remove or relax `viewportGpuSceneForceEnvEnabled()` gate in `LEDViewport3D_Gpu.cpp`.
3. Test `GPU_SCENE=1` then `GPU_SCENE` + `GPU_LABELS`.
4. Confirm FBO single-frame path in `paintGlGpu()`.
5. Default to shader scene with `OPENRGB3D_VIEWPORT_LEGACY=1` fallback.

## Architecture (shader path — for Qt 6)

```text
paintGlGpu():
  beginGpuSceneFramebuffer()
    drawViewportSceneGpu()
    renderGizmoGpu()
    paintViewportLabelsInActiveGpuFrame()
  endGpuSceneFramebuffer()
  finalizePaintGlForQtCompositor()
```

## Key files

| File | Role |
|------|------|
| `LEDViewport3D.cpp` | `paintGL` → legacy or `paintGlGpu` |
| `LEDViewport3D_Gpu.cpp` | Env gates, labels-after-scene |
| `LEDViewport3D_SceneGpu.cpp` | Shader meshes |
| `ViewportRenderer.cpp` | Programs, FBO, overlays |
