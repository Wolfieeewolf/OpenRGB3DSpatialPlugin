# Audio UI and Effects – Super Deep Dive (Current Reference)

Full inventory of our audio pipeline, UI, and effects; comparison with OpenRGB Effects Plugin audio; and how to bring their best ideas into our 3D space. References: **MEGA_CUBE_ANIMATIONS_REFERENCE.md**, **EFFECT_CONVERSION_GUIDE.md**, **AUDIO_EFFECTS_REWORK.md**, **Audio_Effects_Deep_Dive.md**, **Audio_Pipeline_Deep_Dive.md**.

---

## Part 1 – Our Audio System (Complete Inventory)

### 1.1 Audio pipeline (capture → FFT → bands/level/onset)

| File | Role |
|------|------|
| **Audio/AudioInputManager.h** | Singleton API: device list, start/stop, gain, smoothing, bands count, FFT size, crossovers; `level()`, `getBands()`, `getBandEnergyHz()`, `getBassLevel()` / `getMidLevel()` / `getTrebleLevel()`, `getOnsetLevel()`, `getSpectrumSnapshot()`; FeedPCM16. |
| **Audio/AudioInputManager.cpp** | WASAPI capture (loopback/capture), mono sum, `processBuffer()` (RMS, gain, auto-level, EMA level); when `sample_buffer.size() >= fft_size` runs `computeSpectrum()` (windowed FFT, log-spaced bands, log10(1+9*v) per band, **smoothed max** normalization, bass/mid/treble, onset = spectral flux); `updateVisualizerBuckets` for snapshot. |

**Defaults (after pipeline fix):** gain default ~3.5x (slider 35), 8 bands, 512 FFT, band normalization by smoothed running max.

### 1.2 Audio UI

| File | What it does |
|------|----------------|
| **ui/OpenRGB3DSpatialTab_Audio.cpp** | **SetupAudioPanel()**: Group "Audio Input". Start/Stop, Level bar (0–1000), Input Device combo, Gain (1–100 → 0.1–10x, default 35), Bands (8/16/32, default 8), FFT Size (512–8192). Help text. Then **SetupFrequencyRangeEffectsUI()**. Load/save AudioDeviceIndex, AudioGain, AudioBands, AudioFFTSize from plugin settings. **UpdateAudioPanelVisibility()**: show panel only when selected stack effect is category "Audio". |
| **ui/OpenRGB3DSpatialTab_FreqRanges.cpp** | **Frequency Range Effects** group: list (freq_ranges_list), Add/Remove/Duplicate. Per-range details: Name, Enabled, **Low Hz** / **High Hz** (20–20k), **Effect** combo (Audio effects only, no AudioContainer3D), **Zone** combo, **Origin** combo (Room Center or reference points). **freq_effect_settings_widget**: child layout for the selected effect’s SetupCustomUI (so each range can have its own effect instance and settings). Save/Load **frequency_ranges** and **next_freq_range_id** in plugin settings. **Note:** FrequencyRangeEffect3D also has smoothing, sensitivity, attack, decay (used in render); if no dedicated UI for them in the tab, they come from JSON only or effect defaults. |

**Flow:** User picks an Audio effect in the main stack → Audio panel appears. There they start listening, set device/gain/bands/FFT, and below that configure **frequency ranges** (each = name, Hz range, effect type, zone, origin, effect-specific settings). Ranges are independent “layers” that only run when audio is running and are blended additively in render.

### 1.3 Frequency range data and render

| File | Role |
|------|------|
| **FrequencyRangeEffect3D.h** | Struct: id, name, enabled, low_hz, high_hz, effect_class_name, zone_index, origin_ref_index, position/rotation/scale, effect_settings (json), **smoothing, sensitivity, attack, decay**, effect_instance (SpatialEffect3D), current_level, smoothed_level. SaveToJSON / LoadFromJSON. |
| **ui/OpenRGB3DSpatialTab_EffectStackRender.cpp** | When **has_enabled_freq_ranges** and **AudioInputManager::instance()->isRunning()**: for each range, `raw_level = getBandEnergyHz(range->low_hz, range->high_hz)`, attack/decay on current_level, smoothing → smoothed_level, `effect_level = clamp(smoothed_level * sensitivity)`. Effect instance created from effect_class_name if missing; **LoadSettings** called each frame with low_hz, high_hz, audio_level, frequency_band_energy (and origin from reference point if set). Then per grid point: **slot effects** blended first, then **frequency range effects** blended **additively** (R+G+B add, capped 255). |

