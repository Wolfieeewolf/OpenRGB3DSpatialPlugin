# Action engine recipe map (Phase 0)

Ideas-only map from reference looks → our engines. **No third-party code is copied**; we implement clean-room LED sampling in room space (**Roomscape** — place devices anywhere, engines sample the room).

| Look / idea (inspiration) | Engine | First toolbox tool | Priority |
|---------------------------|--------|--------------------|----------|
| Soft radial ripple / expanding shell (cube + volume `.fs`) | Volume field | `field_ripple` (shader: `ripple_ring`) | P0 — Phase 1 gate |
| Full-room Atmos wash (every LED lit) | Volume field | `field_room_glow` (`room_glow`), `field_strata` | P1 |
| Title / hit flash (Godot `flash_white`) | Volume field | `field_flash` (`title_flash`) | P1 |
| Burst / sparks (Godot energy_burst one-shot) | Particle | `spark_burst` | P1 — Phase 2 gate |
| Rain / fireflies (Godot rain_drops streaks) | Particle | `particle_rain`, `particle_fireflies` | P1 |
| Swarm / flock (neighborhood-grid idea) | Particle (+ spatial hash) | later | P2 |
| Neon tubes / ribbons (TubesCursor / FreeFrontend — look only) | Ribbon | `neon_ribbon` (`arc`), `ribbon_helix` (`helix`) | P2 — Phase 3 gate |
| Laser / plane sweep across room | Ribbon | `laser_sweep` (`ribbons: sweep`) | P2 |
| Dreamy glow dense cloud (Codrops GPGPU idea) | Particle (true per-LED GPU eval) | later | P2 |
| Dense splat / three.js cloud | Volume field data source | optional later | P3 |

## Phase order (gated)

1. **Volume field** — prepare once / true `volumeMain` at each LED + `field_*` builtins + one minimal `.fx` (GL required).
2. **Particle** — GPGPU `volumeMain` (positions in shader) at each LED + builtins + `spark_burst` (GL required).
3. **Ribbon / tube** — GPGPU `volumeMain` (curve in shader) at each LED + `neon_ribbon` (GL required).
4. **Toolbox coverage** — thin tools that exercise engines (`field_strata`, `field_burst`, `particle_rain`, `particle_fireflies`, `ribbon_helix`).
5. **Showcase Actions** — first Roomscape packs landed (`roomscape_*` in triggers + one Movie Sync wash). Expand later as needed.

## Playback seams (once vs loop)

- **`loop: once`** — drive the look with span `progress` (0→1), fade density to **black** before the end, then the player clears LEDs. No recycle/`fract` on progress.
- **`loop: forever` / `while_active`** — animate with continuous **`u_time`** only. Do not key motion off wrapping `progress` or wrapped `local_ms` (that flashes/jumps at the pack seam). Rain, helix, fireflies, aurora, portal, etc. already follow this.
- Engines feed `u_time` from continuous elapsed time when playing; GPU one-shot params use non-wrapping span progress.

## Multi-pass look brief (treat like image gen)

Author looks in **three passes**. First pass = readable silhouette. Second = motion + colour character. Third = sparse-LED survival + seam polish.

| Pass | Goal |
|------|------|
| 1 | Something lights the room with clear shape (not mush) |
| 2 | Hot heads / cool tails, gravity or wind, asymmetric timing |
| 3 | Fat kernels, floor/ceiling catch, seamless `u_time`, once→black |

**Positive prompts (keep):** bright cores, streak kernels, unique per-particle phase/speed, Lissajous drift, dive directions, floor splash, ring beads, filament accents, additive glow.

**Negative prompts (kill):** thin 1px misses, synced blink, orange wash, progress-wrap flash, linger after once, all meteors same angle, flat disc stamps, rigid spring helix.

| Mode | Engine | Notes |
|------|--------|-------|
| `burst` | particles | Gravity arc + velocity ellipsoid + white→amber→ember |
| `rain` | particles | Wind shear, tip glow, floor splash, per-drop speed |
| `fireflies` | particles | Lissajous + asymmetric blink + core/halo |
| `meteors` | particles | Unique dive dirs, trail beads, white→amber |
| `embers` | particles | Slow rise, rare pops, ambient wow |
| aurora / portal | volume field | Curtain filaments / ring beads (toolbox `.fs`) |

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
| `roomscape_aurora` | triggers + movie-sync washes | `field_aurora` |
| `roomscape_portal` | triggers + movie-sync washes | `field_portal` (+ electric) |
| `roomscape_meteor_storm` | triggers + movie-sync explosions | `field_dual_vortex` + `particle_meteors` |
| `roomscape_embers` | triggers + movie-sync washes | `field_strata` + `particle_embers` |

## Deferred / next (later)

- **3D grid placement editor** — place volume clips in the viewport (translate/rotate/scale); keep timeline; switch modes. Separate branch.
- **Swarm / flock** particle mode (spatial hash).
- **More showcase packs** once the first five feel good in real rooms.
- **Ribbon polish** (multi-tube, accent lights) if neon looks need it.
