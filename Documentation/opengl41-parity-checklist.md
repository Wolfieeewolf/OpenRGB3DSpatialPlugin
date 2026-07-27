# OpenGL 4.1 migration — parity checklist

Run after each phase on the `feat/opengl-41-viewport` branch (Windows Qt 6.8; Linux/macOS when available).

## Context

- [ ] App starts; Spatial tab opens without GL context errors
- [ ] Log shows requested **4.1 Compatibility** and a real `GL_VERSION` / `GL_RENDERER` line
- [ ] Custom controller preview dialog still opens and paints

## Visual

- [ ] Floor grid, axes + labels, room box
- [ ] Controllers + LED colors/sizes; hidden-by-virtual still hides
- [ ] Display planes + screen preview / calibration pattern
- [ ] Room grid overlay, light blockers, user figure, reference points
- [ ] Gizmo visible in move / rotate / freeroam

## Input

- [ ] Orbit / pan / zoom
- [ ] Room turntable
- [ ] Click-select controller / ref / plane / room volume
- [ ] Gizmo drag move / rotate / freeroam (+ shift snap)
- [ ] Keyboard gizmo mode cycle

## DPR

- [ ] 100% and 125%+ scaling: mouse pick and HUD labels still align

## Non-goals this branch (until Phase 8)

- Effect Pack editor unchanged
- ShaderField / SpatialShaderEngine GLSL bump deferred