So: each range drives one effect instance with a dedicated Hz band and optional zone/origin; the effect’s CalculateColorGrid still reads from AudioInputManager directly (and can use the injected settings if it wants).

### 1.4 Shared audio helpers

| File | Role |
|------|------|
| **Effects3D/AudioReactiveCommon.h** | AudioReactiveSettings3D (low_hz, high_hz, smoothing, falloff, foreground/background gradients, background_mix, peak_boost). Helpers: ApplyAudioIntensity, ComposeAudioGradientColor, SampleGradient, NormalizeGradient, BlendRGBColors, ScaleRGBColor, ModulateRGBColors, ComputeRadialNormalized, NormalizeRange. Used by most Audio effects. |

### 1.5 Our Audio effects (category "Audio")

| Effect | Files | Audio API used | Short behavior |
|--------|--------|----------------|----------------|
| **Beat Pulse** | BeatPulse3D.{h,cpp} | getOnsetLevel(), getBandEnergyHz(low, high) | Onset triggers radial pulse list; wave front expands; SamplePulseField(radial_norm, height_norm); envelope + gradient. |
| **Audio Level** | AudioLevel3D.{h,cpp} | getBandEnergyHz(low, high) | Fill “water level” along path axis (X/Y/Z); boundary wave (sin); edge softness; gradient by position. |
| **Audio Pulse** | AudioPulse3D.{h,cpp} | getBandEnergyHz(low, high) | Smoothed level → brightness; optional radial fade; “Breathing” = radius grows with level. |
| **Disco Flash** | DiscoFlash3D.{h,cpp} | getOnsetLevel() (Beat mode) | Onset → spawn flashes at random (nx,ny,nz); Sparkle mode = time-based twinkle. |
| **Freq Ripple** | FreqRipple3D.{h,cpp} | getOnsetLevel(), getBandEnergyHz | Onset → spawn expanding ring; trail width, decay. |
| **Spectrum Bars** | SpectrumBars3D.{h,cpp} | getBands() | Log-spaced bands → bar height along X; height/radial profile; sweep. |
| **Band Scan** | BandScan3D.{h,cpp} | getBands() | Time-based scan picks “hot” band; position = spectrum axis. |
| **Freq Fill** | FreqFill3D.{h,cpp} | getBandEnergyHz(low, high) | Level → fill along axis; soft edge. |
| **Cube Layer** | CubeLayer3D.{h,cpp} | getBandEnergyHz(low, high) | Level → one lit “layer” (slice) along path axis; layer thickness. |
| **Audio Container** | AudioContainer3D.{h,cpp} | — | Placeholder; no rendering; marks “audio” for pipeline. |

All of these (except Audio Container) can be used **in the main effect stack** and/or as the **effect** of a **frequency range**. When used in a range, the range’s low_hz/high_hz are written into the effect each frame; the effect still reads from AudioInputManager (with that band).

---

## Part 2 – OpenRGB Effects Plugin Audio (Inventory)

### 2.1 Their audio pipeline

| File | Role |
|------|------|
| **Audio/AudioManager.{h,cpp}** | Singleton; Capture(device_idx, float buf[512]). Per-device capture thread, 512-sample buffer. Windows: WASAPI loopback/capture, float samples. Linux: OpenAL, 256 samples → 512. |
| **Audio/AudioSignalProcessor.cpp** | Process(FPS, settings): Capture 512 samples, **amplitude** (gain), window (Hanning/Hamming/Blackman), **rfft(256)**, magnitude with peak-hold (only update bin if new > old), decay per frame. **Normalization** curve (nrml_ofst + nrml_scl * i). **Averaging**: avg_mode (binning or low-pass), avg_size. **16-band equalizer** (fft_fltr). Output: **data.fft_fltr[256]** (filtered FFT). |
| **Audio/AudioSettingsStruct.h** | audio_device, **amplitude** (100), avg_mode, avg_size, window_mode, decay (80), filter_constant, nrml_ofst, nrml_scl, equalizer[16]. |
| **Audio/AudioSettings.{h,cpp}** | Global audio settings UI (device, amplitude, etc.). |

