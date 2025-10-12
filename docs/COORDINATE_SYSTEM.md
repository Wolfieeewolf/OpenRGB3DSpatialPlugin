# 3D Spatial Coordinate System and Grid Architecture

**Last Updated:** 2025-10-10
**Author:** Wolfie (with Claude Code assistance)

---

## Table of Contents

1. [Overview](#overview)
2. [Coordinate System](#coordinate-system)
3. [Grid System](#grid-system)
4. [Effect Normalization](#effect-normalization)
5. [Common Issues and Fixes](#common-issues-and-fixes)
6. [Implementation Details](#implementation-details)

---

## Overview

The OpenRGB 3D Spatial Plugin creates **true 3D room-scale effects** that work consistently across any room size and LED configuration. Effects calculate colors based on the **actual physical positions** of LEDs in 3D space (millimeters).

### Key Principles

1. **1:1 Scale Representation** - Viewport mirrors real-world room layout exactly
2. **Center-Origin Coordinate System** - Origin (0,0,0) is the dead center of the room
3. **Automatic Room Detection** - Grid bounds auto-calculate from LED positions
4. **Normalized Effect Calculations** - Effects adapt to room size for consistent visual results
5. **Physical Accuracy** - What you see in viewport matches real-world LEDs

---

## Coordinate System

### Origin Point

**Origin (0, 0, 0) = Dead center of the room in 3D space**

- NOT the floor center
- NOT a wall corner
- NOT the ceiling center
- **The geometric center of the entire room volume**

### Axis Definitions

- **X-axis**: Left (-) to Right (+)
  - Parallel to the front/rear walls
  - Example: Left wall at X = -1834mm, Right wall at X = +1834mm

- **Y-axis**: Front (-) to Rear (+)
  - Perpendicular to front/rear walls
  - Example: Front wall at Y = -1211mm, Rear wall at Y = +1211mm

- **Z-axis**: Floor (-) to Ceiling (+)
  - Vertical axis
  - Example: Floor at Z = -1361mm, Ceiling at Z = +1361mm

### Example Room

```
Room Dimensions:
- Width (X): 3668mm (left-right)
- Depth (Y): 2423mm (front-back)
- Height (Z): 2723mm (floor-ceiling)

Origin (0,0,0) at dead center:
- 1834mm from left/right walls
- 1211.5mm from front/rear walls
- 1361.5mm from floor/ceiling

Grid Bounds:
- X: -1834 to +1834 mm
- Y: -1211.5 to +1211.5 mm
- Z: -1361.5 to +1361.5 mm
```

### LED Positioning

LEDs are positioned using **real-world millimeter measurements** from the center origin:

```cpp
// Example: Keyboard on desk
// - 500mm to the right of center (positive X)
// - 200mm in front of center (negative Y)
// - 400mm below center/sitting height (negative Z)
keyboard_position = { 500.0f, -200.0f, -400.0f };

// Example: Ceiling LED strip at front-left corner
// - 1800mm left (negative X)
// - 1200mm front (negative Y)
// - 1350mm up at ceiling (positive Z)
ceiling_led_position = { -1800.0f, -1200.0f, 1350.0f };
```

---

## Grid System

### What is the Grid?

The **Grid Context** represents the **actual bounding box** of all LED positions in the room. It's calculated dynamically each frame by finding the min/max positions of all LEDs.

### GridContext3D Structure

```cpp
struct GridContext3D
{
    float min_x, max_x;   // Minimum and maximum X coordinates of all LEDs
    float min_y, max_y;   // Minimum and maximum Y coordinates of all LEDs
    float min_z, max_z;   // Minimum and maximum Z coordinates of all LEDs
    float width, height, depth;  // Calculated dimensions

    GridContext3D(float minX, float maxX, float minY, float maxY, float minZ, float maxZ)
        : min_x(minX), max_x(maxX), min_y(minY), max_y(maxY), min_z(minZ), max_z(maxZ)
    {
        width = max_x - min_x + 1.0f;
        height = max_y - min_y + 1.0f;
        depth = max_z - min_z + 1.0f;
    }
};
```

### Grid Calculation (Auto-Detection)

**Location:** `OpenRGB3DSpatialTab_EffectStackRender.cpp` lines 40-121

The grid bounds are calculated by:
1. Iterating through ALL controllers
2. Finding the MIN and MAX position for each axis
3. Creating a bounding box that encompasses all LEDs

```cpp
// Pseudo-code
for each controller:
    for each LED in controller:
        if LED.x < grid_min_x: grid_min_x = LED.x
        if LED.x > grid_max_x: grid_max_x = LED.x
        // Same for Y and Z

grid_context = GridContext3D(grid_min_x, grid_max_x, ...)
```

**Result:** Effects receive the **actual room dimensions** based on where you've placed LEDs, NOT arbitrary grid cell counts.

### Why Auto-Calculate?

1. **No manual configuration needed** - System adapts automatically
2. **Add/remove LEDs freely** - Grid updates automatically
3. **Consistent effects** - Patterns scale correctly to room size
4. **Future-proof** - Works with any room configuration

---

## Effect Normalization

### The Problem

Effects using **raw millimeter coordinates** produce inconsistent results:

```cpp
// BAD: Uses raw position in millimeters
float wave = sin(position * frequency);

// If position = 1834mm (right wall in 3.6m room):
wave = sin(1834 * 0.1) = sin(183.4)  // Way too many cycles!

// If position = 500mm (small 1m room right wall):
wave = sin(500 * 0.1) = sin(50)      // Too few cycles!
```

**Result:** Wave effect looks completely different in different room sizes!

### The Solution: Normalize to 0-1 Range

Effects should normalize coordinates using the grid context:

```cpp
// GOOD: Normalize position to 0-1 range within room bounds
float normalized_position = (position - grid.min_x) / grid.width;

// Now normalized_position is ALWAYS 0.0-1.0 regardless of room size!
// Left wall = 0.0, Right wall = 1.0

float wave = sin(normalized_position * frequency * TWO_PI);
```

### Implementation Pattern

**Every effect should override `CalculateColorGrid()` instead of just `CalculateColor()`:**

```cpp
RGBColor YourEffect::CalculateColorGrid(float x, float y, float z, float time,
                                        const GridContext3D& grid)
{
    Vector3D origin = GetEffectOrigin();
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    // Normalize coordinates to 0-1 based on grid bounds
    float norm_x = (x - grid.min_x) / grid.width;
    float norm_y = (y - grid.min_y) / grid.height;
    float norm_z = (z - grid.min_z) / grid.depth;

    // Select coordinate based on axis
    float position = 0.0f;
    switch(effect_axis)
    {
        case AXIS_X: position = norm_x; break;
        case AXIS_Y: position = norm_y; break;
        case AXIS_Z: position = norm_z; break;
        case AXIS_RADIAL:
            // For radial, normalize distance from origin
            float distance = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
            float max_distance = sqrt(grid.width*grid.width +
                                     grid.height*grid.height +
                                     grid.depth*grid.depth) / 2.0f;
            position = distance / max_distance;
            break;
    }

    // Now use normalized position (0-1) for calculations
    float progress = CalculateProgress(time);
    float freq = GetScaledFrequency();
    float value = sin(position * freq * TWO_PI - progress);

    // ... rest of effect calculation
}
```

---

## Common Issues and Fixes

### Issue 1: Effects Don't Appear on Some LEDs

**Symptom:** LEDs at certain positions stay black/don't show effect

**Cause:** Grid bounds culling was active (FIXED in latest version)

**Old Code (WRONG):**
```cpp
// Lines 173-176, 342-345 (OLD - REMOVED)
if(x >= grid_min_x && x <= grid_max_x &&
   y >= grid_min_y && y <= grid_max_y &&
   z >= grid_min_z && z <= grid_max_z)
{
    // Only calculate effect if LED within bounds
}
```

**Fix:** Removed bounds culling entirely. ALL LEDs render regardless of position.

### Issue 2: Wipe Effect Appears Instantly

**Symptom:** Wipe shows all LEDs at full brightness immediately

**Cause:** Hardcoded normalization to -100 to +100 range

**Old Code (WRONG):**
```cpp
// Wipe3D.cpp line 189-191 (OLD)
position = (position + 100.0f) / 200.0f;  // Assumes -100 to +100!
```

**Fix:** Normalize using actual grid bounds:
```cpp
// Normalize to 0-1 based on actual room dimensions
float position;
switch(effect_axis)
{
    case AXIS_X:
        position = (x - grid.min_x) / grid.width;
        break;
    // ... etc
}
```

### Issue 3: Wave Effect Has Too Many/Few Cycles

**Symptom:** Wave pattern density changes with room size

**Cause:** Using raw millimeter coordinates without normalization

**Old Code (WRONG):**
```cpp
// Wave3D.cpp line 216 (OLD)
float wave_value = sin(position * freq_scale - progress);
// If position = 1834mm, creates way too many cycles!
```

**Fix:** Normalize position first:
```cpp
// Normalize position to 0-1 range
float norm_position = (position - grid.min_x) / grid.width;
float wave_value = sin(norm_position * freq_scale * TWO_PI - progress);
```

### Issue 4: Radial Effects Don't Scale to Room

**Symptom:** Radial wave/pulse only covers small area in large rooms

**Cause:** Not accounting for room size when calculating radial distance

**Old Code (WRONG):**
```cpp
float distance = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
// Distance is in mm, not normalized!
```

**Fix:** Normalize distance to room diagonal:
```cpp
float distance = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);

// Calculate maximum possible distance (room diagonal / 2)
float max_distance = sqrt(grid.width*grid.width +
                         grid.height*grid.height +
                         grid.depth*grid.depth) / 2.0f;

float norm_distance = distance / max_distance;  // Now 0-1!
```

---

## Implementation Details

### Reference Points

Effects can originate from different points:

1. **Room Center (0,0,0)** - Default origin
2. **User Position** - Where the user's head is (set via reference points)
3. **Custom Points** - Future: Multiple reference points for multi-source effects

```cpp
Vector3D origin = GetEffectOrigin();  // Returns appropriate origin

// Calculate position relative to origin
float rel_x = x - origin.x;
float rel_y = y - origin.y;
float rel_z = z - origin.z;
```

### Effect Parameters

Effects use standardized parameter helpers:

```cpp
// Speed (0.0-1.0, quadratic curve for smooth control)
float speed = GetScaledSpeed();

// Frequency (0.0-1.0, quadratic curve)
float frequency = GetScaledFrequency();

// Size multiplier (0.1-2.0, linear)
float size = GetNormalizedSize();

// Scale/coverage (0.1-2.0, affects reach)
float scale = GetNormalizedScale();

// Animation progress (accounts for speed and reverse)
float progress = CalculateProgress(time);
```

### Directional Effects

For axis-based effects (wipes, directional waves):

```cpp
float position;
switch(effect_axis)
{
    case AXIS_X:  // Left to Right
        position = (x - grid.min_x) / grid.width;
        break;
    case AXIS_Y:  // Front to Back
        position = (y - grid.min_y) / grid.height;
        break;
    case AXIS_Z:  // Floor to Ceiling
        position = (z - grid.min_z) / grid.depth;
        break;
    case AXIS_RADIAL:  // Outward from origin
        float distance = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
        float max_dist = sqrt(grid.width*grid.width + grid.height*grid.height + grid.depth*grid.depth) / 2.0f;
        position = distance / max_dist;
        break;
}

if(effect_reverse)
{
    position = 1.0f - position;  // Reverse direction
}
```

---

## Testing Checklist

When implementing/testing effects:

- [ ] Effect works in small room (1m x 1m x 1m)
- [ ] Effect works in large room (10m x 10m x 10m)
- [ ] Visual density/pattern is consistent across room sizes
- [ ] Wipe travels smoothly from one side to other
- [ ] Wave shows consistent number of cycles
- [ ] Radial effects expand to cover full room
- [ ] All LEDs receive effect (no black zones)
- [ ] Effect respects axis selection (X/Y/Z/Radial)
- [ ] Reverse direction works correctly
- [ ] Effect origins from user position when set

---

## Summary

**The Golden Rules:**

1. ✅ **Always use `CalculateColorGrid()` not just `CalculateColor()`**
2. ✅ **Always normalize coordinates using grid bounds**
3. ✅ **Never hardcode position ranges** (no `-100 to +100` assumptions)
4. ✅ **Test with multiple room sizes** to verify consistency
5. ✅ **Use relative positions from origin** for directional effects

**Effect Template:**

```cpp
RGBColor Effect::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    // 1. Get origin
    Vector3D origin = GetEffectOrigin();
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    // 2. Normalize to 0-1
    float norm_x = (x - grid.min_x) / grid.width;
    float norm_y = (y - grid.min_y) / grid.height;
    float norm_z = (z - grid.min_z) / grid.depth;

    // 3. Select position based on axis
    float position = /* select based on effect_axis */;

    // 4. Calculate effect value using normalized position
    float progress = CalculateProgress(time);
    float value = /* your effect calculation */;

    // 5. Apply to color and return
    return final_color;
}
```

---

**Questions?** See `EFFECT_DEVELOPMENT.md` for detailed effect creation guide.

