# Mega-Cube Animation Reference

Reference from [MaltWhiskey/Mega-Cube](https://github.com/MaltWhiskey/Mega-Cube) (16×16×16 LED cube, Teensy 4.0). Their animations live in `Software/LED Display/src/space/`. Below is a concise summary of each and how they map to our plugin.

---

## Their animations (from Animation.cpp)

| Name | File | Idea |
|------|------|------|
| **Atoms** | Atoms.h | Electrons around nucleus: 9 points (axis + diagonals), each rotated by its own quaternion (axis from `sin(angle/95)` etc). `radiate5(v, color, distance)` per point. |
| **Sinus** | Sinus.h | **3D wave surface**: `y = sin(phase + sqrt(x'² + z'²))` in a 2D (x,z) grid; point = (x', z', y). Rotate whole surface by quaternion `(phase*10, (1,1,1))`, scale by radius, then `radiate(point, color, 1.0)`. Circular wave that rotates. |
| **Starfield** | Starfield.h | 200 stars at random in (-1,1)³. Move `z += sin(phase)*1.75*dt*r`; wrap at ±1. Rotate by quaternion `(25*phase, (0,1,0))`, `voxel(rotated * body_diagonal, color)`. |
| **Fireworks** | Fireworks.h | **Particle system**: missile from source→target with gravity; on “explode” spawn debris (random velocities), each with hue + brightness decay. `voxel_add` for additive blend; random white sparkles. |
| **Twinkels** | Twinkels.h | Fairy lights / multi lights (modes). |
| **Helix** | Helix.h | **Double helix**: for each y, `(sin(phase+θ), y_norm, cos(phase+θ))*radius`; two strands via `q2.rotate(p0)` and `(q2*q1).rotate(p0)`. Animated reveal (bottom/top/thickness). |
| **Arrows** | Arrows.h | Moving arrows. |
| **Plasma** | Plasma.h | **3D Perlin noise**: fill `noise_map[x][y][z] = noise.noise4(xoff, yoff, zoff, noise_w)*255`. Offsets animated; speeds from 1D noise. Draw with `voxel(x,y,z, Color(palette).scale(noise_map[y][x][z]))`. LavaPalette. |
| **Mario** | Mario.h | Super Mario Run. |
| **Life** | Life.h | **3D Game of Life** (16³). Rules BIRTH/LIVE/DIE per neighbour count; multiple rule sets (4444 gliders, 4555, 5766, etc.). Colors: alive, dieing, birth, dead. |
| **Pong** | Pong.h | Classical Pong game. |
| **Spectrum** | Spectrum.h | WiFi spectrum analyser. |
| **Scroller** | Scroller.h | Circular text scroller. |
| **Accelerometer** | Accelerometer.h | Test accelerometer. |
| **Cube** | Cube.h | **Cube in a cube**: 12 edges of a cube (wireframe), rotate with quaternion (axis cycles Z → (1,1,1) → Y). For each edge, `radiate(v1 - inc*j, color, distance)` along the segment. |

---

## Core helpers (from their code)

- **`voxel(x, y, z, color)`** – set one voxel (cube coords -1..1 or 0..15 depending on API).
- **`voxel_add(pos, color)`** – additive blend at position.
- **`radiate(point, color, distance)`** – draw a soft blob at 3D point (point in -1..1 space, scaled to cube); “distance” controls falloff.
- **`Quaternion(angle, Vector3 axis)`** – rotation; `.rotate(v)`.
- **Noise** – `noise.noise1(t)`, `noise.noise4(x,y,z,w)` (Perlin/simplex style).
- **Particle** – position, velocity, hue, brightness, seconds (decay).

---

## Mapping to our plugin

| Mega-Cube | Our effect(s) | Note |
|-----------|----------------|------|
| **Plasma** | Plasma3D | We have 3D plasma; they use full 3D noise grid + animated offsets. |
| **Sinus** | Wave3D (partial) | Our Wave3D is “color = wave along axis/radius”. Their Sinus is a **surface** (height = sin(phase + r)), then rotated. Different – could add “Sinus surface” mode or new effect. |
| **Helix** | DNAHelix3D | Double helix; we have it. |
| **Cube** | – | Wireframe cube with radiate along edges. We don’t have this; could add “WireframeCube3D” or similar. |
| **Fireworks** | Explosion3D (partial) | We have explosion; they have missile + debris + gravity + additive. Could make Explosion3D more particle-based or add “Fireworks3D”. |
| **Starfield** | – | Moving stars with wrap + rotate. We don’t have starfield; could add. |
| **Life** | – | 3D Game of Life. We don’t have it; would need 3D grid state. |
| **Atoms** | – | Orbiting points with per-point quaternion. We have Comet/Spiral; not quite same. |
| **Layer sweep** | CubeLayer3D | We added Cube Layer (audio-driven). Their cube has no direct “layer” effect in the list; we provide it. |

---

## Ideas we could implement next

1. **Sinus surface** – One surface: `Y = sin(phase + sqrt(X²+Z²))` in local XZ, then apply rotation and scale; sample “above/below” surface or draw surface with falloff. Fits as new “Wave Surface 3D” or mode in Wave3D.
2. **Fireworks3D** – Particle missile + explosion debris (gravity, decay, additive blend); optionally audio-triggered explode.
3. **Starfield3D** – Points moving along Z (or toward camera), wrap, rotate; optional audio for speed.
4. **Wireframe cube** – 12 edges, rotate with quaternion, draw with soft falloff along each edge (like their Cube).
5. **Life3D** – 3D Game of Life with configurable rules and colors (larger effort: stateful grid).

If you want one of these in the plugin, say which and we can implement it next.
