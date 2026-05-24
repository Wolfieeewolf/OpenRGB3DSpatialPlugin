# 3D viewport and gizmo

## Components

| Class | Role |
|-------|------|
| `LEDViewport3D` | Room grid, controllers, reference points, display planes, picking, orbit/pan |
| `Gizmo3D` | Move / rotate / view-relative transform on selection |
| `CaptureAreaPreviewWidget` | Screen-mirror zone editing (custom paint, inside `CaptureZonesWidget`) |

Side panels (`SceneTransformPanel`, `GridSettingsPanel`, controller cards) are Qt Designer; the viewport is **C++ only**.

## Qt 5.15 production viewport

OpenRGB still uses **Qt 5.15**. The plugin ships a **legacy OpenGL room** and optional **GPU texture labels** (no shader room in release builds).

| Doc | Contents |
|-----|----------|
| [VIEWPORT_QT515.md](VIEWPORT_QT515.md) | What works now, settings JSON, env vars |
| [VIEWPORT_MODERN_GL.md](VIEWPORT_MODERN_GL.md) | Shader room for **Qt 6** later |

**Quick enable GPU labels:** `"Viewport": { "GpuLabels": true }` in plugin settings, or `OPENRGB3D_VIEWPORT_GPU_LABELS=1` for one session.

## HiDPI mouse contract

1. Qt delivers mouse in **logical** pixels (top-left, Y down).
2. After `makeCurrent()`, `GL_VIEWPORT` is often **device** pixels.
3. **`MapQtMouseToGluWindow`** converts once; picking and gizmo use that space.
4. **`Gizmo3D::GenerateRay`** must **not** flip Y again with `viewport[3] - y`.

Breaking this causes wrong picks / gizmo hits while orbit may still work.

## Tab wiring (`OpenRGB3DSpatialTab.cpp`)

| Viewport signal | Tab slot |
|-----------------|----------|
| `ControllerSelected` | `on_viewport_controller_selected` |
| `ControllerPositionChanged` | `on_controller_position_changed` |
| `ControllerRotationChanged` | `on_controller_rotation_changed` |
| `ControllerDeleteRequested` | `on_remove_controller_from_viewport` |
| `ReferencePointSelected` | `on_ref_point_selected` |
| `ReferencePointPositionChanged` | `on_ref_point_position_changed` |
| `DisplayPlanePositionChanged` | `on_display_plane_position_signal` |
| `DisplayPlaneRotationChanged` | `on_display_plane_rotation_signal` |

`SceneTransformPanel` and `GridSettingsPanel` share the same apply helpers. **`CustomControllerPreviewDialog`** embeds a second viewport with identical mouse behavior.

## Related features

- **Display planes** — positioned in viewport; used by **Screen Mirror** capture mapping ([EFFECTS.md](EFFECTS.md) Ambilight).
- **Zones** — logical LED groups; effect stack targets zones per layer.
- **Grid / room** — `GridSettingsPanel` ↔ `viewport` room dimensions, snap, overlay buffer.

## Mouse navigation

| Input | Action |
|-------|--------|
| Right drag | Orbit camera |
| Middle drag | Pan camera |
| Left drag (empty space) | Pan camera target |
| Left click | Pick controller / ref point / display plane |
| Wheel | Zoom (uses `pixelDelta` when the device provides it) |

## Shortcuts

Help text is in **`viewportHelpLabel`** above the 3D view. Shortcuts are registered on the viewport column (`WidgetWithChildrenShortcut`), so they work after you click the 3D view or any control in that column (grid settings, frame button, etc.). Click the viewport once if a key does nothing.

| Key | Action |
|-----|--------|
| W / E / R | Gizmo move / rotate / view-relative (editor-style, not WASD camera) |
| Q | Toggle grid snap |
| F | Frame camera on current selection (target, zoom, 45°/30° view) |
| Home | Reset camera to default orbit |
| Delete / Backspace | Remove selected controller from scene |
| Click gizmo center (orange cube) | Cycle gizmo mode (move → rotate → view-relative) |

**Grid Settings → 3D view overlay:** toggle **room guide labels** (walls/floor/origin); **Reset camera** (same as **Home**); optional **alternate label renderer** (Qt 5.15-safe; see [VIEWPORT_QT515.md](VIEWPORT_QT515.md)); **Frame selection** (same as **F**). On **Qt 6** builds, **Shader-based 3D room** mirrors `OPENRGB3D_VIEWPORT_GPU_SCENE=1` (`Viewport.GpuScene` in settings). Gizmo rotate text always shows while dragging a ring.

**Shader room (Qt 6 only):** Grid Settings checkbox or `OPENRGB3D_VIEWPORT_GPU_SCENE=1`. On Qt 5.15 both are ignored unless `OPENRGB3D_VIEWPORT_GPU_SCENE_FORCE=1` (dev-only, may crash).

**Performance:** When the Spatial tab is hidden, viewport repaints and screen-preview texture uploads are paused (same gate as `UpdateColors()`).

## Shader room (Qt 6)

See [VIEWPORT_MODERN_GL.md](VIEWPORT_MODERN_GL.md). `OPENRGB3D_VIEWPORT_GPU_SCENE=1` is enabled automatically on Qt 6 hosts; stay on legacy room for Qt 5.15 OpenRGB unless using `GPU_SCENE_FORCE` for experiments.

## Performance notes

- Floor grid lines are cached until room/grid extents change.
- LED preview uses batched `glDrawArrays(GL_POINTS)` per controller; `UpdateColors()` is a no-op while the plugin tab is hidden.
- Controller hull faces use alpha only (no depth write) so LEDs stay visible when zoomed in.
- Screen-capture textures upload on the ~33 ms preview timer or effect tick, not inside `paintGL`.
- Texture objects are destroyed when preview is fully off, not every frame.

## HiDPI and framebuffer

`resizeGL` sets `glViewport` to **device pixels** (`logical size × devicePixelRatio`). Mouse events stay in logical Qt coordinates and are scaled in `MapQtMouseToGluWindow` to match `GL_VIEWPORT`. On Qt 6, `DevicePixelRatioChange` triggers a repaint; Qt 5.15 relies on resize/show.

## LED preview point size

LED dots scale slightly with camera distance (`ledPreviewPointSizeGl`, roughly 4–12 px) so they stay visible when zoomed out or in.

## Room guide label placement

Wall/floor/origin labels anchor to face centers (origin at `(0,0,0)`). Overlap detection skips lower-priority labels when the room is small or the view clusters them.

## Forward

- Document recommended room scale vs real-world room size in layout profiles (user doc).
