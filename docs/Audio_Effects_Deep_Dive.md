# Audio Effects Deep Dive – 3D Room, Surfaces, and “Feel”

A design vision for making audio effects feel physical, beat-driven, and great on **any** surface: strips, panels, ceiling, floor, single controller, or full room.

---

## 1. Current State (Audit)

### What We Have

| Effect | What it does today | Gap |
|--------|--------------------|-----|
| **Beat Pulse** | Onset → spawn radial waves from center; wave front expands; envelope from band energy. | Works in room center; on a **strip** or **single panel** you only see a slice—often no wave “hit” at all. No **particle** or **impact** feel. |
| **Audio Level** | Level → fill height along one axis; optional boundary wave. | “Water level” is clear but static; no **beat pop** or **bounce**. Doesn’t suggest “bass” vs “treble”. |
| **Audio Pulse** | Smoothed level → brightness; optional radial “breathing” (radius grows with level). | Smooth only; no **transient** or **punch**. Same on every surface. |
| **Disco Flash** | Onset → spawn random 3D flash blobs (nx,ny,nz); Sparkle mode = time-based twinkle. | Random blobs don’t read as **disco floor**, **ceiling**, or **mirror ball**. No surface-aware layout. |
| **Freq Ripple** | Onset → expanding rings from origin. | Good for center; on strips/panels you see a line, not “disco”. |
| **Spectrum Bars** | Bands → bar height along X; height/radial profile; slow sweep. | **No beat**—bars are level-only. No “bounce on kick”. |
| **Band Scan** | Time-based scan picks “hot” band; position = spectrum. | No beat sync; scan is speed slider only. |
| **Freq Fill** | Level → fill along axis; soft edge. | VU style; no bounce or particle. |

### Why They Don’t Yet “Feel” Right

1. **Bass** – Nothing that feels like a **physical punch**: a wave that hits you, or particles that burst out on the kick. Beat Pulse is a wave but not particle-like; Audio Pulse is smooth, no transient.
2. **Spectrum** – Bars and scan are **level → value**. No **onset → bounce** (e.g. bar jumps on beat then decays).
3. **Disco** – Flashes are random 3D blobs. Real disco has: **floor** (grid of tiles), **ceiling** (lights above), **mirror ball** (rotating, spots), **wall/panel** (chase, blocks). We don’t map LED **position** to those roles.
4. **Any surface** – Effects assume “room with center”. On a **strip** (line), **panel** (rectangle), or **single device**, “origin” and “radius” often don’t match geometry, so the effect looks weak or wrong.

---

## 2. Design Principles for “Any Surface”

### 2.1 Surface-Aware, Not Just 3D-Aware

- **Strip** (1D in space): effects should use **length** and **direction**; e.g. bass = pulse along strip, spectrum = bars along strip, disco = chase or segment flash.
- **Panel / wall** (2D): **floor** = Y low or Z back, **ceiling** = Y high, **wall** = one of XZ planes; mirror ball = center + radial; “disco floor” = grid on that plane.
- **Room** (3D): keep current behavior but add **regions** (floor vs ceiling vs wall) so we can drive them differently (e.g. floor and ceiling flash on beat, mirror ball in center).

So: same effect **logic** (beat, level, spectrum), but **interpretation** of (x,y,z) depends on **layout** or **user choice** (e.g. “this effect is for floor”, “this is mirror ball”).

### 2.2 Beat = Onset + Envelope

- **Onset** (existing `getOnsetLevel()`): trigger **events**—new pulse, new bounce, new flash, new ripple.
- **Envelope** (smoothed level): **intensity** and **decay** of that event (e.g. how high the bar bounces, how wide the bass ring).

So: **bass beat** = onset triggers a “hit” + envelope shapes how big/long it is. **Spectrum bounce** = onset triggers bar “bounce” + envelope = height. **Disco** = onset triggers flash/chase + envelope = brightness/size.

### 2.3 Particle-Like Without Real Particles

We don’t need a full particle system. We need **per-LED, procedural “particle-like”** feel:

- **Bass:** At each LED, a **distance-from-origin** and **time since last onset**. Brightness = short burst that expands outward (ring or sphere) and fades. So: **one expanding shell** per beat, no particle list.
- **Disco:** Per LED, **tile index** (floor grid) or **angle/distance** (mirror ball) or **segment** (strip). On beat, light by tile/angle/segment with decay. No need to store “particles”, just **last N onset times** and **phase**.

---

## 3. Bass: “Bass Beat Pulsing Out”

### Goal

Bass should feel like a **punch** or **thump** that **travels outward** (or along a strip): something that **hits** on the kick and then **decays**.

### 3.1 Current vs Desired

