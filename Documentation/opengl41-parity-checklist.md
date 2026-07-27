# OpenGL 4.1 migration — parity checklist

Run on the `feat/opengl-41-viewport` branch (Windows Qt 6.8; Linux/macOS when available).

**Status (2026-07-27):** Phases 0–7 implementation complete on Windows. Core surface, MeshBatch/GLSL 410 viewport path, rotation/wipe parity, and gizmo screen scale verified in-session. DPR pick/HUD math audited (`qRound` FBO size, `DevicePixelRatioChange` → `resizeGL`). Thermonuclear tidy applied (dead APIs, MeshGeometry extract, Draw/Input/lifecycle files under 1k lines). Remaining: optional physical 125%/150% + Linux/macOS smoke; Phase 8 ShaderField; optional gizmo interleaved mesh build.

## Context

- [x] App starts; Spatial tab opens without GL context errors
- [x] Log shows requested **4.1 Core** and a real `GL_VERSION` / `GL_RENDERER` line
- [x] Custom controller preview dialog still opens and paints (hosts `LEDViewport3D`)

## Visual

- [x] Floor grid, axes + labels, room box
- [x] Controllers + LED colors/sizes; hidden-by-virtual still hides
- [x] Display planes + screen preview / calibration pattern
- [x] Room grid overlay, light blockers, user figure, reference points
- [x] Gizmo visible in move / rotate / freeroam

## Input

- [x] Orbit / pan / zoom
- [x] Room turntable
- [x] Click-select controller / ref / plane / room volume
- [x] Gizmo drag move / rotate / freeroam (+ shift snap)
- [x] Keyboard gizmo mode cycle

## DPR

- [x] 100% scaling: mouse pick and HUD labels align
- [x] 125%+ mapping audited: pick uses `MapQtMouseToGlWindow` (logical→framebuffer via `qRound` DPR), HUD uses `ProjectWorldToScreen` + `GlWindowPointToQtLogical`; `DevicePixelRatioChange` calls `resizeGL` then `update`. **Physical 125%/150% smoke still recommended once.**

## Phase 2 — CPU matrices

Paint/pick must not call `gluPerspective` / `gluLookAt` / `gluProject` / `gluUnProject`.
CPU `ViewportFrame` is the source of truth (shaders upload MVP; no fixed-function matrix stack).

- [x] Pan uses **view** matrices (`pick_view_modelview_`)
- [x] Rays / gizmo / labels use **scene** matrices (`pick_scene_modelview_`, includes room turntable)

## Phase 3 — QPainter HUD

- [x] One HUD pass after all 3D (room guides, rotate readout, gizmo toast)
- [x] No HUD flicker/corruption after hide/show; `NoPartialUpdate` still stable
- [x] Labels align at 100% DPR *(125%+ under DPR above)*

## Phase 4 — shader scaffold

- [x] Log shows `unlit_color` / `unlit_point` / `textured_unlit` linked (GLSL 410)
- [x] Production paint path uses MeshBatch + those programs (Phases 5–6 landed)

## Phase 5a — grid + room selection

- [x] Floor grid + border via MeshBatch / unlit_color
- [x] Room volume selection outline + floor tint when selected

## Phase 5b — axes + room boundary

- [x] RGB axes + arrowheads via MeshBatch
- [x] Manual room boundary box when enabled

## Phase 5c — controllers / LEDs

- [x] Controller AABB faces translucent with LEDs visible when zoomed (depth-mask off on faces)
- [x] LED points size scales with camera distance
- [x] Selection highlight edges / primary vs multi-select colors
- [x] Green/red orientation sphere on controller top

## Phase 5d — blockers / user / refs

- [x] Light-blocker cells translucent (depth-mask off)
- [x] User figure face + selection box
- [x] Non-user reference spheres + equator ring + selection box

## Phase 5e — planes + overlay

- [x] Display plane fill / calibration / screen-preview texture
- [x] Plane border selection thickness/color
- [x] Room grid overlay points update with effect colors

## Phase 6 — gizmo

- [x] Move / rotate / freeroam gizmo visible when selection exists
- [x] Hover highlight + drag; rotate arc while dragging
- [x] Screen-stable size (view-space depth); freeroam tip not oversized
- [x] No lighting/blend leak into HUD after gizmo

## Phase 7 — Core 4.1

- [x] Requested surface is **4.1 Core**; log profile string is Core
- [x] No `ViewportGLIncludes` / GLU link; no `glBegin` / matrix-stack / `GL_LIGHTING` in viewport paint
- [x] Draw vs effect rotation match (`ViewportMath::RotationAxis` = `ComputeRotationMatrix`); room wipe ↔ controllers
- [x] Controller rot X **+90** convention restored (no −90 workaround)
- [ ] Linux / macOS smoke *(when available)*

## Post-Core fixes (this branch)

- [x] Wipe depth mismatch vs room grid (draw used transposed Rodrigues sin terms)
- [x] Gizmo oversized / wrong zoom feel (arm fraction + view-space depth + freeroam tip scale)

## Phase 8 — deferred (not blocking this branch merge)

See notes below. Track separately after Core viewport ships.

### Effect Pack editor

- Pack editor itself is Widgets/timeline (no dedicated GL viewport).
- Preview already drives `LEDViewport3D` / device colors via `ApplyPackFrame` — **no Phase 8 GL work required** unless we add a pack-only GL preview surface later.

### ShaderField / `SpatialShaderEngine`

Offscreen GPU field presets still use a **legacy GLSL 110** path (`attribute` / `gl_FragColor`, `#version 110` in `Shaders/SpatialShaderEngine.cpp`). That is separate from the room viewport’s GLSL 410 programs.

Likely work:

1. Request a **compat or explicit** offscreen format so ShaderField does not inherit Core-only defaults and fail to compile 110 — **or**
2. Bump engine + presets to **GLSL 410 core** (`in`/`out`, `fragColor`, VAO/VBO fullscreen triangle) matching the viewport stack.
3. Retest ShaderField presets (plasma / waves / ember / spectrum) on Windows Core builds; then Linux/macOS.
4. Confirm PBO readback still works under Core / `QOpenGLExtraFunctions`.

### Optional later

- Qt RHI / Metal path (not in scope; see architecture notes in chat).
- Share more helpers between viewport `GlProgram` and `SpatialShaderEngine`.