So: **fixed 256 FFT bins**, peak-hold + decay (no per-frame “max = 1” renormalization), amplitude default 100, per-bin normalization curve. Each effect that uses audio holds an **AudioSignalProcessor** and calls **Process(FPS, &audio_settings_struct)** in its StepEffect.

### 2.2 Their audio effects (list and one-liner)

| Effect | One-line idea |
|--------|----------------|
| **Audio Sync** | 256 bins → 256×64 “graph”; hue per bin (rainbow), value = fft_fltr[i]; find max in bypass band → dominant hue/sat/val; **roll modes**: Linear, None, Radial, Wave, Linear2 – how that color is applied along the strip (position = LED index). |
| **Audio Visualizer** | Full 256×64 visualizer: bar graph + foreground/background patterns (solid, static, animated rainbows, color wheels, spectrum cycle). Zone mapping: matrix (x,y) or linear (x). Many pattern and single-color options. |
| **Audio VU Meter** | Level → “height” (fill along strip); color_offset, color_spread, saturation, invert_hue. Classic VU. |
| **Audio Sine** | **Sine wave** whose **height** is driven by audio (height_mult); repeat, glow, thickness, oscillation; color modes. |
| **Audio Star** | Star/burst pattern; **edge_beat**: on transient, edge LEDs get a different color/sensitivity. |
| **Audio Party** | **Divisions** (segments); **effect_threshold**; motion_zone_stop / color_zone_stop (ranges). Wave color + background; “party” segmentation. |
| **Audio Bubbles** | **Bubbles** (circles) that **expand** from a center; trigger by volume; max_bubbles, max_expansion, thickness, speed; spawn mode (random XY, X, Y, center); presets (Unicorn Vomit, Borealis, Ocean, etc.). |
| **Swirl Circles Audio** | **Rotating circles** (swirl); **radius** and **current_level** (sum of fft_fltr); GetColor(x, y, zone, …) for 2D position. So: swirl + audio-driven brightness/size. |

### 2.3 Mapping their effect → our 3D space

| Their effect | Our equivalent or gap | How to do it in 3D |
|--------------|------------------------|--------------------|
| **Audio Sync** | Spectrum Bars + hue by frequency; “roll” = how color moves along strip | We have SpectrumBars3D (axis = freq). Add “roll” idea: color shift along axis (time or beat). Or: use dominant-freq hue like Audio Sync in a 3D bar or radial layout. |
| **Audio Visualizer** | SpectrumBars3D, BandScan3D | We have bars and scan. Their patterns (rainbow, color wheel, spectrum cycle) → our foreground/background gradients and rainbow mode. Matrix zone mapping → our grid (x,y,z). |
| **Audio VU Meter** | FreqFill3D, AudioLevel3D | We have fill and level. VU is 1D fill; we already have path axis. Optional: dedicated “VU” style (single fill bar look) on one axis. |
| **Audio Sine** | Wave3D / wave surface | We have waves. **Gap:** sine whose **amplitude/height** is **audio-driven** (height_mult). In 3D: wave surface where wave height = f(level). Good candidate. |
| **Audio Star** | — | **Gap:** star/burst with **edge beat** (transient lights up edges). In 3D: radial burst from origin; “edge” = far from origin; on onset, outer shell brightens. |
| **Audio Party** | — | **Gap:** segmented “zones” with threshold and motion/color ranges. In 3D: divide room into segments (e.g. by angle or by axis bands); each segment can have threshold and color. |
| **Audio Bubbles** | Bubbles3D (non-audio?) | They have **audio-triggered expanding circles** with presets. We have Bubbles3D; need to check if we have an audio-driven “bubble” that expands on level/onset. If not: **Audio Bubbles 3D** = expanding spheres from origin (or random 3D points), trigger by onset or level, size/thickness from level. |
| **Swirl Circles Audio** | — | **Gap:** rotating **circles** (swirl) with **audio level** driving brightness/size. In 3D: **Swirl Circles 3D** = one or more circles in a plane (e.g. XZ), rotating; radius or brightness = f(level). Could be a mode in PulseRing3D or new effect. |

---

## Part 3 – References: MEGA_CUBE and EFFECT_CONVERSION_GUIDE

### 3.1 MEGA_CUBE_ANIMATIONS_REFERENCE.md