- **Current:** Beat Pulse = radial wave front (smooth band); Audio Pulse = smooth breathing. Neither has a sharp **impact** or **particle-like** burst.
- **Desired:** On bass onset, a **single expanding shell** (or short burst) that:
  - Starts at origin (or sub position) and **expands** (radius = f(time - onset)).
  - Has a **bright core** and **softer trail** (or just a thin ring).
  - **Decays** in time (envelope) and optionally in **distance** (so it feels like it’s “going through” the room).

### 3.2 Implementation Sketch (No New Effect Yet)

- **Option A – Enhance Beat Pulse:**  
  - Keep existing wave front.  
  - Add a **second layer**: on each onset, record `last_onset_time` and `onset_strength`. For each LED, `distance_from_origin` and `age = time - last_onset_time`.  
  - **Impact shell:** brightness = sharp peak when `age` is small and `distance ≈ speed * age` (expanding ring) with fast decay in `age` and soft falloff in `distance`.  
  - So: **wave** (existing) + **impact ring** (new) = “bass pulse out”.

- **Option B – New “Bass Impact” effect:**  
  - Dedicated effect: **only** onset-driven expanding shell(s).  
  - Multiple recent onsets (e.g. last 2–3) so double kicks give two rings.  
  - User can place it as a **frequency range** (bass only) and blend with other layers.

- **Strip/panel:** Use **path axis** (X/Y/Z) so “expansion” is along the strip (one-way or from-center). So on a strip, “pulse out” = bright segment that moves along the strip from one end (or center) and fades.

### 3.3 Parameters (for Option A or B)

- **Bass source:** low_hz–high_hz (or “use bass band”).
- **Onset threshold**, **smoothing** (existing).
- **Impact speed** (how fast the ring expands).
- **Impact width** (thickness of ring / core).
- **Decay time** (how long the ring stays visible).
- **Path axis** (for strips: pulse along axis instead of full 3D radius).

---

## 4. Spectrum: “Bars Bouncing to the Beat”

### Goal

Spectrum bars should **react to the beat**: on onset, bars **jump** (or **bounce**) and then **settle** to the level-driven height. So you get **rhythm** as well as **frequency**.

### 4.1 Current vs Desired

- **Current:** Bar height = smoothed band value; optional sweep. No onset.
- **Desired:**  
  - **Base height** = smoothed band (as now).  
  - **Onset** adds a **bounce**: e.g. `bounce = A * exp(-k * (time - last_onset))` so right after a beat all bars (or bass bar) get a temporary boost.  
  - Option: **only bass bar** bounces, or **all bars** bounce with decay by frequency (bass bounces more).

### 4.2 Implementation Sketch

- **SpectrumBars3D:**  
  - Maintain `last_onset_time` (global or per-band; global is simpler).  
  - In `ComposeColor` (or equivalent):  
    - `base_height = smoothed_bands[...]` (existing).  
    - `bounce = onset_strength * exp(-decay * (time - last_onset_time))`; optionally scale by band (e.g. more bounce for low bands).  
    - `effective_bar_height = base_height + bounce` (clamped).  
  - Use `effective_bar_height` for the “bar” so that **above** that height the LED is dim, **below** it’s lit. So on beat, the bar “jumps” up and then falls back.

- **BandScan3D:**  
  - Same idea: on onset, **boost** the “hot” band or **freeze** the scan for one moment so the scan “stops” on the beat then continues (optional).

### 4.3 Parameters

- **Beat sensitivity** (onset threshold).
- **Bounce amount** (how much the bar jumps on beat).
- **Bounce decay** (how fast it settles back).
- **Bass-only bounce** (checkbox: only low bands bounce).

---

## 5. Disco: Floor, Ceiling, Mirror Ball, Panels

### Goal

Disco should feel like a **real disco**: **floor** (grid of lit tiles), **ceiling** (lights above), **spinning mirror ball** (moving spots), **wall/panel** (chase or blocks). Same effect should **adapt** so that on a **strip** it’s a chase, on a **floor plane** it’s tiles, on a **ceiling** it’s ceiling lights, and in the **center** it can be mirror ball.

### 5.1 Modes (Same Effect, Different Layout)

| Mode | Interpretation of (x,y,z) | Behavior |
|------|---------------------------|----------|
| **Disco Floor** | Y ≈ min (or “floor” plane): (x,z) = tile grid. | On beat, **random tiles** (or checkerboard) flash; color by tile or random; decay. |
| **Disco Ceiling** | Y ≈ max (or “ceiling” plane): (x,z) = tile grid. | Same as floor but for ceiling LEDs. |
| **Mirror Ball** | Distance + angle from “center”; Y up. | **Rotating** pattern (phase = time + angle); on beat, **brighter** or **more spots**. Spots = angular bands (e.g. 4–8 segments) that move. |
| **Wall / Panel** | One axis = “along strip” or “along row”. | **Chase**: on beat, a **block** of LEDs lights and moves; or **random segments** flash (like current flash but 1D segments). |
| **Classic (current)** | Random 3D blobs. | Keep as “random flash” option. |

