# FFT System Analysis & Improvement Plan

**Date**: 2026-01-27  
**Current State**: Working but needs improvements for precise frequency isolation

---

## Current Implementation Analysis

### FFT Pipeline:

```
Audio Capture (WASAPI)
    ↓
PCM16 → Float Conversion
    ↓
Mono Downmix (average all channels)
    ↓
Sample Buffer (fft_size samples)
    ↓
Hann Window Application
    ↓
Cooley-Tukey FFT
    ↓
Magnitude Spectrum (n/2 bins)
    ↓
Logarithmic Band Mapping
    ↓
EMA Smoothing
    ↓
Normalization (0..1)
    ↓
Bass/Mid/Treble Aggregation
```

---

## Current Settings

**Location**: `Audio/AudioInputManager.h` & `.cpp`

| Parameter | Current Value | Range | Notes |
|-----------|--------------|-------|-------|
| **FFT Size** | 1024 | 512-8192 | Power of 2 |
| **Sample Rate** | 48000 Hz | Variable | From audio device |
| **Bands** | 16 | 8/16/32 | User selectable |
| **Frequency Range** | ~46 Hz - 24000 Hz | Calculated | f_min = fs/fft_size, f_max = fs/2 |
| **Bass Crossover** | 200 Hz | User adjustable | Default |
| **Mid Crossover** | 2000 Hz | User adjustable | Default |
| **Window** | Hann | Fixed | Applied before FFT |
| **Smoothing** | 0.8 (EMA) | 0.0-0.99 | Applied to bands |
| **Overlap** | 50% | Fixed | fft_size/2 samples kept |

---

## Issues Identified

### 🔴 **Critical Issues:**

#### 1. **Poor Frequency Resolution at Low Frequencies**

**Problem**: With FFT size of 1024 @ 48kHz, bin resolution is:
```
Frequency Resolution = Sample Rate / FFT Size
                     = 48000 Hz / 1024
                     = 46.875 Hz per bin
```

**Impact**:
- **Bass (20-200 Hz)** only gets ~4 FFT bins (very coarse!)
- Cannot distinguish between kick drum (40-80 Hz) vs bass guitar (80-200 Hz)
- Impossible to isolate sub-bass (20-60 Hz) accurately

**Example**:
```
Bin 1:  46 Hz   ← Sub-bass
Bin 2:  93 Hz   ← Kick drum
Bin 3:  140 Hz  ← Bass guitar
Bin 4:  187 Hz  ← Upper bass
```

With only 4 bins for the entire bass range, effects will respond to everything as "bass" instead of specific frequency components.

---

#### 2. **Logarithmic Band Mapping Loses Precision**

**Current Code** (`AudioInputManager.cpp:731-748`):
```cpp
for(int b = 0; b < bands_count; b++)
{
    float t0 = (float)b / (float)bands_count;
    float t1 = (float)(b + 1) / (float)bands_count;
    float ratio = f_max / f_min;
    float fb0 = f_min * std::pow(ratio, t0);  // Log scale
    float fb1 = f_min * std::pow(ratio, t1);
    
    // Average all FFT bins in this range
    int i0 = (int)std::floor(fb0 * n2 / f_max);
    int i1 = (int)std::ceil (fb1 * n2 / f_max);
    
    // ... average mags[i0..i1]
}
```

**Problem**:
- Uses logarithmic spacing (good for perception)
- **But averages all FFT bins** in each band (loses detail)
- Low bands may span only 1-2 bins
- High bands may span hundreds of bins
- Averaging dilutes important peaks

---

#### 3. **Bass/Mid/Treble Crossovers Are Coarse**