- **Spectrum** – They have “WiFi spectrum analyser” (hardware). Not directly an audio effect; we do software FFT → our Spectrum Bars / Band Scan cover “spectrum” in 3D.
- **Layer sweep** – “One lit layer at a time”; we have **CubeLayer3D** (audio-driven layer position). Matches.
- **Fireworks** – Particle missile + explosion; we have Fireworks3D and Explosion3D. Audio-triggered explode is an option (e.g. onset triggers new explosion).
- **Plasma, Sinus, Helix, Cube, etc.** – Mapped to our 3D effects; audio can **modulate** speed or intensity (e.g. Cube Layer = audio layer, or wave speed = f(level)).

So: from Mega-Cube we already took layer sweep (Cube Layer); spectrum we do via FFT. Audio can drive **which layer** (Cube Layer) or **when** something triggers (e.g. explosion on beat).

### 3.2 EFFECT_CONVERSION_GUIDE.md

- **SwirlCircles** – Listed as “(new)” for 3D; conversion: radial swirl in XZ plane. **Swirl Circles Audio** in Effects Plugin = swirl + audio level → we can add **SwirlCircles3D** or **SwirlCirclesAudio3D**: circles in plane, rotation + radius/brightness from level.
- **Visor** – “(new)” beam sweeping in 3D; can be audio-driven (sweep speed or beam width from level).
- **Linear → 3D** – Map strip index to position; our path axis and zone/origin already support “this range only” and “from this origin.”

So: Swirl Circles (and optionally Visor) are explicit conversion targets; both can get an **audio** variant (level or onset).

---

## Part 4 – Coverage Checklist (Current)

### 4.1 Our audio UI

- [x] Start/Stop listening  
- [x] Level meter  
- [x] Input device list  
- [x] Gain (with sensible default 3.5x)  
- [x] Bands (8/16/32)  
- [x] FFT size (512–8192)  
- [x] Frequency range list (add/remove/duplicate)  
- [x] Per-range: name, enabled, low/high Hz, effect type, zone, origin  
- [x] Per-range effect settings (effect’s SetupCustomUI in freq_effect_settings_widget)  
- [x] **Per-range smoothing / sensitivity / attack / decay** – Exposed in Frequency Range details UI.
- [x] Save/load device, gain, bands, FFT, frequency_ranges in plugin settings  
- [x] Audio panel visibility when selected effect is Audio  

### 4.2 Our audio pipeline

- [x] Capture (WASAPI)  
- [x] Mono sum, RMS, gain  
- [x] Auto-level (peak/floor)  
- [x] FFT, window, log-spaced bands  
- [x] Smoothed max band normalization (no longer per-frame max crush)  
- [x] Bass/mid/treble, onset (flux), getBandEnergyHz, getBands, getSpectrumSnapshot  
- [x] Defaults: 8 bands, 512 FFT, gain 3.5x  

### 4.3 Our audio effects

- [x] Beat Pulse (onset + wave)  
- [x] Audio Level (fill surface)  
- [x] Audio Pulse (breathing)  
- [x] Disco Flash (onset blobs / sparkle)  
- [x] Freq Ripple (onset rings)  
- [x] Spectrum Bars (bands → bars)  
- [x] Band Scan (moving hot band)  
- [x] Freq Fill (VU-style fill)  
- [x] Cube Layer (audio-driven layer)  
- [x] Audio Container (placeholder)  

### 4.4 Gaps vs Effects Plugin (candidates for 3D)

- [ ] **Audio-driven sine wave** – Sine whose amplitude/height = f(level). (Their Audio Sine.)  
- [ ] **Audio Star** – Burst/star with **edge beat** (outer part lights on transient).  
- [ ] **Audio Bubbles (audio)** – Expanding spheres/circles triggered by level or onset; presets optional.  
- [ ] **Swirl Circles Audio 3D** – Rotating circles in a plane (e.g. XZ), radius/brightness from level.  
- [ ] **Audio Party–style** – Segmented zones with threshold; in 3D = segment room by angle/axis, apply threshold per segment.  
- [ ] **Roll / color spread** – Like Audio Sync roll modes (linear, radial, wave) for “how color moves along strip” in our Spectrum or fill effects.  

---

## Part 5 – How to Do Their Effects Better in 3D

### 5.1 Audio-driven sine (Audio Sine 3D)

