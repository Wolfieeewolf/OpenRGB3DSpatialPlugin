# Master Effect Grouping (Current Status)

## Implemented Groups

### Wave family
- `Wave`: line/surface modes, with Sinus/Radial/Linear/Ocean drift/Gradient variants.

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

1. Visual quality / defaults (color, speed, falloff).
2. Artifacts on sparse layouts.
3. Consistent parameter ranges and labels.
4. Save/Load and UI sync audit.
5. Regression checks for stacks/presets.
