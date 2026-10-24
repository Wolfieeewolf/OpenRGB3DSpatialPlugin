# Action engine recipe map (Phase 0)

Ideas-only map from reference looks → our engines. **No third-party code is copied**; we implement clean-room LED sampling in room space (Atmos-for-LEDs).

| Look / idea (inspiration) | Engine | First toolbox tool | Priority |
|---------------------------|--------|--------------------|----------|
| Soft radial ripple / expanding shell (cube + volume `.fs`) | Volume field | `field_ripple` (shader: `ripple_ring`) | P0 — Phase 1 gate |
| Strata / wash / fog / portal wash (Godot-ish fields, effect-shaders) | Volume field | later `field_strata` etc. | P1 |
| Burst / sparks / rain / fireflies (Effekseer, Godot VFX, cube) | Particle | after Phase 1 gate | P1 |
| Swarm / flock (neighborhood-grid idea) | Particle (+ spatial hash) | later | P2 |
| Neon tubes / ribbons (TubesCursor / FreeFrontend — look only) | Ribbon | after particles gate | P2 |
| Dreamy glow dense cloud (Codrops GPGPU idea) | Particle (CPU first; GPGPU later) | later | P2 |
| Dense splat / three.js cloud | Volume field data source | optional later | P3 |

## Phase order (gated)

1. **Volume field** — prepare once / sample per LED + `field_*` builtins + one minimal `.fx`.
2. **Particle** — CPU density sample + builtins.
3. **Ribbon / tube** — curve falloff + colour knobs.
4. **Toolbox coverage** — thin tools that exercise engines.
5. **Showcase Actions** — a few room-scale looks only after tools are proven.

## Clean-room rule

Study vault/web demos for motion and colour language. Write original GLSL/`fx`/C++ for this plugin. Do not paste or vendor Effekseer, Godot, three.js, or demo sources.