**Current Code** (`AudioInputManager.cpp:772-802`):
```cpp
// Crossover uses band indices, not precise Hz
int b_end = (int)std::floor(bass_norm * bands_count);
int m_end = (int)std::floor(mid_norm * bands_count);

// Average all bands in range
for(int i=0;i<bands_count;i++)
{
    if(i < b_end) { bsum += bands16[i]; bc++; }      // Bass
    else if(i < m_end) { msum += bands16[i]; mc++; } // Mid
    else { tsum += bands16[i]; tc++; }               // Treble
}
bass_level = (bc? bsum/bc:0.0f);
```

**Problem**:
- With 16 bands across 20-24000 Hz:
  - Bass (20-200 Hz) = ~2-3 bands
  - Mids (200-2000 Hz) = ~4-5 bands  
  - Treble (2000-24000 Hz) = ~8-9 bands
- Very coarse separation
- Cannot isolate sub-bass, kick, snare, hi-hats independently

---

#### 4. **No Peak Detection**

**Missing**:
- Beat detection (transients)
- Percussive vs harmonic separation
- Attack/decay envelope tracking

**Impact**:
- Effects respond to average energy, not rhythmic events
- Kick drum has same response as sustained bass note
- Cannot trigger "burst" effects on percussive hits

---

#### 5. **Aggressive Normalization**

**Current Code** (`AudioInputManager.cpp:749-762`):
```cpp
// Find max band value
float maxv = 1e-6f;
for(int band_index = 0; band_index < (int)newBands.size(); band_index++)
{
    if(newBands[band_index] > maxv) maxv = newBands[band_index];
}

// Normalize all bands to 0..1
for(int band_index = 0; band_index < (int)newBands.size(); band_index++)
{
    newBands[band_index] = std::min(1.0f, newBands[band_index] / maxv);
}
```

**Problem**:
- Normalizes to **strongest band** = 1.0
- If treble is loud, bass gets boosted too (incorrect!)
- Loses relative energy information between frequencies
- Quiet sections get artificially amplified

---

### 🟡 **Moderate Issues:**

#### 6. **Fixed Hann Window**

- Hann window is good general-purpose
- But other windows better for different use cases:
  - **Hamming**: Better frequency resolution
  - **Blackman**: Lower sidelobes (cleaner spectrum)
  - **Rectangular**: Maximum temporal resolution (transient detection)

#### 7. **No DC Removal**

- Audio can have DC offset
- FFT bin 0 (DC) is skipped but offset affects other bins
- Should apply high-pass filter before FFT

#### 8. **Limited FFT Size Range**

- Max 8192 (frequency resolution = 5.86 Hz @ 48kHz)
- For precise low-frequency work, may need 16384 or 32768
- Trade-off: larger FFT = more latency

---

## Improvement Recommendations

### **Phase 1: Immediate Improvements** ⭐

#### **1.1: Increase Default FFT Size**
```cpp
// Change from:
int fft_size = 1024;  // 46.875 Hz resolution

// To:
int fft_size = 4096;  // 11.72 Hz resolution @ 48kHz
```

**Benefits**:
- 4x better frequency resolution
- Bass (20-200 Hz) now has ~16 bins instead of 4
- Can distinguish kick (40-80 Hz) from bass (80-200 Hz)

**Trade-off**:
- ~85ms latency @ 48kHz (acceptable for lighting)
- Slightly more CPU (negligible on modern hardware)

---

#### **1.2: Per-Band Normalization**
```cpp
// Instead of normalizing to max band:
for(int b = 0; b < bands_count; b++)
{
    // Apply per-band auto-ranging
    float& val = newBands[b];
    
    // Track min/max per band over time
    band_min[b] = 0.995f * band_min[b] + 0.005f * val;
    band_max[b] = 0.995f * band_max[b] + 0.005f * std::max(val, band_max[b]);
    
    // Normalize using per-band range
    float range = band_max[b] - band_min[b];
    if(range > 0.001f)
    {
        val = (val - band_min[b]) / range;
    }
}
```

**Benefits**:
- Each frequency band normalized independently
- Bass doesn't get boosted when treble is loud
- Maintains relative energy information

---

