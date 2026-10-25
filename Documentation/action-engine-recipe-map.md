# Action engine recipe map (Phase 0)

Ideas-only map from reference looks → our engines. **No third-party code is copied**; we implement clean-room LED sampling in room space (**Roomscape** — place devices anywhere, engines sample the room).

| Look / idea (inspiration) | Engine | First toolbox tool | Priority |
|---------------------------|--------|--------------------|----------|
| Soft radial ripple / expanding shell (cube + volume `.fs`) | Volume field | `field_ripple` (shader: `ripple_ring`) | P0 — Phase 1 gate |
| Strata / wash / fog / portal wash (Godot-ish fields, effect-shaders) | Volume field | `field_strata` (`strata_wash`), `field_burst` (`radial_burst`) | P1 |
| Burst / sparks / rain / fireflies (Effekseer, Godot VFX, cube) | Particle | `spark_burst`, `particle_rain`, `particle_fireflies` | P1 — Phase 2 gate |
| Swarm / flock (neighborhood-grid idea) | Particle (+ spatial hash) | later | P2 |
| Neon tubes / ribbons (TubesCursor / FreeFrontend — look only) | Ribbon | `neon_ribbon` (`arc`), `ribbon_helix` (`helix`) | P2 — Phase 3 gate |
| Dreamy glow dense cloud (Codrops GPGPU idea) | Particle (true per-LED GPU eval) | later | P2 |
| Dense splat / three.js cloud | Volume field data source | optional later | P3 |

## Phase order (gated)

1. **Volume field** — prepare once / true `volumeMain` at each LED + `field_*` builtins + one minimal `.fx`.
2. **Particle** — GPGPU `volumeMain` (positions in shader) at each LED + builtins + `spark_burst` (CPU fallback).
3. **Ribbon / tube** — GPGPU `volumeMain` (curve in shader) at each LED + `neon_ribbon` (CPU fallback).
4. **Toolbox coverage** — thin tools that exercise engines (`field_strata`, `field_burst`, `particle_rain`, `particle_fireflies`, `ribbon_helix`).
5. **Showcase Actions** — first Roomscape packs landed (`roomscape_*` in triggers + one Movie Sync wash). Expand later as needed.

## Clean-room rule

Study vault/web demos for motion and colour language. Write original GLSL/`fx`/C++ for this plugin. Do not paste or vendor Effekseer, Godot, three.js, or demo sources.

## Showcase packs (presets)

| Pack | Where | Engines |
|------|-------|---------|
| `roomscape_field_ripple` | `triggers/effects/roomscape/` | volume field |
| `roomscape_spark_burst` | `triggers/effects/roomscape/` | particles |
| `roomscape_neon_ribbon` | `triggers/effects/roomscape/` | ribbon |
| `roomscape_strata_fireflies` | `triggers/effects/roomscape/` | field + particles |
| `roomscape_helix_rain` | `movie-sync/effects/washes/` | ribbon + particles |

## Deferred / next (later)

- **3D grid placement editor** — place volume clips in the viewport (translate/rotate/scale); keep timeline; switch modes. Separate branch.
- **Swarm / flock** particle mode (spatial hash).
- **More showcase packs** once the first five feel good in real rooms.
- **Ribbon polish** (multi-tube, accent lights) if neon looks need it.
