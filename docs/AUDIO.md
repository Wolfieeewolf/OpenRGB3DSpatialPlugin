# Audio

## Pipeline

| Piece | Location |
|-------|----------|
| Capture + FFT + bands | `Audio/AudioInputManager` (WASAPI on Windows) |
| Main panel | `ui/forms/AudioInputPanel.ui` + `OpenRGB3DSpatialTab_Audio.cpp` (visible when stack category **Audio** or audio layer selected) |
| Advanced analyzer | `ui/AudioAdvancedSettingsDialog` — **Advanced…** on audio panel |
| Effect helpers | `Effects3D/AudioReactiveCommon.h`, `Effects3D/AudioReactiveUi.h` |

**Defaults:** 8 log-spaced bands, 512-point FFT, input gain ~3.5×. Band levels use **smoothed running-max** normalization (not per-frame max crush).

## Main panel controls

- Device, mix mode, input gain, band count (8 / 16 / 32), FFT size.
- Bass / mid / high meters, optional stem toggles, 16-band EQ (rebuilds when band count changes).
- Spectrum preview: fixed **48px** height while listening.
- Start / stop capture tied to stack run state.

## Advanced analyzer (`Advanced…`)

Tuning is applied to `AudioInputManager` and persisted under **`3DSpatialPlugin`** in `OpenRGB.json` (see [PLUGIN_DATA_FOLDERS.md](PLUGIN_DATA_FOLDERS.md) for keys).

| Control area | Purpose |
|--------------|---------|
| Smoothing | EMA across frames / bands |
| Peak decay | Band, bass, activity, visualizer, auto-level peaks |
| Auto-level | Target and peak decay for loudness normalization |
| Crossovers | Bass / mid split frequencies |

**Restore defaults** in the dialog resets factory analyzer values.

## Effects (category Audio)

| Effect | Uses |
|--------|------|
| **Audio Pulse** | Beat/onset, full beat-wave section + response |
| **Disco Flash** | Beat flashes or sparkle mode (hides audio panels in sparkle; radius = common **Size**) |
| **Audio Level**, **Frequency Fill**, **Cube Layer** | `SampleAudioVisualLevel` + band/drive/response |
| **Band Scan**, **Spectrum Bars** | Raw `getBands()` in Hz range + response (no drive row) |
| **Audio Strip Visualizer** | Spectrum snapshot + EQ; best for **strip / zone** layouts |
| **Shader Field** | Optional audio uniforms to GLSL presets |

## Shared UI sections (`AudioReactiveUi`)

| Pattern | Effects |
|---------|---------|
| Band + response only | Band Scan, Spectrum Bars, Audio Strip Visualizer |
| Band + drive + response | Frequency Fill, Audio Level, Cube Layer |
| Full beat-wave + response | Audio Pulse |

Global **16-band EQ** is only on the Audio Input panel, not duplicated per effect.

## Tuning tips

- Weak bands: raise **Input gain**; start with 8 bands + 512 FFT before very large FFT.
- Missed beats: lower beat sensitivity or widen band preset.
- Room-filling vs strips: full-room layers (Audio Pulse, Disco Flash) vs **Audio Strip Visualizer** with path axis and zone targeting.

## Forward

- Preset stacks that pair strip visualizer + zone layouts.
- Optional in-panel summary when advanced settings differ from defaults.
