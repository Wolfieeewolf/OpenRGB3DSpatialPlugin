# Audio Pipeline Deep Dive – Why Sliders Need Cranking & FFT/Band Choices

Comparison with OpenRGB Effects Plugin and fixes so effects respond well at sensible defaults.

---

## 1. Capture: We’re in Good Shape

- **Our plugin:** WASAPI loopback/capture, float or PCM, mix to mono, feed into a single pipeline. Good quality, low lag.
- **Effects Plugin:** WASAPI (Windows) or OpenAL (Linux), fixed 512-sample buffer, amplitude applied in processor.
- **Conclusion:** Our capture method is fine. The issues are **gain staging**, **band normalization**, and **defaults**, not capture itself.

---

## 2. Why You Have to Crank the Sliders

### 2.1 Gain

- **Our UI:** Gain slider 1–100, displayed as “X.Xx”. Code: `gain = value / 10` clamped to 0.1–10. So **slider 10 → gain 1.0** (no amplification).
- **Effects Plugin:** `amplitude` default 100, applied to samples before FFT (`fft_tmp[i] *= settings->amplitude`). So they effectively start with 100% scale.
- **Result:** At default (slider 10, gain 1.0), loopback RMS is often 0.01–0.05. So `val = rms * 1` stays small; after auto-level you still sit in the bottom of the 0–1 range. Effects that multiply by level or threshold get almost nothing unless you crank gain (e.g. slider 70 → 7x) or other sliders.

**Fix:** Raise the **default gain** (e.g. default slider so gain is ~3–4x), and/or change the mapping so the middle of the slider is a sensible “1x” and higher values give more boost. See “Fixes” below.

**Low Windows volume (e.g. G560 at 20–30%):** If system volume is low, the captured signal is small. The UI gain slider now goes up to **50×** and gain is applied to the **samples** before FFT with **soft-limiting** (tanh). Default gain is **10×** (slider 100), similar to their amplitude 100.

**What the Effects Plugin does with level (that we now match):**
- **Pre-FFT boost:** They multiply every time-domain sample by `amplitude` (default **100**) before the FFT. So they effectively use 100× input gain by default. We now default to **10×** gain (slider 100) and allow up to 50×.
- **Magnitude curve:** They use **0.5×log10(1.1×fftmag) + 0.9×fftmag** (i.e. **90% linear, 10% log**). So low levels are not crushed—quiet music still produces visible bars. We used to use **log10(1+9×v)** (very compressive). We now use **0.4×log10(1+9×v) + 0.6×v** (60% linear, 40% log) so low levels show without maxing sliders.
- **No global renormalization:** They use **per-bin peak-hold + decay**; each bin can peak independently. We use **per-band peak** so bass, mids and treble can all peak.

### 2.2 Band Normalization (Per-Frame Max)

- **Our code:** In `computeSpectrum()` we build log-spaced bands, then:
  - `maxv = max(newBands[0..N-1])`
  - `newBands[i] /= maxv`
  So **every frame** the loudest band = 1.0 and all others are scaled down.
- **Effect:** If the mix is treble-heavy, bass bands are divided by that treble max and become tiny. So bass-heavy effects (Beat Pulse, bass range) get almost no signal even when bass is present. You compensate by cranking “sensitivity” or “peak boost” in the effect.
- **Effects Plugin:** They use **peak-hold + decay**: `if (fftmag > data.fft[i]) data.fft[i] = fftmag`, then each frame `data.fft[i] *= decay`. So each bin holds its peak and decays; no per-frame “renormalize so max = 1” across the spectrum. So multiple frequency regions can stay visible.

**Fix:** Don’t normalize bands by the current frame’s max. Use either:
- A **smoothed global max** (e.g. running max with decay), and normalize by that, or
- **Per-band peak-hold + decay** (like Effects Plugin), then optionally normalize by a smoothed max so overall scale is stable.

See “Fixes” below for a concrete approach (smoothed max).

### 2.3 Auto-Level

- **Our code:** `auto_level_peak` and `auto_level_floor` form a 0–1 range: `normalized = (val - floor) / (peak - floor)`. Floor/peak adapt to the signal.
- **Risk:** If the signal is very quiet, `peak` and `floor` can be very small and close; `min_range = 0.01` avoids divide-by-zero but the resulting 0–1 range can still be tiny in absolute input. So again, without enough gain upstream, “level” stays small.
- **Conclusion:** Fixing gain and band normalization is the main lever. Auto-level can stay as-is for now; we can tune min_peak/min_range later if needed.

---

## 3. Why 8 Bands + 512 FFT “Works” and Higher Doesn’t

### 3.1 FFT Size

- **512 FFT:** 256 frequency bins. At 48 kHz, bin width ≈ 93.75 Hz. We need 512 samples to run FFT; at 48 kHz that’s ~10.7 ms per FFT. Updates are frequent.
- **1024+ FFT:** We need more samples (e.g. 1024). So we accumulate longer before each FFT. Updates are less frequent (e.g. ~21 ms for 1024). Also, **frequency resolution** improves (e.g. 46.9 Hz per bin at 1024), so each bin is “narrower” and can be noisier on quiet or low-energy content.
- **Why “higher FFT = no effect” can happen:** (1) With larger FFT we might be clearing or not filling the buffer correctly after a config change (e.g. `setFFTSize` clears `sample_buffer`), so it can take a moment before the first FFT runs; (2) with 1024+, if band normalization is “divide by max”, and the max is in a few narrow bins, other bands (e.g. bass) can collapse to near zero more easily; (3) UI might be defaulting to 1024 while our internal default in code was 1024 – so “512 works” might mean “user selected 512”, which gives fewer, wider bands and more aggregation, so the band-max is often from a wider region and the crush is less extreme.