- **Idea:** A wave surface (e.g. `y = A * sin(phase + k*x)` in local XZ) where **A (amplitude)** = f(smoothed level).  
- **Implementation:** Reuse wave math from Wave3D/WaveSurface3D; add a “height = level” mode: amplitude = GetBandEnergyHz or smoothed level. Optionally path axis and origin.  
- **Files:** New effect **AudioSine3D** or mode in **Wave3D** / **WaveSurface3D** (e.g. “Amplitude: Fixed / Audio”).

### 5.2 Audio Star 3D (edge beat)

- **Idea:** Radial burst from origin; “edge” = LEDs far from center. On **onset**, outer shell (or all) gets a brightness/color bump.  
- **Implementation:** distance_norm = distance / max_radius; on onset, add term like `edge_boost * exp(-(distance_norm - 1)^2 / sigma^2)` so edge lights up.  
- **Files:** New **AudioStar3D** or add “Edge beat” mode to **BeatPulse3D** or **AudioPulse3D**.

### 5.3 Audio Bubbles 3D

- **Idea:** Expanding spheres (or circles in a plane); each bubble has center, birth time, max_radius, decay. Trigger new bubble on **onset** or when level crosses threshold.  
- **Implementation:** List of bubbles (center, t0, strength); per LED, sum contribution from each bubble (distance to center vs current radius with thickness). Reuse DiscoFlash-style trigger (onset) or level threshold.  
- **Files:** New **AudioBubbles3D** or extend **Bubbles3D** with audio trigger and 3D centers.

### 5.4 Swirl Circles Audio 3D

- **Idea:** One or more circles in a plane (e.g. XZ); rotation angle = time * speed; radius or line brightness = f(level).  
- **Implementation:** In XZ: angle = atan2(z - oz, x - ox); circle_angle = angle - time * speed; distance from circle = fmod(circle_angle, 2*pi) → “distance to line”; brightness = level * falloff(distance_to_line). Optionally multiple concentric circles.  
- **Files:** New **SwirlCirclesAudio3D** or mode in **PulseRing3D** (“Swirl + level”).

### 5.5 Roll / color spread (like Audio Sync)

- **Idea:** For Spectrum Bars or fill: “roll” = how the color or bar pattern moves along the axis (linear, radial, wave).  
- **Implementation:** Add a “roll” or “phase shift” parameter: position_for_color = axis_pos + roll_phase (time or beat). Roll mode: 0 = none, 1 = linear time, 2 = radial (angle), 3 = wave (sin).  
- **Files:** **SpectrumBars3D** or **FreqFill3D**: add roll_mode and phase offset in ComposeColor / fill logic.

### 5.6 Per-range attack/sensitivity UI

- **Idea:** Expose smoothing, sensitivity, attack, decay in the Frequency Range details so users don’t need to edit JSON.  
- **Implementation:** In **OpenRGB3DSpatialTab_FreqRanges.cpp**, in the details layout (e.g. after Origin), add four sliders bound to range->smoothing, range->sensitivity, range->attack, range->decay; save in SaveToJSON (already there).  
- **Files:** **ui/OpenRGB3DSpatialTab_FreqRanges.cpp** (and optionally a small “Advanced” group).

---

## Part 6 – Summary Table (Our vs Their Audio)

| Aspect | Us | Them |
|--------|-----|------|
| **Capture** | WASAPI, variable chunks, mono, high quality | WASAPI/OpenAL, fixed 512 samples |
| **FFT** | 512–8192, configurable; 8/16/32 log-spaced bands | Fixed 256 bins; 16-band EQ |
| **Level** | RMS × gain, auto-level, EMA | Amplitude × 100, no auto-level |
| **Bands** | Smoothed max normalization | Peak-hold + decay per bin |
| **Onset** | Spectral flux, log, smoothed | Not exposed separately |
| **UI** | One “Audio” panel (device, gain, bands, FFT) + frequency ranges (list + per-range effect/zone/origin) | Global audio settings (device, amplitude, etc.); each effect has “audio settings” button |
| **Effect model** | 3D grid: CalculateColorGrid(x,y,z,time,grid); can use in main stack or per frequency range | 2D zones: StepEffect(zones), SetLED by index |
| **Multi-band** | Multiple frequency ranges, each with its own effect + zone + origin | One effect per zone; effect sees full FFT |
| **Their effects we don’t have (as named)** | — | Audio Sine (height), Audio Star (edge beat), Audio Bubbles (expanding circles), Swirl Circles Audio, Audio Party (divisions) |
| **Our unique 3D** | Cube Layer, 3D fill/level, radial pulse, 3D disco flash, 3D ripple, zone/origin per range | — |