#### **1.3: Weighted Band Averaging**
```cpp
// Instead of simple average:
float accum = 0.0f; int cnt = 0;
for(int i = i0; i < i1; i++) {
    accum += mags[i];  // ❌ Simple average
    cnt++;
}

// Use weighted peak detection:
float accum = 0.0f; float weight_sum = 0.0f;
for(int i = i0; i < i1; i++) {
    float freq = i * (fs / fft_size);
    float weight = std::pow(mags[i], 2.0f);  // Energy weighting
    accum += mags[i] * weight;
    weight_sum += weight;
}
float band_value = (weight_sum > 0) ? (accum / weight_sum) : 0.0f;
```

**Benefits**:
- Emphasizes strong peaks over noise floor
- Better represents actual frequency content
- Less smearing from weak harmonics

---

### **Phase 2: Advanced Features**

#### **2.1: Multiple FFT Sizes (Multi-Resolution)**
```cpp
struct MultiResolutionFFT {
    int fft_size_low = 8192;   // 5.86 Hz - great for bass
    int fft_size_mid = 2048;   // 23.4 Hz - good for mids
    int fft_size_high = 512;   // 93.75 Hz - fast for treble
};
```

**Benefits**:
- Best resolution for each frequency range
- Low latency for high frequencies (transients)
- High precision for low frequencies (bass separation)

---

#### **2.2: Mel Scale / Bark Scale Mapping**
```cpp
// Instead of logarithmic, use perceptual scale
float hz_to_mel(float hz) {
    return 2595.0f * std::log10(1.0f + hz / 700.0f);
}

// Map bands using mel scale (human hearing response)
```

**Benefits**:
- Matches human frequency perception
- Better for musical applications
- More bands in critical mid-range

---

#### **2.3: Beat/Onset Detection**
```cpp
struct BeatDetector {
    float onset_threshold = 1.5;     // Spike above average
    float decay_rate = 0.95;         // Peak hold decay
    
    bool detectBeat(float current, float& running_avg, float& peak) {
        running_avg = 0.98f * running_avg + 0.02f * current;
        peak = std::max(peak * decay_rate, current);
        return (current > running_avg * onset_threshold && 
                current > peak * 0.8f);
    }
};
```

**Benefits**:
- Detect kick drum hits, snare hits, etc.
- Trigger burst effects on transients
- Separate percussive from sustained sounds

---

#### **2.4: Harmonic/Percussive Separation** (Advanced)
```cpp
// Decompose into:
// - Harmonic content (pitched sounds: bass, synths)
// - Percussive content (drums, hi-hats)

// Use median filtering in time/frequency domain
// Harmonics = stable over time (vertical filtering)
// Percussion = stable across frequencies (horizontal filtering)
```

**Benefits**:
- Bass effect responds only to bass guitar, not kick drum
- Percussion effects respond only to drums
- More precise musical reactivity

---

### **Phase 3: User-Configurable Bands**

#### **3.1: Custom Band Definitions**
```cpp
struct FrequencyBand {
    std::string name;      // "Sub-Bass", "Kick", "Snare", etc.
    float low_hz;
    float high_hz;
    float smoothing;       // Per-band smoothing
    float sensitivity;     // Per-band gain
    bool use_peak;         // Peak vs average
};

std::vector<FrequencyBand> user_bands = {
    {"Sub-Bass",   20,   60,  0.85, 1.5, true},   // Emphasize peaks
    {"Kick",       60,  120,  0.75, 2.0, true},   // Fast response
    {"Bass",      120,  250,  0.80, 1.2, false},  // Smooth
    {"Low-Mid",   250,  500,  0.75, 1.0, false},
    {"Mid",       500, 2000,  0.70, 1.0, false},
    {"Hi-Mid",   2000, 4000,  0.65, 0.8, false},
    {"Treble",   4000, 8000,  0.60, 1.0, true},
    {"Hi-Hat",   8000,16000,  0.50, 1.5, true},   // Very fast
};
```

