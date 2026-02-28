# Audio Effects Rework – Design

## Current Problems

1. **Same-y behavior** – Most effects use `getBandEnergyHz(low, high)` → smooth → gradient. Result: "brightness follows volume" with slight spatial variation. No real beat sync, no waves, no frequency-as-position.
2. **No real beat** – BeatPulse uses `CalculateProgress(time)` (speed slider) for wave position, not audio. So the "pulse" is a time-based wave, not beat-triggered.
3. **Flash-only onset** – DiscoFlash and FreqRipple use a simple threshold on band energy to spawn flashes/ripples. No use of `getOnsetLevel()`, no intensity shaping.
4. **2D thinking in 3D** – SpectrumBars/BandScan map spectrum to one axis (X). FreqFill is a 1D VU fill. We have full 3D room; we should use height, depth, radius, and angle.
5. **Hz underused** – We have `getBands()` (per-band), `getBandEnergyHz(low, high)`, `getOnsetLevel()`, `getBassLevel()`/getMidLevel/getTrebleLevel, `getSpectrumSnapshot()`. Most effects only use one scalar level.

## Available Audio APIs (AudioInputManager)

| API | Purpose |
|-----|---------|
| `getBandEnergyHz(low_hz, high_hz)` | 0..1 average energy in Hz range |
| `getBands()` / `getBands(out)` | Per-band 0..1 (log-spaced, band count from settings) |
| `getBassLevel()` | ~20–200 Hz |
| `getMidLevel()` | ~200–2k Hz |
| `getTrebleLevel()` | ~2k–16k Hz |
| `getOnsetLevel()` | Spectral onset strength 0..1 (transient detection) |
| `getSpectrumSnapshot(target_bins)` | Raw FFT bins + peak hold, with min/max Hz |

## Design Principles

1. **3D-first** – Use radius, altitude (Y), angle, distance from origin. Allow axis scale/rotation to fit strips/panels.
2. **Beat = onset + envelope** – Use `getOnsetLevel()` for triggers; use smoothed level for intensity. Optionally BPM or speed only for fallback motion.
3. **Hz → space** – Map frequency (bass→treble or band index) to position: e.g. bass at floor, treble at ceiling; or low Hz left, high Hz right.
4. **Motion and waves** – Traveling waves (phase = time * speed + position), expanding rings, breathing (sin envelope), not just instant flash.
5. **Distinct identities** – Each effect should have a clear, recognizable behavior.

---

## Per-Effect Rework Plan

### BeatPulse3D
- **Goal:** Clear beat-synced wave that feels like the room breathing or a wave passing through.
- **Use:** `getOnsetLevel()` for trigger; smoothed `getBandEnergyHz(bass)` for intensity; optional BPM/speed for wave speed.
- **Behavior:** On onset above threshold, start a new "pulse wave" from origin (or from one wall). Wave front = radius or axis position; falloff behind front (trail). Intensity modulates brightness and optional color. Multiple overlapping pulses possible (decay).
- **3D:** Wave travels along chosen axis or radially. Height/angle modulate brightness (e.g. brighter at center, dimmer at edges).

### AudioLevel3D
- **Goal:** Room fills like a 3D "water level" or wave surface driven by level; not a flat gradient.
- **Use:** `getBandEnergyHz(low, high)` (or full range) smoothed; optionally separate bass/mid/treble for RGB or layers.
- **Behavior:** Level defines a "fill height" or "radius" or "wave surface". Points above the surface get one color, below get another; smooth blend at boundary. Option: wave surface = base_height + level * scale + sin(position)*small_wave for motion.
- **3D:** Fill along chosen axis (Y = height, X = width, Z = depth); or radial fill from center; or spherical "bubble" that grows/shrinks.

### AudioPulse3D
- **Goal:** Whole-room or radial "breathing" pulse: brightness (and optionally size) follows smoothed level, no sharp flashes.
- **Use:** Smoothed `getBandEnergyHz()`; no onset.
- **Behavior:** Single smooth envelope. Brightness = f(level); optional "radius of influence" that grows/shrinks with level so more of the room lights as it gets louder. Color from gradient by level or radial position.
- **3D:** Strong radial falloff from origin; or axis-based (e.g. pulse along Y so ceiling and floor get it differently).

### DiscoFlash3D
- **Goal:** Beat-triggered bursts of color in 3D space; each burst has position and size.
- **Use:** `getOnsetLevel()` for trigger; optionally `getBandEnergyHz()` for burst count/size.
- **Behavior:** On onset, spawn 1–N bursts at random 3D positions (or along a surface). Each burst: position, color (random hue or from gradient), size, decay. Sample point contributes if inside burst radius with smooth falloff. Use proper 3D distance.
- **3D:** Bursts in room volume; optional "floor only" or "ceiling only"; size can scale with onset strength.