---

## Part 7 – Recommended Next Steps

1. **Effects polish first:** fix weak/flat visuals, tune decay/sensitivity defaults, and resolve effect-specific rendering issues.  
2. **Then add concepts:** implement one or two of: **Audio Sine 3D**, **Swirl Circles Audio 3D**, **Audio Star 3D**, **Audio Bubbles 3D**.  
3. **Spectrum polish:** add “roll” or “phase spread” option to Spectrum Bars or Freq Fill for stronger movement feel.  
4. **Keep docs lean:** use this file plus `AUDIO_EFFECTS_REWORK.md` as the active references.

All of the above keeps our 3D model (grid, zones, origins, path axis) and our capture/FFT pipeline; we only add or refine effects and UI so their best ideas work in our space.

---

## Part 8 – Audio UI Deep Dive (EQ, Analysers, Hz Range Picking)

Focused comparison of **their** audio UI (EQ, analysers, band-pass / Hz range) with **ours**, and concrete ways to improve our UI.

### 8.1 Their global Audio Settings (dialog)

- **Entry:** Each audio effect has an **“Audio settings”** button that opens a single shared **AudioSettings** dialog (modal: false).
- **Layout (AudioSettings.ui):**
  - **Capture settings** frame (grid):
    - **AudioDevice** – combo of devices from `AudioManager::GetAudioDevices()`.
    - **Amplitude** – spinbox (0–1e9), stored as `amplitude` (default 100); acts as global gain.
    - **Decay (% per step)** – spinbox 0–100; peak-hold decay.
    - **Average size** – spinbox 1–256; FFT bin averaging.
    - **Filter constant** – double spin; smoothing for filtered FFT.
    - **Normalization offset / scale** – double spinboxes; `nrml_ofst`, `nrml_scl` for per-bin normalization curve.
    - **Average mode** – combo: “Binning” / “Low Pass”.
    - **FFT Window mode** – combo: “None”, “Hanning”, “Hamming”, “Blackman”.
  - **Restore default** – resets all capture settings (not EQ).
  - **Equalizer** section:
    - **16 vertical sliders** (`eq_01`–`eq_16`), each 0–200 (stored as ×0.01 → 0–2.0 multiplier).
    - Sliders are **QTooltipedSlider** (value shown in tooltip while dragging).
    - **Reset EQ** – restores all 16 bands to 1.0.
  - EQ bands map to FFT bins: `equalizer[i/16]` for bin `i` (0–255), so 16 bands over 256 bins (fixed mapping; no Hz labels in UI).
- **Takeaways:** One place for device + gain + FFT options + **16-band graphic EQ**; separate “Restore default” and “Reset EQ”; tooltip sliders for numeric feedback.

### 8.2 Their per-effect UI: preview + band-pass (Hz range)

- **Audio Sync (AudioSync.ui):**
  - **Preview** – `QLabel` at top; minimum height implicit from content. The effect draws a **256×64 spectrum graph** (one column per FFT bin, height = magnitude). The **band-pass range** is shown visually: saturation/value = 255 inside the range, 128 outside. Updated every frame via `UpdateGraphSignal(QPixmap)`.
  - **Band-pass filter** – single **ctkRangeSlider** (dual-thumb range slider):
    - Range 0–256 (FFT **bin indices**, not Hz).
    - `bypass_min` / `bypass_max` (saved in effect settings); `on_bypass_valuesChanged(int min, int max)`.
  - So: user picks “which part of the spectrum” with **one control** (low and high in one slider), and **sees the spectrum** plus the selected range in the preview.
- **Audio Visualizer (AudioVisualizer.ui):**
  - **Preview** – `QLabel` at top; the effect draws the full visualizer (bar graph + foreground/background patterns) into it. Acts as a **live analyser + effect preview**.
- **Audio Party, etc.** – “Audio settings” button + sometimes a preview label; no band-pass in those UIs.
- **ctkRangeSlider** (Dependencies/ctkrangeslider): `setMinimumValue` / `setMaximumValue`, `valuesChanged(int min, int max)`; range is integer (they use 0–256 for bins).

### 8.3 Our audio UI (current)

