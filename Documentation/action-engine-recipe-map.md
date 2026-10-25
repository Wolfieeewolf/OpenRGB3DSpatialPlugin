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

- **`loop: once`** — drive the look with span/sweep `progress` (0→1, speed may finish early but must not wrap), fade density to **black** before the end, then the player clears LEDs. No recycle/`fract` on progress.
- **`loop: forever` / `while_active`** — animate with continuous **`u_time`** / script `time_ms` only. Do not key ambient motion off wrapping `progress` or wrapped `local_ms` (that flashes/jumps at the pack seam). Rain, helix, fireflies, aurora, portal, etc. already follow this.
- Engines feed `u_time` from continuous elapsed time when playing; GPU one-shot params use non-wrapping span progress; script `progress` is sweep (span × speed, clamped).

## Catch-all multi-pass (any file / tool)

Multi-pass is a **general review tool** — Action toolbox looks, engine C++, packs, docs, UI wiring, anything we touch. Showcase *and* simple utilities. A pass that concludes “already good → no edit” is a valid outcome. The point is a multi-point attack, not forced rewrites.

**Skill (persistent):** `.cursor/skills/openrgb3d-multipass-review/SKILL.md` — also in the user Agent Store. Ask for “multi-pass” / “run the pass” to load it. Includes thermo-nuclear structure pressure, **CONTRIBUTING hygiene** (dead/debug code, minimal comments, no legacy shims), **deslop**, **better-not-worse** gate, **end-of-run skill self-check** (propose process upgrades; apply only if you OK — not file-specific bans), and **apply fixes/updates** (not review-only) across touched files and dependents.

### Attack surfaces (run what applies)

| Pass | Attack surface |
|------|----------------|
| 1 | **Purpose / role** — does it fulfill the intended job? A simple tool must still *be* that tool (e.g. `blink` blinks, `solid` holds a level, `chase` travels). Wrong category, misleading name, or “looks cool but isn’t the role” fails. |
| 2 | **Correctness** — compiles/loads, headers match body (`shader:` / `particles:` / `ribbons:`), swatch/description match, knobs wired, no dead code |
| 3 | **Fit & integration** — check everything it touches: engines, applier, player loop modes, Movie Sync / Triggers packs, Action Editor toolbar, docs, AppData install paths. No orphan headers, broken call sites, or “works alone / breaks in stack”. |
| 4 | **Seams & bugs** — once→black, forever/`while_active` continuous `u_time`, no linger/jump/recycle, intensity/size do something real |
| 5 | **Readability / UX** — sparse-LED survival for GPU looks; clear UI/docs behaviour for utilities |
| 6 | **Character polish** — deepen motion/colour/language only if earlier passes left it meh *and* that fits the role (don’t turn `solid` into aurora) |
| 7–9 | Structure / deslop+CONTRIBUTING (the tidy pass) / close-the-loop + diff tidy sweep; **net better** incl. snappy/feel, or revert |
| 10 | **Skill self-check** — propose “do existing passes better” ideas; don’t auto-edit the skill or memorialize one-off bugs |

**Positive prompts (keep):** honest role, bright cores / clear motion where the look needs it, wired knobs, matches callers/docs, additive glow, seamless loops, once dies clean.

**Negative prompts (kill):** wrong job for the name, thin 1px misses, synced blink, orange wash, progress-wrap flash, linger, orphan/`TODO` wiring, docs drift, unused knobs, silent fail, over-polishing a utility into a showcase.

Basics (`solid`, `blink`, `chase`, …) and media still get **purpose + correctness + fit + seams** every time; readability/character only if something’s wrong or the role isn’t being served.

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