### FreqRipple3D
- **Goal:** Beat-triggered expanding ring(s) from origin; clearly distinct from DiscoFlash (rings vs blobs).
- **Use:** Onset for trigger; level for ring strength/width.
- **Behavior:** On beat, spawn ring. Ring = distance from origin in XY, XZ, or spherical. Sample contributes if distance ≈ front (with width). Multiple rings overlap; decay over time.
- **3D:** Choose plane (XY, XZ, YZ) or true spherical. Optional: ripple speed scales with BPM or level.

### BandScan3D
- **Goal:** Single "hot" band that moves through the spectrum over time; position in room = frequency.
- **Use:** `getBands()`; speed controls scan rate.
- **Behavior:** Scan phase (time * speed) picks which band is "hot". Room position along chosen axis = band index. Brightness = band value * proximity to scan position. Trail behind scan.
- **3D:** Keep axis mapping; add height/radial modulation so it's not a flat 2D bar (e.g. bar "height" in Y from band value, or radial fade).

### SpectrumBars3D
- **Goal:** Full spectrum as 3D bars: each bar height = band energy; position = frequency.
- **Use:** `getBands()`; smoothing.
- **Behavior:** Axis = frequency (log-mapped). At each position, "bar height" = that band's energy. Color by position (gradient) or by level. Optional: bars extend in Y (height) or Z (depth); add slight wave/sweep so it's not static.
- **3D:** Bars can grow in Y; or use two axes (e.g. X = freq, Z = time for a waterfall tail). Keep one primary "bar" axis.

### FreqFill3D
- **Goal:** Level-driven fill along one axis (VU style) but with 3D character.
- **Use:** `getBandEnergyHz(low, high)` smoothed.
- **Behavior:** Fill level 0..1; points with position < fill_level light up. Add soft edge, optional "wave" on the fill boundary (sin modulation). Optional: fill from center outward (radius) instead of axis.
- **3D:** Support fill along X, Y, or Z; optional radial fill from center; gradient by position.

---

## LED cube / voxel grid style

Effects that match the vocabulary of physical LED cubes (e.g. [Mega-Cube](https://github.com/MaltWhiskey/Mega-Cube) 16×16×16, or similar 8³ / 16³ grids). See **MEGA_CUBE_ANIMATIONS_REFERENCE.md** for a breakdown of their animation code (Plasma, Sinus, Fireworks, Helix, Life, Cube, Starfield, etc.) and how we map or could implement them.

| Effect | Role |
|--------|------|
| **Cube Layer** (new) | One lit layer (slice) at a time; layer position = audio level. Classic “layer sweep” cube look. |
| **3D Rain** | Falling drops with trail; use on room or virtual cube. |
| **3D Wave** | Circles/squares wave in a plane; speed + scale. |
| **Audio Level** | Fill surface with boundary wave = “water level” through the cube. |
| **Beat Pulse** | Beat-triggered waves expanding from center = radial pulse through voxels. |
| **Freq Ripple** | Beat-triggered expanding ring(s) from origin. |
| **Plasma / Explosion / DNA Helix** | Full 3D motion; axis scale to fit strips or cube. |

Use **Axis Scale** and **Scale** to fit these to real layouts (e.g. one axis compressed for strips, or equal for a cube).

---

## Implementation Order

1. **BeatPulse3D** – ✅ Onset-driven wave + envelope; sets the "beat" standard. (Done: getOnsetLevel() triggers new pulses; speed = expansion rate; beat sensitivity slider.)
2. **AudioLevel3D** – ✅ 3D fill surface / wave. (Done: fill axis X/Y/Z, boundary wave, edge softness; level drives fill height with sin wave on boundary.)
3. **AudioPulse3D** – ✅ Breathing pulse. (Done: "Breathing" checkbox = radius grows with level; quiet = center only, loud = fill room.)
4. **DiscoFlash3D** – ✅ Use getOnsetLevel() for trigger. (Done: trigger from onset instead of band energy.)
5. **FreqRipple3D** – ✅ Use getOnsetLevel() for trigger. (Done: trigger from onset instead of band energy.)
6. **BandScan3D / SpectrumBars3D** – Add height/radial variation; keep spectrum→position.
7. **FreqFill3D** – Optional wave on boundary; optional radial mode.

---

## Shared Utilities (Optional)

- **Onset detector wrapper:** Hold time, threshold, optional min interval. Returns "trigger this frame" + strength.
- **Envelope follower:** Attack/decay on level for smooth "breathing" from getBandEnergyHz or getOnsetLevel.
- **Wave front helper:** Given phase 0..1, radius or axis position, return "distance from wave front" for trail/pulse shape.
