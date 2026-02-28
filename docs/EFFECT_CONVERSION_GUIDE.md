# Effect Conversion Guide: 2D/1D → 3D Spatial

This guide explains how to convert effects from OpenRGB Effects, FastLED, WLED, and NightDriverStrip into 3D spatial effects for OpenRGB3DSpatialPlugin.

## Architecture Comparison

| Source | API | Position Model |
|--------|-----|----------------|
| **OpenRGB Effects** | `StepEffect(zones)` → `SetLED(LedID, color)` | Linear: LedID. Matrix: (col, row) |
| **FastLED** | `renderLed(index, point)` | 1D index or Point3D |
| **WLED/WS2812FX** | Per-LED index | 1D strip index |
| **OpenRGB3DSpatial** | `CalculateColorGrid(x, y, z, time, grid)` | 3D room coordinates |

## Conversion Strategies

### 1. Linear → 3D: Map strip index to position

For 1D effects (rainbow wave, chase, comet):
- **Original**: `color = f(LedID, time)` 
- **3D**: Choose an axis or path. `position = (x - min_x) / width` (or y, or z)
- **Example**: RainbowWave → use `(x + z) / (width + depth)` as position for diagonal wave

### 2. Matrix (col, row) → 3D: Project to plane

For 2D matrix effects (spiral, zigzag, radial):
- **Original**: `color = f(col, row, time)` with center (cx, cy)
- **3D**: Map (col, row) → (x, z) in room. Use `(x - origin.x) / half`, `(z - origin.z) / half` as normalized coords
- **Example**: Spiral → `angle = atan2(x-cx, z-cz)`, `r = sqrt((x-cx)² + (z-cz)²)` (already have Spiral3D)

### 3. Radial → 3D: Use distance + angle

For radial/circular effects:
- **Original**: `color = f(distance_from_center, angle, time)`
- **3D**: `r = sqrt(lx² + lz²)`, `angle = atan2(lz, lx)` where lx,lz are normalized coords
- **Example**: RadialRainbow → PulseRing3D, or radial gradient in XZ plane

### 4. Path-based → 3D: Assign path position

For snake/zigzag/marquee (LEDs along a path):
- **Original**: Path order = snake through grid. `path_pos = snake_index(col, row) / total`
- **3D**: Define path through room. For floor: discretize XZ, compute snake index, normalize. Each LED gets path_pos; if `progress > path_pos` light it.

## Conversion Checklist

1. **Create** `Effects3D/EffectName3D/EffectName3D.h` and `.cpp`
2. **Inherit** from `SpatialEffect3D`
3. **Implement** `CalculateColorGrid(x, y, z, time, grid)` – return color for that 3D point
4. **Map** 2D/1D logic to 3D:
   - `LedID` → position along axis or path
   - `(col, row)` → `(lx, lz)` normalized in XZ plane
   - `time` → `CalculateProgress(time)` or `time` directly
5. **Add** to `OpenRGB3DSpatialPlugin.pro` (HEADERS and SOURCES)
6. **Use** `GetRainbowColor()`, `GetColorAtPosition()`, `IsWithinEffectBoundary()`, `TransformPointByRotation()`

## Effect Mapping Table

| OpenRGB Effect | 3D Equivalent | Conversion Notes |
|----------------|---------------|------------------|
| RainbowWave | Wave3D | Position = axis or diagonal |
| Spiral | Spiral3D | ✓ Already exists |
| ZigZag | ZigZag3D | Snake path on floor (XZ) |
| RadialRainbow | PulseRing3D | ✓ Similar |
| Comet | Comet3D | ✓ Already exists |
| BouncingBall | BouncingBall3D | ✓ Already exists |
| Rain | (merged into Realtime Environment) | — |
| Lightning | SkyLightning3D | ✓ Sky Lightning; Plasma Ball = plasma |
| Bubbles | (new) | Rising spheres in 3D |
| SwirlCircles | (new) | Radial swirl in XZ plane |
| Visor | (new) | Beam sweeping in 3D |
| NoiseMap | Plasma3D | ✓ Similar |

## FastLED / WLED / NightDriver

- **FastLED examples**: Often use `sin/cos` of position + time. Map `coord1, coord2` to `(x,y)` or `(x,z)` in room.
- **WLED WS2812FX**: 1D index. Use `position = index/count` or map to room axis.
- **NightDriver**: Similar to FastLED; extract the `renderLed` math and replace coords with 3D position.
