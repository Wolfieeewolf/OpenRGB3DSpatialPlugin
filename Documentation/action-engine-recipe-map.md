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

- **`loop: once`** — drive the look with span/sweep `progress` (0→1, speed may finish early but must not wrap), fade density to **black** before the end, then the player clears LEDs. No recycle/`fract` on progress. Canonical helpers: `ClipSpanProgress` (envelope / once fade), `ClipSweepProgress` (script `progress`).
- **`loop: forever` / `while_active`** — animate with continuous **`u_time`** / script `time_ms` only (`GpuAnimMs` / `GpuAnimMsOr` — not wrapping `local_ms`). Do not key ambient motion off wrapping `progress` or wrapped `local_ms` (that flashes/jumps at the pack seam). Rain, helix, fireflies, aurora, portal, etc. already follow this.
- Engines feed `u_time` from continuous elapsed time when playing; GPU one-shot params use non-wrapping span progress; script `progress` is sweep (span × speed, clamped). Timeline GPU thumbs and LED apply share **`SpatialOffscreenGlPool`** (no second GL context); true-GPU tools fail closed to black (no CPU dual / no gradient fake thumbs). Clear-on-idle: every path that empties the player calls `ClearActionPreview`.

## Catch-all multi-pass (any file / tool)

Multi-pass is a **general review tool** — Action toolbox looks, engine C++, packs, docs, UI wiring, anything we touch. Showcase *and* simple utilities. A pass that concludes “already good → no edit” is a valid outcome. The point is a multi-point attack, not forced rewrites.

**Skill (persistent):** `.cursor/skills/openrgb3d-multipass-review/SKILL.md` — also in the user Agent Store. Ask for “multi-pass” / “run the pass” to load it. Includes thermo-nuclear structure pressure, **CONTRIBUTING hygiene**, **deslop**, **better-not-worse**, **architectural ceiling**, **wiring + UI integrity**, **pass 2b security** (plugin-tuned [VibeSec](https://github.com/BehiSecc/VibeSec-Skill) at `.cursor/skills/vibesec/`; Bugbot stays a separate diff review), **ratchet re-check** (re-apply new gates on recent/high-traffic surfaces — not a full historical redo), **keep/kill prompts**, **draft-to-refine seed loop** (first landing is a draft; multipass then refine before commit), **end-of-run skill self-check** (propose only; apply if you OK), and **apply fixes/updates** across touched files.

### Attack surfaces (run what applies)

| Pass | Attack surface |
|------|----------------|
| 1 | **Purpose / role** — does it fulfill the intended job? A simple tool must still *be* that tool (e.g. `blink` blinks, `solid` holds a level, `chase` travels). Wrong category, misleading name, or “looks cool but isn’t the role” fails. |
| 2 | **Correctness + dead code** — loads/compiles; headers match body (`shader:` / `particles:` / `ribbons:`); swatch/knobs honest; same-engine sibling tools share one header skeleton (icons/knobs/paint — header-only orphans fail); if `.fx` has `shader:` + `knobs:… pulse`, paired `.fs` must use `param: size` / `u_params[2]`; **remove** unused helpers/paths/debug leftovers |
| 3 | **Fit, integration & wiring** — engines, applier, player, Movie Sync / Triggers, Action Editor toolbar, docs, AppData; **trace signals/slots and knob↔field links**; live-preview dirty flag must cover mutators that bypass the form-write path; **every path that empties the player must ClearActionPreview** (Stop / uncheck / level-off / natural end); no orphans, half-wired actions, or “works alone / breaks in stack”. |
| 4 | **Seams & bugs** — once→black; forever/`while_active` continuous `u_time` (`GpuAnimMs`); no linger/jump/recycle; clear-on-idle for every stop path; shared ApplyPackFrame seq still pushes **per-clip** params; intensity/size do something real |
| 5 | **Readability / UX + UI integrity** — sparse-LED survival; clear utility behaviour; **human UI copy** (no placeholder/raw-id junk); usable layout; broken/empty controls fail |
| 6 | **Character polish** — deepen motion/colour/language only if earlier passes left it meh *and* that fits the role (don’t turn `solid` into aurora) |
| 7–9 | **Structure** (code judo, ~1k-line ceiling, reuse `ClipSpan`/`ClipSweep`/`GpuAnimMsOr` — no parallel stacks vs singular `SpatialOffscreenGlPool`) / **deslop+CONTRIBUTING** (the tidy pass) / **close-the-loop** + diff tidy sweep; **net better** incl. snappy/feel, or revert |
| 10 | **Skill self-check** — propose “do existing passes better” ideas; don’t auto-edit the skill or memorialize one-off bugs |

**Positive prompts (keep):** honest role, bright cores / clear motion where the look needs it, wired knobs, matches callers/docs, additive glow, seamless loops, once dies clean, human-readable UI copy, canonical clocks/progress (`ClipSpanProgress` / `ClipSweepProgress` / `GpuAnimMsOr`), shared offscreen pool.

**Negative prompts (kill):** wrong job for the name, thin 1px misses, synced blink, orange wash, progress-wrap flash, linger, orphan/`TODO` wiring, dead connects, missing `ClearActionPreview`, garbage/raw-id UI strings, docs drift, unused knobs, silent fail / CPU dual / gradient fake thumbs, over-polishing a utility into a showcase.

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
| `roomscape_glow_fireflies` | `triggers/effects/roomscape/` | field + particles |
| `roomscape_helix_rain` | `movie-sync/effects/washes/` | ribbon + particles |
| `roomscape_aurora` | triggers + movie-sync washes | `field_aurora` |
| `roomscape_portal` | triggers + movie-sync washes | `field_portal` (+ electric) |
| `roomscape_meteor_storm` | triggers + movie-sync explosions | `field_dual_vortex` + `particle_meteors` |
| `roomscape_embers` | triggers + movie-sync washes | `field_strata` + `particle_embers` |

## Deferred / next (later)

- **Multipass (Claude)** — **parked until ~2026-11-23** (other-models usage reset). Plugin resume: [multipass-coverage.md](multipass-coverage.md) § Parked for Claude reset. Presets every-file judgment: sibling [OpenRGB3DSpatialPresets `docs/multipass-coverage.md`](../../OpenRGB3DSpatialPresets/docs/multipass-coverage.md) (hybrid hygiene done 2026-10-27; rows `pending`/`hygiene` until Claude keep/kill). Do not finish judgment passes with a weaker model.
- **Engine folder regroup** — phases 1–9 done (`engines/*` + `shared/` + `ui/` affinity); effect base / zones / controllers still at repo root; [plugin-file-map.md](plugin-file-map.md).
- **Line splits (later)** — FolderVolume ~2.6k, Reactive ~1.5k, AudioInputManager ~1.4k (after Claude ratchet where noted).
- **3D grid placement editor** — place volume clips in the viewport (translate/rotate/scale); keep timeline; switch modes. Separate branch.
- **Swarm / flock** particle mode (spatial hash).
- **More showcase packs** once the first five feel good in real rooms.
- **Ribbon polish** (multi-tube, accent lights) if neon looks need it.
- **DualSense ControllerTorch** — parked on `feat/dualsense-controller-torch` until 9-axis mag.
