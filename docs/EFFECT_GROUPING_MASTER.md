# Master Effect Grouping – WLED, FastLED, OpenRGB, NightDriver → 3D

Consolidation plan: group similar effects and add variant dropdowns to existing 3D effects.

---

## 1. WAVE FAMILY → WaveSurface3D + Wave3D

**Host effect:** WaveSurface3D (add variant dropdown)

| Source | Effect | Variant to add |
|--------|--------|----------------|
| OpenRGB | Wavy | Wavy (smooth sine) |
| OpenRGB | RainbowWave | Rainbow wave |
| OpenRGB | GradientWave | Gradient wave |
| OpenRGB | CustomGradientWave | Custom gradient |
| FastLED | Wave | Basic wave |
| FastLED | Wave2d | 2D wave |
| FastLED | Pacifica | Ocean waves |
| WLED | Rainbow | Rainbow cycle |
| WLED | Running Lights | Running wave |
| NightDriver | PatternWave | Wave |
| NightDriver | PatternSMRadialWave | Radial wave |

**WaveSurface3D variants:** Sinus (current), Radial, Linear, Pacifica, Gradient

---

## 2. FIRE FAMILY → SurfaceAmbient3D + Fireworks3D

**Host:** SurfaceAmbient3D – Fire, Water, Slime, Lava, Ember, Ocean, Steam ✓

| Source | Effect | Status |
|--------|--------|--------|
| WLED | Fire Flicker | SurfaceAmbient Fire |
| FastLED | Fire2012, Fire2023 | SurfaceAmbient Fire/Ember |
| NightDriver | FireEffect, PatternSMFire | SurfaceAmbient |
| OpenRGB | NoiseMap Lava | SurfaceAmbient Lava ✓ |
| OpenRGB | NoiseMap Ocean | SurfaceAmbient Ocean ✓ |
| 3D | Fireworks3D | Separate (explosions) |

**SurfaceAmbient3D styles:** Fire, Water, Slime, Lava, Ember (soft fire), Ocean (deep water), Steam

---

## 3. PLASMA / NOISE FAMILY → Plasma3D

**Host effect:** Plasma3D (add variant dropdown)

| Source | Effect | Variant |
|--------|--------|---------|
| OpenRGB | NoiseMap | Noise map |
| FastLED | Noise, NoisePlusPalette | Noise |
| WLED | (noise modes) | - |
| NightDriver | PatternSMNoise | Noise |
| esp-spatial-led | CubeFire | 3D sine spheres |
| 3D | Plasma3D | Classic, Swirl, Ripple, Organic |

**Plasma3D variants:** Classic, Swirl, Ripple, Organic, Noise, CubeFire (sine spheres)

---

## 4. SPIRAL / SWIRL FAMILY → Spiral3D

**Host effect:** Spiral3D (add variant dropdown)

| Source | Effect | Variant |
|--------|--------|---------|
| OpenRGB | Spiral | Spiral |
| OpenRGB | SwirlCircles | Swirl circles |
| OpenRGB | Hypnotoad | Hypnotic spiral |
| FastLED | - | - |
| NightDriver | PatternSwirl | Swirl |
| NightDriver | PatternSMSpiroPulse | Spiro pulse |
| NightDriver | PatternSMTwister | Twister |
| 3D | Spiral3D | Smooth, Pinwheel, Sharp |

**Spiral3D variants:** Smooth, Pinwheel, Sharp, Swirl Circles, Hypnotic

---

## 5. COMET / METEOR / CHASE FAMILY → Comet3D + ZigZag3D

| Source | Effect | 3D Effect |
|--------|--------|-----------|
| OpenRGB | Comet | Comet3D ✓ |
| WLED | Comet | Comet3D ✓ |
| WLED | Chase (various) | Comet3D / ZigZag3D |
| WLED | Larson Scanner | Visor3D (new) |
| OpenRGB | ZigZag | ZigZag3D ✓ |
| OpenRGB | Visor | Visor3D (new) |
| OpenRGB | Marquee | ZigZag3D variant |
| NightDriver | MeteorEffect | Comet3D |

