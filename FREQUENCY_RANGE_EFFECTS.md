# Frequency Range Effects System

## Overview

The Frequency Range Effects system allows you to configure **independent audio-reactive effects** for specific frequency ranges. This enables sophisticated multi-band audio lighting setups, such as bass lights on the floor and treble sparkles on the ceiling.

## Concept

Each **Frequency Range** is similar to how **Display Planes** work in the ScreenMirror3D system:
- Each range is independently configurable
- Each has its own effect, spatial transform, and audio processing settings
- Multiple ranges can run simultaneously
- Ranges are saved/loaded with the plugin configuration

## Features Implemented (Phase 1: Core System)

### ✅ Data Structure
- `FrequencyRangeEffect3D` class with full configuration support
- JSON serialization for save/load
- Unique ID system for tracking ranges

### ✅ UI Components
- **Ranges List**: Add, remove, duplicate frequency ranges
- **Frequency Sliders**: Set low/high Hz cutoffs (20-20000 Hz)
- **Effect Selector**: Choose from available audio effects
- **Zone Selector**: Target specific zones/controllers or all controllers
- **Spatial Transform Controls**:
  - Position (X, Y, Z) for placing the effect in 3D space
  - Rotation (X, Y, Z) for orienting the effect
  - Scale (X, Y, Z) for sizing the effect
- **Audio Processing Controls**:
  - **Smoothing**: EMA smoothing factor (0-99%)
  - **Sensitivity**: Gain multiplier (0.1x-10.0x)
  - **Attack**: Rise speed (0.01-1.0)
  - **Decay**: Fall speed (0.01-1.0)

### ✅ Persistence
- Saves to plugin settings JSON
- Loads automatically on startup
- Preserves all configuration across sessions

## Usage Example

### Creating a Bass Floor Effect

1. **Add Range**: Click "Add Range"
2. **Configure**:
   - Name: "Sub-Bass Floor"
   - Low Hz: 20
   - High Hz: 60
   - Effect: Beat Pulse 3D
   - Zone: Floor/Skirting LEDs
   - Position: Y = -500mm (floor level)
   - Scale: 2.0x width, 0.5x height (wide & flat)
   - Smoothing: 80% (smooth bass response)
   - Sensitivity: 200% (emphasize bass)

### Creating a Treble Ceiling Effect

1. **Add Range**: Click "Add Range"
2. **Configure**:
   - Name: "Treble Sparkles"
   - Low Hz: 4000
   - High Hz: 16000
   - Effect: (TBD - will add sparkle effect)
   - Zone: Ceiling LEDs
   - Position: Y = 500mm (ceiling)
   - Scale: 2.0x width, 0.3x height
   - Smoothing: 50% (more reactive)
   - Sensitivity: 100% (normal)

## Implementation Files

### Core
- `FrequencyRangeEffect3D.h` - Data structure and serialization
- `ui/OpenRGB3DSpatialTab_FreqRanges.cpp` - UI implementation
- `ui/OpenRGB3DSpatialTab.h` - Header with declarations

### Integration Points
- `ui/OpenRGB3DSpatialTab_Audio.cpp` - Integrated into Audio tab

## Next Steps (Phase 2-5)

### Phase 2: Effect Settings UI (Dynamic)
- Add dynamic effect settings panel based on selected effect
- Each effect can provide its own custom controls
- Settings are saved per-range

### Phase 3: Audio Level Extraction
- Implement `AudioInputManager::getBandEnergyHz(low, high)` function
- Extract frequency-specific audio energy levels
- Apply attack/decay envelope processing
- Apply smoothing and sensitivity scaling

### Phase 4: Multi-Range Rendering
- Create rendering loop that processes all enabled ranges
- Extract audio level for each range's frequency band
- Apply envelope and smoothing
- Instantiate and render effect for each range
- Blend multiple ranges properly

### Phase 5: Preview Cube Widget
- Add mini 3D OpenGL preview (similar to ScreenMirror3D's Ambilight preview)
- Show real-time effect visualization with live audio reactivity
- Interactive camera (drag to rotate, scroll to zoom)
- Audio level meter per range

## Technical Details

### Audio Processing Pipeline

For each frequency range:
```
1. Raw Audio Input (FFT bins)
   ↓
2. Extract Energy in [low_hz, high_hz]
   ↓
3. Apply Attack/Decay Envelope
   ↓
4. Apply EMA Smoothing
   ↓
5. Apply Sensitivity (Gain)
   ↓
6. Clamp to [0.0, 1.0]
   ↓
7. Feed to Effect Instance
   ↓
8. Render to Target Zone/Controllers
```

### Spatial Transform Application

Each effect instance receives the spatial transform from its range:
- **Position**: Offsets the effect's origin in 3D space
- **Rotation**: Rotates the effect pattern
- **Scale**: Scales the effect's spatial extent

This allows effects like "Bass Floor" to be positioned at floor level (Y=-500mm) and scaled wide (2.0x width) to cover the entire floor plane.

### Zone Targeting

Ranges can target:
- **All Controllers** (zone_index = -1)
- **Specific Zone** (zone_index >= 0)
- **Individual Controller** (zone_index <= -1000, encoded as `-(controller_index) - 1000`)

This matches the existing zone system used throughout the plugin.

## Benefits

1. **Frequency Isolation**: Bass and treble can react independently
2. **Spatial Separation**: Different frequencies can light different physical locations
3. **Effect Diversity**: Mix different effect types for different frequency bands
4. **Fine Control**: Per-range audio processing parameters
5. **Modular Design**: Add/remove/duplicate ranges as needed
6. **Persistent Configuration**: Save complex multi-band setups

## Future Enhancements

- Preset frequency range configurations (e.g., "DJ Setup", "Home Theater")
- Visual spectrum analyzer with band highlighting
- Audio monitoring/solo system (hear isolated frequency bands)
- FFT improvements (higher resolution, per-band normalization, Mel scale)
- Beat detection and onset triggers
- Crossfade between ranges for smooth transitions
- Real-time preview cube with audio visualization

---

## 🎉 Implementation Status

**Phase 1: Core System** ✅ **COMPLETE**
- Data structures with save/load
- UI with add/remove/duplicate
- Frequency sliders and zone targeting
- Spatial transform controls
- Audio processing parameters

**Phase 2: Dynamic Effect Settings** ✅ **COMPLETE**
- Effect-specific UI dynamically generated
- Settings saved per frequency range
- Hot-reload when switching effects

**Phase 3: Audio Level Extraction** ✅ **COMPLETE**
- `getBandEnergyHz()` extracts frequency-specific audio
- Attack/decay envelope processing
- EMA smoothing
- Sensitivity (gain) adjustment

**Phase 4: Multi-Range Rendering** ✅ **COMPLETE**
- Integrated into main effect render loop
- Per-range effect instantiation
- Spatial transform application
- Zone targeting (all/zone/controller)
- Additive blending with effect stack

**Phase 5: Preview Cube Widget** ⏳ **PENDING**
- Mini 3D preview (like ScreenMirror3D ambilight)
- Real-time audio visualization
- Interactive camera controls

---

**Status**: Phases 1-4 ✅ Complete - **SYSTEM IS FUNCTIONAL!**  
**Next**: Phase 5 (Preview Cube) - Optional visual enhancement