So the main issue isn’t “FFT size is wrong”, but **band normalization** and **gain**. 512 vs 1024 can still be documented as a trade-off: 512 = snappier, coarser; 1024 = smoother spectrum, needs more samples per update.

### 3.2 Band Count (8 vs 16 vs 32)

- **8 bands:** Fewer, wider log-spaced bands. More FFT bins per band → more averaging → more stable values. After “divide by max”, the 8 values still have a spread so some bands (e.g. bass) can sit at a reasonable fraction of max.
- **16/32 bands:** More, narrower bands. Each band can be noisier; and when we set “max band = 1.0” every frame, the others get scaled down more aggressively. So you see “only one or two bars” or “bass never moves” unless you crank sensitivity.
- **Conclusion:** Again, **per-frame max normalization** is the main culprit. With a smoothed max or peak-hold, 16 and 32 bands should behave better. We can still recommend “if in doubt, try 8 bands and 512 FFT” as a safe default until the normalization fix is in.

---

## 4. Comparison Summary (Effects Plugin vs Us)

| Aspect | OpenRGB Effects Plugin | Our Plugin (before fixes) |
|--------|------------------------|---------------------------|
| Capture | WASAPI 512 samples, amplitude ×100 default | WASAPI, variable chunks, gain default 1.0 |
| FFT | Fixed 256 (512 samples) | 512–8192, default 1024 |
| Bands | 256 bins, then binned/averaged; 16-band EQ | 8/16/32 log-spaced bands |
| Level / magnitude | Peak-hold + decay per bin; no per-frame “max=1” | Per-frame: bands /= max(bands) → one band=1, rest crushed |
| Normalization | nrml_ofst + nrml_scl curve on FFT | Auto-level on RMS; bands normalized by max |
| Result | Multiple frequency regions stay visible; amplitude gives obvious boost | Need to crank gain and effect sliders; many bands look dead |

---

## 5. Fixes (Implemented or Recommended)

### 5.1 Default Gain and Mapping

- **Option A:** Default slider position so that gain is ~3–4 (e.g. default slider 35–40 if gain = value/10).
- **Option B:** Change mapping: e.g. gain = 0.5 + (value/100)*9.5 so slider 50 ≈ 5x, 100 = 10x; default slider 50.
- **Recommended:** Raise default slider so that **default gain is 3.0–4.0** (e.g. slider 35 → 3.5). Keep mapping linear (value/10) for clarity. So in UI: default gain slider to 35 (or 30–40 range).

### 5.2 Band Normalization: Smoothed Max Instead of Per-Frame Max

- Add a **running max** for band normalization, with decay:
  - e.g. `band_max_smoothed = max(band_max_smoothed * decay, current_frame_max)` with decay ≈ 0.995–0.998.
  - Normalize: `newBands[i] /= max(band_max_smoothed, 1e-6f)`.
- So the scale doesn’t jump every frame to “loudest band = 1”; the smoothed max goes up on loud transients and decays slowly. Multiple bands can sit at a reasonable fraction of that scale. Optionally clamp normalized values to 1.0 so we don’t exceed 1.

### 5.3 FFT / Bands: Safe Defaults in UI and Docs

- **UI defaults:** Consider defaulting to **8 bands** and **512 FFT** in the Audio tab (or in the plugin’s saved settings) so that out-of-the-box behavior is “something moves” even before the normalization fix. After the fix, 16/32 and 1024 can be the default if desired.
- **Doc:** In this file (or a short user-facing note): “If effects are weak, try: Gain 3–5x, 8 bands, 512 FFT. After pipeline improvements, 16 bands and 1024 FFT should work well.”

### 5.4 Onset and Sensitivity

- Onset is derived from spectral flux; it’s already 0–1 after log and smoothing. If overall levels are small (due to gain/band normalization), effects that threshold onset (e.g. Disco Flash, Beat Pulse) will rarely trigger. Fixing gain and band normalization will make onset and level both sit in a better range so sensitivity sliders don’t need to be maxed.

---

## 6. Implementation Checklist

- [x] **Gain:** Default gain slider to 35 so default gain ≈ 3.5x; apply default to manager when no saved settings.
- [x] **Bands:** In `computeSpectrum()`, use smoothed running max (`band_max_smoothed`) with decay 0.996 instead of per-frame max; clamp band values to 0–1.
- [x] **UI defaults:** Default Audio tab to 8 bands and 512 FFT; manager defaults in `AudioInputManager.h` set to 8 bands and 512 FFT.
- [ ] **Optional:** Auto-level min_peak / min_range tuning if level bar still looks dead at default gain.

---

## 7. References

- Our capture: `Audio/AudioInputManager.cpp` (WASAPI, `FeedPCM16`, `processBuffer`, `computeSpectrum`).
- Our gain: `setGain`, `processBuffer` (val = rms * gain); UI `on_audio_gain_changed` (gain = value/10).
- Our bands: `computeSpectrum()` (log-spaced bands, then `newBands[i] /= maxv`).
- Effects Plugin: `Audio/AudioSignalProcessor.cpp` (amplitude, peak-hold, decay), `Audio/AudioManager.cpp` (capture), `Audio/AudioSettingsStruct.h` (amplitude 100, decay 80).
