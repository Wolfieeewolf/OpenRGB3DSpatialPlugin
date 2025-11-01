# 3D Spatial Ambilight - Vision & Roadmap

## Core Vision

Create an ambilight system that leverages real-world 3D LED positions to produce immersive, spatially-aware lighting effects that **pull you into the screen**. Unlike traditional ambilight systems that map screen edges to LED strips in 2D, our system uses geometric projection and distance-based falloff to create a volumetric lighting experience.

### Key Differentiator
**Real-world 3D spatial positioning** - Every LED has an actual position in 3D space. By knowing where displays are located relative to LEDs, we can:
- Apply distance-based intensity falloff (closer LEDs = brighter/sharper, far LEDs = softer/dimmer)
- Use geometric projection to map screen regions to room surfaces (ceiling, walls, floor)
- Create temporal gradients where light "flows" through space
- Produce effects that feel like the room is being filled by the screen content

---

## Design Philosophy

### Two Core User Desires
1. **Full Fidelity Mirror** - Output 100% of screen content (traditional ambilight)
2. **Mood Reactive** - Extract atmosphere, brightness, flashes (additive enhancement)

Both modes must leverage 3D spatial mapping to differentiate from existing solutions.

### The "Drag You In" Effect
The goal is immersion through spatial depth:
- Light should feel like it's **wrapping around** you, not just bouncing off walls
- Nearby LEDs react instantly with high fidelity
- Distant LEDs receive blurred, delayed, softer versions
- Creates the sensation that you're inside the screen content

---

## Technical Architecture

### Core Components

#### 1. Display Plane (Already Implemented)
- `DisplayPlane3D` entity with Transform3D, physical dimensions, bezel info
- Rendered in viewport as translucent quad
- Multiple planes supported for multi-monitor setups
- Persisted in JSON alongside other spatial objects

#### 2. Screen Capture Manager (To Build)
- Platform-specific capture (DXGI Desktop Duplication for Windows first)
- Per-monitor capture streams
- Downscaling pipeline (e.g., 320180 for performance)
- Thread-safe frame buffer access
- Target: 30fps capture, <50ms latency

#### 3. Capture Zones (To Build)
- User-definable rectangular regions on captured frame
- Zone types:
  - **Edge bands** (top/bottom/left/right strips)
  - **Corners** (top-left, top-right, bottom-left, bottom-right)
  - **Center** (middle of screen)
  - **Custom** (arbitrary rectangles)
- Per-zone extraction modes:
  - Full color (all pixels, bilinear sampling)
  - Average color (mean RGB)
  - Peak brightness (max saturation/luminance)
  - Dominant hue (histogram-based)

#### 4. Spatial LED Mapping (To Build)
- For each LED:
  - Project world position onto display plane
  - Calculate intersection UV coordinates
  - Determine distance from plane
  - Classify surface type (ceiling/floor/wall/behind-monitor)
- Cache projections, recalculate only on layout change

#### 5. 3D Falloff & Blending (To Build)
- **Distance-based intensity**: `intensity = base * (1.0 / (1.0 + distance * decay))`
- **Temporal smoothing**: Closer LEDs update faster, distant LEDs lag
- **Directional bias**: Zone position influences which LEDs receive color
- **Surface weighting**: Top screen zones  ceiling LEDs, bottom  floor, etc.

---

## Feature Roadmap

### Phase 1: Foundation (Easy Wins)
**Goal**: Prove the 3D spatial concept with minimal complexity

- [ ] **Screen Capture Manager**
  - Windows DXGI Desktop Duplication
  - Single monitor support
  - Fixed 320180 downscale
  - Thread-safe buffer access

- [ ] **Basic Edge Band Zones**
  - Hardcoded top/bottom/left/right edge strips (e.g., 10% screen height/width)
  - Average color extraction per band

- [ ] **LED  Plane Projection Math**
  - World position  plane UV mapping
  - Distance calculation from plane
  - Cache UVs, only recalculate on layout change

