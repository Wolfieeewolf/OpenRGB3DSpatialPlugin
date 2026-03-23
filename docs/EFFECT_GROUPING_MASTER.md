# Master Effect Grouping (Current Status)

This is the current grouping map for 3D effects. It is focused on what is implemented today and what still needs polish.

---

## Implemented Groups

### Wave family
- `Wave`: line/surface modes, with Sinus/Radial/Linear/Pacifica/Gradient variants.

### Fire / ambient family
- `SurfaceAmbient`: Fire/Water/Slime/Lava/Ember/Ocean/Steam styles.
- `Fireworks`: separate explosion-style effect.

### Plasma / noise family
- `Plasma`: includes Noise/CubeFire variants.

### Spiral / swirl family
- `Spiral`: includes Swirl Circles and Hypnotic variants.

### Chase / comet / scanner family
- `TravelingLight`: Comet, Chase, Marquee, ZigZag, KITT Scanner, Wipe, Moving Panes, Crossing Beams, Rotating Beam.

### Rainbow / radial family
- `PulseRing`: includes Radial Rainbow.
- `Wave` and `Spiral` cover rotating/cycling rainbow styles.

### Other implemented effects
- `BouncingBall`
- `Bubbles`
- `BreathingSphere` (includes Global Pulse)
- `Lightning` (Plasma Ball + Sky Lightning styles)
- `Starfield` (includes Twinkle)
- `WireframeCube`

---

## Priority Polish Backlog

1. Improve visual quality and defaults for weaker effects (color balance, speed, falloff, edge softness).
2. Fix effects with rendering artifacts or weak output on sparse layouts/strips.
3. Standardize parameter ranges and labels so similar effects behave consistently.
4. Audit each effect for Save/Load completeness and UI control sync.
5. Add lightweight regression checks for stack/preset/profile loading with mixed effect types.

---

## Notes

- Some legacy plans referred to separate effect classes that are now implemented as modes inside existing effects.
- This file is a living status tracker; keep it focused on current implementation and actionable polish work.