**UI**:
```
┌─────────────────────────────────────────────┐
│ Custom Frequency Bands:                     │
├─────────────────────────────────────────────┤
│ [Sub-Bass] 20-60 Hz    Smooth:[====●===]   │
│ [Kick]     60-120 Hz   Smooth:[===●====]   │
│ [Bass]     120-250 Hz  Smooth:[====●===]   │
│ [Add Band] [Remove] [Presets▼]             │
└─────────────────────────────────────────────┘
```

---

## Implementation Roadmap

### **Step 1: Quick Wins** (1-2 hours) ⭐
1. Increase default FFT size to 4096
2. Add per-band normalization option
3. Implement weighted band averaging

### **Step 2: Core Improvements** (4-6 hours)
1. Multi-resolution FFT for bass/mid/treble
2. Improve crossover precision
3. Add beat/onset detection

### **Step 3: Advanced Features** (8-12 hours)
1. Custom user-defined frequency bands
2. Mel/Bark scale mapping option
3. Harmonic/percussive separation
4. Stereo channel analysis (L/R separation)

### **Step 4: Polish & UI** (4-6 hours)
1. Frequency band visualizer (spectrum analyzer)
2. Real-time parameter adjustment
3. Presets for different music genres
4. Per-band sensitivity controls

---

## Recommended Immediate Changes

### **Change 1: Increase FFT Size**
**File**: `Audio/AudioInputManager.h:149`
```cpp
// From:
int fft_size = 1024;

// To:
int fft_size = 4096;  // Better bass resolution
```

### **Change 2: Add Configurable Bands**
**File**: `Audio/AudioInputManager.h` (add):
```cpp
struct AudioBandConfig {
    float low_hz;
    float high_hz;
    float smoothing;  // Per-band EMA
    float gain;       // Per-band sensitivity
};
std::vector<AudioBandConfig> custom_bands;
```

### **Change 3: Expose Precise Band API**
**File**: `Audio/AudioInputManager.h` (add):
```cpp
// Get energy in exact Hz range (not pre-defined bands)
float getEnergyInRange(float low_hz, float high_hz, bool use_peak = false) const;

// Get multiple custom bands at once
std::vector<float> getCustomBands(const std::vector<AudioBandConfig>& bands) const;
```

---

## Testing Strategy

### Test Scenarios:

1. **Bass Separation Test**
   - Play kick drum (40-80 Hz)
   - Play bass guitar (80-200 Hz)
   - Verify effects respond independently

2. **Transient Response Test**
   - Play sharp snare hit
   - Verify quick attack and decay
   - Check latency acceptable

3. **Genre Tests**
   - EDM (strong bass, fast beats)
   - Rock (drums, guitars, vocals)
   - Classical (wide frequency range, dynamics)
   - Hip-hop (sub-bass, percussion)

---

## Performance Considerations

| FFT Size | Resolution @ 48kHz | Latency | CPU Impact |
|----------|-------------------|---------|------------|
| 512 | 93.75 Hz | ~10ms | Very Low |
| 1024 | 46.88 Hz | ~21ms | Low |
| 2048 | 23.44 Hz | ~43ms | Low |
| **4096** | **11.72 Hz** | **~85ms** | **Medium (Recommended)** |
| 8192 | 5.86 Hz | ~170ms | Medium-High |
| 16384 | 2.93 Hz | ~341ms | High |

**Recommended**: 4096 strikes best balance for musical lighting applications.

---

## Next Steps

**What do you want to prioritize?**

1. **Immediate**: Increase FFT size + per-band normalization (quick improvement)
2. **User Control**: Add custom frequency band definitions (flexible)
3. **Beat Detection**: Add onset/transient detection (rhythmic effects)
4. **Full Overhaul**: Implement multi-resolution FFT system (comprehensive)

Let me know which direction you want to go, and I'll implement it!