- [ ] **First Effect: `ScreenMirror3D`**
  - Maps edge bands to LEDs using 3D projection
  - Simple distance falloff (inverse-square or exponential)
  - Basic temporal smoothing (EMA filter)
  - Parameters:
    - Display plane selection
    - Falloff distance (mm)
    - Smoothing time (ms)
    - Brightness multiplier

- [ ] **Second Effect: `ScreenMood3D`**
  - Full-frame average color
  - Spreads color through 3D space with distance gradient
  - Slower temporal smoothing (2-3 second fade)
  - Parameters:
    - Display plane selection
    - Spread radius (mm)
    - Fade time (ms)
    - Intensity

**Success Criteria**: User can capture one monitor, see edge colors project onto their LEDs with visible distance falloff, and feel spatial depth.

---

### Phase 2: Customization & Multi-Monitor
**Goal**: Support complex setups and user control

- [ ] **Multi-Monitor Capture**
  - Enumerate displays via DXGI
  - Per-plane capture source binding
  - Handle different resolutions/refresh rates

- [ ] **Custom Capture Zones UI**
  - Visual zone editor (draw rectangles on screen preview)
  - Zone types: edge, corner, center, custom
  - Per-zone extraction mode (average, peak, dominant, full)
  - Zone  LED group manual assignment (optional)

- [ ] **Auto LED Grouping**
  - Automatic classification by 3D position:
    - Distance from plane (near/mid/far)
    - Surface (ceiling/floor/wall/behind-display)
    - Angle relative to plane normal
  - UI to preview groups with color overlays

- [ ] **Advanced Falloff Curves**
  - Presets: Linear, Inverse-Square, Exponential, Custom
  - Per-surface multipliers (ceiling vs floor vs wall)
  - Preview visualization in viewport

- [ ] **Effect: `ScreenReactive3D`**
  - Detects brightness/saturation spikes
  - Triggers burst effects in 3D space (radial expansion)
  - Fast events (explosions)  tight pulse nearby, slow bloom far away
  - Parameters:
    - Threshold (brightness/saturation delta)
    - Burst speed, radius, decay
    - Directional bias

**Success Criteria**: Users can configure 3-monitor setups with custom zones, assign LED groups, and see explosions ripple through their room in 3D.

---

### Phase 3: Audio Integration (The Cool Stuff)
**Goal**: Sync spatial lighting with audio for full A/V immersion

- [ ] **Audio-Enhanced Capture**
  - Extend `AudioInputManager` to expose:
    - Spectral flux (transient detection)
    - Bass energy (low-frequency power)
    - Treble energy (high-frequency power)
    - Stereo balance (L/R bias)

- [ ] **Audio  Spatial Mapping**
  - Low bass  floor/subwoofer area LEDs pulse
  - High treble  ceiling/upper LEDs sparkle
  - Stereo wide  expand 3D gradient spread
  - Transient peaks  trigger reactive bursts

- [ ] **Effect: `ScreenAudioSync3D`**
  - Combines screen capture zones with audio bands
  - Screen provides base color, audio modulates intensity/spread
  - Example: Explosion on screen + bass thump = radial 3D shockwave
  - Parameters:
    - Audio band weights (bass/mid/treble)
    - Spatial modulation (floor/ceiling bias)
    - Sync tightness (latency compensation)

**Success Criteria**: Watching an action movie, explosions pulse the floor with bass, dialogue keeps lighting tight to screen, music spreads atmosphere through the room.

---

### Phase 4: Intelligent Content Detection (Experimental)
**Goal**: Heuristic "fake depth" to infer off-screen space

- [ ] **Basic Scene Heuristics**
  - **Sky detection**: Top 20% bright + blue/white  enhance ceiling
  - **Ground detection**: Bottom dark + brown/green  floor wash
  - **Horizon line**: Detect where "sky meets ground", split ceiling/floor
  - **Letterbox removal**: Detect black bars, ignore in sampling