- **Audio Input panel (OpenRGB3DSpatialTab_Audio.cpp):**
  - Start/Stop, **Level** (progress bar 0–1000 from RMS).
  - **Input Device** combo.
  - **Gain** – single slider 1–100 (0.1–10×), value label (e.g. “3.5x”).
  - **Bands** combo (8 / 16 / 32).
  - **FFT Size** combo (512–8192).
  - No spectrum/analyser widget, no EQ, no “Restore default” for audio.
- **Frequency Range details (OpenRGB3DSpatialTab_FreqRanges.cpp):**
  - Per range: Name, Enabled, **Low Hz**, **High Hz** (two **separate** sliders, 20–20000 Hz), Effect, Zone, Origin, effect-specific settings.
  - **Low Hz** / **High Hz** are independent sliders with labels (e.g. “20 Hz”, “200 Hz”). No visual link between them; no live spectrum; no single “range” control.

### 8.4 Recommendations for our audio UI

| Idea | Their approach | Suggested for us |
|------|----------------|------------------|
| **Hz range picking** | One **range slider** (0–256 bins) with live **spectrum preview** showing the selected band. | **Option A:** Add a **dual-thumb range slider** for “Low Hz” / “High Hz” (one widget, min/max in Hz). **Option B:** Keep two sliders but add a **live spectrum bar** above them (e.g. 64–256px wide) with a highlighted band between low/high Hz so users see what they’re selecting. **Option C:** Offer both: range slider in Hz + optional small spectrum strip with draggable band overlay. |
| **Live analyser** | Audio Sync: 256×64 spectrum in the effect UI. Audio Visualizer: full bar graph + pattern in preview. | Add a **small spectrum/level analyser** in the **Audio Input** panel (or next to it): e.g. bar graph of current bands or FFT bins, or a single “spectrum strip” (magnitude vs frequency). Could reuse our band data and optional `getSpectrumSnapshot()`; update on LevelUpdated or a dedicated timer. |
| **EQ** | 16-band graphic EQ in global Audio Settings (vertical sliders 0–200%, Reset EQ). | **Optional:** Add an **EQ section** in our Audio panel: e.g. 8 vertical sliders (one per band when Bands=8) or fixed 16 bands, each 0–200% gain. Apply in `AudioInputManager::computeSpectrum()` as per-band multipliers before or after band aggregation. “Reset EQ” button. |
| **Tooltip sliders** | QTooltipedSlider – value visible while dragging. | Use or implement a **tooltip-on-drag** for Gain and for Low/High Hz (and any new EQ sliders): show current value (e.g. “3.5x”, “120 Hz”) near the handle. |
| **Restore defaults** | “Restore default” (capture) and “Reset EQ” in Audio Settings. | **Restore default** button in Audio Input panel: reset device index, gain, bands, FFT to our defaults (e.g. gain 35, 8 bands, 512 FFT). Optionally “Reset EQ” if we add EQ. |
| **Per-range smoothing/sensitivity** | They don’t expose this per “range”; we have the data but no UI. | Already called out in Part 7: add **smoothing, sensitivity, attack, decay** sliders in Frequency Range details so the range’s response is tunable without JSON. |

### 8.5 Implementation notes (Hz range + analyser)

- **Range slider in Hz:** Our backend already uses **low_hz, high_hz**. A range slider can work in Hz (e.g. 20–20000) with log scale optional: store low_hz / high_hz; no need for bin indices in UI. If we don’t want a dependency on ctk, implement a simple dual-thumb slider (min/max value, same range as current sliders) or use a suitable Qt widget if available.
- **Spectrum strip in Audio panel:** In `on_audio_level_updated` (or a timer tied to running capture), fill a small pixmap from `getBands()` or `getSpectrumSnapshot()` and set it on a QLabel. For **Frequency Range** details, a second small strip could show the same spectrum with a **highlighted segment** between low_hz and high_hz (using our existing Hz→band or Hz→bin mapping).
- **EQ:** If we add it, add `float eq_per_band[MaxBands]` (or 16) to AudioInputManager, apply in `computeSpectrum()` after band computation: `newBands[i] *= eq_per_band[i]`. UI: one row of vertical sliders + “Reset EQ” in the Audio Input panel.

This keeps our 3D and pipeline; we only improve **how** users pick the Hz range and **how** they see and shape the audio (analyser, EQ, tooltips, defaults).