**Comet3D:** Already has axis. Add chase-style tail variants.
**ZigZag3D:** Add Marquee variant (simpler chase).

---

## 6. RAINBOW / RADIAL FAMILY → PulseRing3D, Wave3D, Spiral3D

| Source | Effect | 3D Effect |
|--------|--------|-----------|
| OpenRGB | RadialRainbow | PulseRing3D |
| OpenRGB | RotatingRainbow | Spiral3D / Spin3D |
| OpenRGB | DoubleRotatingRainbow | Spiral3D |
| OpenRGB | SpectrumCycling | Wave3D |
| OpenRGB | ColorWheel | - |
| WLED | Rainbow, Rainbow Cycle | Wave3D |
| NightDriver | PatternSMRainbowTunnel | PulseRing3D variant |

**PulseRing3D:** Add Radial Rainbow variant (static radial gradient).

---

## 7. BOUNCING BALL → BouncingBall3D ✓

Already exists.

---

## 8. RAIN → Realtime Environment

Rain is a weather toggle in Realtime Environment (Sunrise3D). No standalone Rain3D.

---

## 9. LIGHTNING → Sky Lightning (NEW) + Plasma Ball

- **Plasma Ball** (ex-Lightning3D): Plasma sphere effect, not sky lightning.
- **Sky Lightning**: New effect – real sky lightning flashes (planned).

---

## 10. STARFIELD / SPARKLE / TWINKLE → Starfield3D + DiscoFlash3D

| Source | Effect | 3D Effect |
|--------|--------|-----------|
| OpenRGB | StarryNight | Starfield3D |
| OpenRGB | SparkleFade | DiscoFlash3D |
| WLED | Twinkle, Sparkle | Starfield3D |
| FastLED | TwinkleFox | Starfield3D |
| 3D | Starfield3D | ✓ |
| 3D | DiscoFlash3D | ✓ |

**Starfield3D:** Add Twinkle variant (faster twinkle).

---

## 11. BUBBLES → Bubbles3D (NEW)

| Source | Effect | Action |
|--------|--------|--------|
| OpenRGB | Bubbles | Convert to Bubbles3D |
| NightDriver | - | - |

**Create:** Bubbles3D – rising spheres in 3D.

---

## 12. BREATHING → BreathingSphere3D ✓

Already exists. Add Breathing (global pulse) variant.

---

## 13. SCANNER / VISOR → Visor3D (NEW)

| Source | Effect | Action |
|--------|--------|--------|
| OpenRGB | Visor | Convert to Visor3D |
| WLED | Larson Scanner | Same |
| FastLED | Cylon | Same |

**Create:** Visor3D – KITT-style sweeping beam.

---

## 14. WIPE / COLOR WIPE → Wipe3D ✓

Already exists.

---

## 15. AUDIO REACTIVE → Existing audio effects ✓

SpectrumBars3D, BeatPulse3D, BandScan3D, etc. – already comprehensive.

---

## Implementation Order

1. **WaveSurface3D** – Add variant dropdown (Sinus, Radial, Linear, Pacifica, Gradient) ✓ DONE
2. **Plasma3D** – Add variant dropdown (Noise, CubeFire) ✓ DONE
3. **Spiral3D** – Add variant dropdown (Swirl Circles, Hypnotic) ✓ DONE
4. **Bubbles3D** – New effect ✓ DONE
5. **Visor3D** – New effect (Larson/Visor) ✓ DONE
6. **Starfield3D** – Add Twinkle variant ✓ DONE
7. **Comet3D** – Add chase variants ✓ DONE
8. **PulseRing3D** – Add Radial Rainbow variant ✓ DONE
9. **ZigZag3D** – Add Marquee mode ✓ DONE
10. **BreathingSphere3D** – Add Global Pulse mode ✓ DONE
11. **Sunrise3D** – Realtime Environment (sky + weather toggles) ✓ DONE
12. **Sky Lightning** – New effect (real lightning flashes) ✓ DONE