- [ ] **Peripheral Extrapolation**
  - Sample gradients at screen edges
  - Blur + extend beyond frame into room space
  - LEDs on side walls get "imagined peripheral" color
  - Creates illusion of seeing beyond the screen

- [ ] **Content Adaptive Behavior**
  - Fast cuts + high motion  tighten falloff, increase speed
  - Slow scenes  widen spatial spread, slow smoothing
  - Bright fullscreen  assume daylight, cool temperature on distant LEDs

- [ ] **Effect: `ScreenAdaptive3D`**
  - Blends mirror, mood, reactive modes based on content analysis
  - Auto-adjusts spatial spread and temporal parameters
  - Minimal user config, "just works"

**Success Criteria**: System feels intelligentoutdoor scenes naturally light the ceiling, ground scenes the floor, without manual zone assignment.

---

### Phase 5: Advanced Features (Future)
**Goal**: Push boundaries, unique experiences

- [ ] **Head Tracking Parallax**
  - Webcam-based head position
  - Shift LED weighting based on viewing angle
  - Looking left  right LEDs dim (they're "behind" you)

- [ ] **Virtual Light Physics**
  - Treat screen as emissive surface
  - Raycast to virtual furniture objects (future "room planner")
  - Bounce screen color onto walls, then to LEDs
  - Proper GI simulation for ultimate realism

- [ ] **Per-Content Profiles**
  - User or community-created *.scene.json timelines
  - Embed events/cues for specific movies/games
  - Philips Hue Sync-style experience

- [ ] **Linux Support**
  - PipeWire (Wayland) capture backend
  - X11 shared pixmap fallback
  - Platform abstraction layer for capture manager

---

## Comparison: Traditional vs 3D Spatial Ambilight

| Aspect | Traditional Ambilight | Our 3D Spatial System |
|--------|----------------------|----------------------|
| **Mapping** | Left screen edge  left LED strip (2D) | Left screen edge  all LEDs on left side of room, weighted by 3D distance/angle |
| **Intensity** | Uniform across all LEDs | Graduated based on distance from screen + temporal smoothing |
| **Spatial Awareness** | Flat ring around TV | Volumetric fill considering ceiling, floor, walls, furniture |
| **Responsiveness** | Instant 1:1 color copy | Adaptivenear LEDs fast, far LEDs slow |
| **Content Detection** | None | Heuristic scene analysis (sky/ground, motion, audio) |
| **Multi-Display** | Complex, manual config | Geometric projection handles automatically |
| **Audio Sync** | Rare, bolted-on | Native spatial mapping (bassfloor, trebleceiling) |
| **Unique Selling Point** | "Extends the screen" | "Pulls you inside the screen" |

---

## Technical Decisions & Open Questions

### Settled
- **Display planes as first-class spatial objects** 
- **Centralized ScreenCaptureManager** (separate from planes) 
- **Start with Windows/DXGI** (Linux later) 
- **30fps capture @ 320180 downscale** 
- **Distance-based falloff as core differentiator** 

### To Decide
- **Effect structure**: One mega "Ambilight3D" with modes, or separate effects (Mirror, Mood, Reactive, etc.)?
  - *Leaning toward separate effects for clarity and composability in stack*

- **Zone storage**: Part of DisplayPlane3D, or separate ZoneConfig objects?
  - *Leaning toward separatezones are usage/effect-specific, planes are scene geometry*

- **LED group assignment**: Auto-only, manual-override, or hybrid?
  - *Leaning toward auto with optional manual override per LED controller*

- **Capture resolution**: User-configurable or hardcoded?
  - *Start hardcoded, expose as advanced setting later*

- **Threading model**: Dedicated capture thread per monitor, or one thread with multi-monitor polling?
  - *One thread, round-robin or async per-monitorsimpler resource management*

---

## Success Metrics

### User Experience
- "Feels 3D" - Users report sensation of depth and immersion
- "Doesn't distract" - Mood/reactive modes enhance without overwhelming
- "Easy to set up" - Multi-monitor config under 5 minutes

### Technical
- <50ms capture-to-LED latency
- Stable 30fps capture on mid-range hardware
- No dropped frames or stuttering
- Minimal CPU usage (<5% on modern quad-core)

### Ecosystem
- Community creates custom zones/profiles for popular content
- Other developers extend with new capture backends
- Becomes the reference implementation for spatial ambilight

---

## How to Use (Quick Start)

### Prerequisites
1. **Create Display Planes** in the "Display Planes" tab (left side)
   - Click "+ Add Plane"
   - Set width, height, and bezel dimensions to match your monitor
   - Position the plane in 3D space where your monitor actually is
   - Optionally assign a capture source ID (monitor identifier)

2. **Position LED controllers** in 3D space around your monitors
   - Use the 3D viewport to place controllers where they physically exist in your room
   - The effect uses real-world positions to calculate distance falloff

### Using ScreenMirror3D Effect

1. Go to **Effect Stack** tab (right side of UI)
2. Click **+ Add Effect** button
3. In the "Effect Type" dropdown, select **Screen Mirror 3D** (under "Ambilight" category)
4. Configure settings in the panel below:
   - **Display Plane** - Choose which monitor/plane to capture from
   - **Monitor Override** - Optional, manually pick a capture source
   - **Falloff Distance** - How far the light reaches in mm (default: 1000mm)
   - **Falloff Curve** - Linear, Inverse Square (default), or Exponential
   - **Edge Band Thickness** - How much of screen edge to sample (0.01-0.5, default: 0.1)
   - **Brightness** - Intensity multiplier (default: 1.0)
   - **Smoothing Time** - Temporal smoothing in ms (default: 50ms)
   - **Bilinear Filtering** - Smoother color sampling (recommended: ON)
5. Click **Refresh Monitors** to detect available screens
6. The effect will now project screen edge colors onto nearby LEDs with 3D spatial falloff!

### Understanding the 3D Spatial Magic

The effect calculates for each LED:
- **Distance** from LED to the display plane surface (using real 3D positions)
- **Closest screen edge** based on geometric projection (top/bottom/left/right)
- **Intensity falloff** based on distance using configurable curves
- **Result**: LEDs close to screen = bright, sharp colors; LEDs far away = soft, dimmed colors

This creates the immersive "pulled into the screen" feeling that traditional ambilight lacks!

---

## Implementation Status

###  Phase 1: Foundation (COMPLETE)

-  ScreenCaptureManager with multi-monitor DXGI support
-  DisplayPlane3D entity with Transform3D positioning
-  Geometry3DUtils for 3D projection math
-  ScreenMirror3D effect with edge band sampling
-  Distance-based falloff (3 curve types)
-  Effect auto-registration system
-  Thread-safe capture at 30fps

###  Next Steps (Phase 2)

1. **Wire DisplayPlaneManager to UI** - Populate global manager when planes change
2. **Tune falloff curves** - Find the "sweet spot" that feels immersive
3. **Add ScreenMood3D** - Full-frame average color mode
4. **Multi-monitor testing** - Verify 3-monitor setups work correctly
5. **User testing** - Get feedback on "does it feel 3D?"

###  Future Phases

- **Phase 3**: Audio Integration (bassfloor, trebleceiling)
- **Phase 4**: Content Detection (sky/ground heuristics, peripheral extrapolation)
- **Phase 5**: Advanced Features (head tracking, light physics simulation)

---

## Long-Term Vision

**The Room Planner Future**

Once ambilight is solid, the display plane foundation enables:
- Furniture objects (desks, shelves, entertainment units)
- LED strips attached to furniture (under-desk, shelf edge, chair)
- Effects that understand room layout (bounce light off walls, shadows, occlusion)
- Full "digital twin" of user's space

The ambilight feature is the proof-of-concept that spatial awareness **matters**. It's the wedge that makes the room planner idea tangible and valuable.

---

**Document Version**: 1.0
**Last Updated**: 2025-10-23
**Status**: Vision & Planning Phase