So: **one** Disco effect with a **mode** dropdown: **Floor**, **Ceiling**, **Mirror ball**, **Wall chase**, **Random (classic)**.

### 5.2 Implementation Sketch

- **Floor / Ceiling:**  
  - Choose **plane** by **path axis** or **Y**: e.g. “floor” = LEDs with `y ≤ center_y - margin`, “ceiling” = `y ≥ center_y + margin`.  
  - For those LEDs, **(x,z)** (or the two non-height axes) → **tile index** (e.g. `tile_x = floor(x / tile_size)`, `tile_z = floor(z / tile_size)`).  
  - On onset: pick **random tiles** (or use hash(tile_x, tile_z, beat_index) so some tiles flash).  
  - Flash color and decay as now; **tile_id** gives deterministic “which tiles this time”.

- **Mirror ball:**  
  - Origin = center (or configurable). For each LED: **angle** (e.g. atan2(y, x) in horizontal plane), **elevation** (angle from horizontal).  
  - **Phase** = time * spin_speed + offset.  
  - “Spots” = e.g. 6–8 angular segments; brightness = `max(0, cos(angle - phase))` or similar so spots rotate.  
  - On beat: **all spots** brighter, or **extra** spot, or spin_speed bumps.

- **Wall / Panel:**  
  - **Strip**: one axis = length; segment index = floor(axis_pos * num_segments). On beat, **random segments** (or one chasing segment) flash.  
  - **Panel**: 2D grid of “tiles”; same as floor but for a chosen plane (e.g. back wall = Z max).

### 5.3 Parameters

- **Mode:** Floor / Ceiling / Mirror ball / Wall chase / Random.
- **Tile size** (for floor/ceiling/wall): scale of grid (or “auto” from room bounds).
- **Spin speed** (mirror ball).
- **Number of spots** (mirror ball).
- **Chase length** (wall: how many LEDs per block).
- **Decay**, **density**, **threshold** (as now).

### 5.4 “Any Surface” in Practice

- **Single strip:** Mode = **Wall chase** or **Random**; chase/segment along strip.  
- **Ceiling only:** Mode = **Ceiling**; only ceiling LEDs get tile flash.  
- **Floor only:** Mode = **Floor**.  
- **Full room:** Mode = **Floor** + **Ceiling** (or two layers: one floor, one ceiling) + optional **Mirror ball** at center.  
- **Panel:** Mode = **Wall** or **Floor** (treat panel as one plane).

---

## 6. Implementation Order (Pragmatic)

1. **Spectrum Bars – Beat bounce**  
   - Add onset tracking; in SpectrumBars3D, add bounce term to bar height; params: beat sensitivity, bounce amount, bounce decay.  
   - Small change, high impact.

2. **Beat Pulse – Bass impact layer**  
   - Add expanding “impact ring” on onset (in addition to existing wave); optional path axis for strips.  
   - Makes bass feel like it “pulses out”.

3. **Disco – Modes**  
   - Add mode: Floor / Ceiling / Mirror ball / Wall chase / Random.  
   - Implement Floor and Ceiling (tile grid by plane); then Mirror ball (angle + spin); then Wall chase (segment along axis).  
   - Reuse existing flash decay/color.

4. **Bass-only / surface-aware options**  
   - Where useful, add “path axis” or “plane” so effects work on strips and panels without requiring full 3D room.

---

## 7. Summary Table (Target)

| Effect | Today | After |
|--------|--------|--------|
| **Bass / Beat Pulse** | Radial wave only | Wave + **impact ring** on onset; optional **pulse along axis** for strips. |
| **Spectrum Bars** | Level → bar height | Level + **beat bounce** (bars jump on onset, then decay). |
| **Disco** | Random 3D blobs | **Floor** / **Ceiling** (tile grid) / **Mirror ball** (rotating spots) / **Wall chase** / **Random**. |
| **Audio Level / Pulse** | Smooth only | Optional **onset boost** (short brightness bump on beat) so they can “hit” a bit. |
| **Freq Ripple** | Rings from center | Unchanged or add **path axis** so on a strip it’s a “line ripple”. |

All of this keeps **per-LED, no particle list** where possible (expand ring = formula in distance and time; disco tiles = hash of tile id and beat index), so performance stays good and effects still look great on **any surface**: strip, panel, ceiling, floor, or full 3D room.
