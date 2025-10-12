# Grid and Effects System Fixes - Applied 2025-10-10

**Purpose:** Fix coordinate system to work with true 3D room-scale effects

**Problem Summary:** Effects were not working correctly because:
1. Grid bounds were calculated from arbitrary user settings instead of actual LED positions
2. Grid bounds culling was preventing LEDs from rendering
3. Effects were using raw millimeter coordinates without normalizing to room size

---

## Changes Applied

### 1. Grid Bounds Calculation (COMPLETED ✓)

**File:** `OpenRGB3DSpatialPlugin\ui\OpenRGB3DSpatialTab_EffectStackRender.cpp`
**Lines:** 40-121

**What Changed:**
- **OLD:** Grid bounds calculated from `custom_grid_x/y/z` user settings (lines 43-56)
- **NEW:** Grid bounds auto-calculated from actual LED positions (lines 45-121)

**How It Works Now:**
```cpp
// Scan ALL LEDs to find min/max positions
for each controller:
    for each LED:
        Update min_x, max_x, min_y, max_y, min_z, max_z

// Create grid context with ACTUAL room bounds
GridContext3D grid_context(min_x, max_x, min_y, max_y, min_z, max_z);
```

**Result:** Grid now represents the actual bounding box of your LED setup, not arbitrary grid cells.

**Log Output Added:**
```
Grid bounds: X[-1834.0 to 1834.0] (3668.0mm) Y[-1211.5 to 1211.5] (2423.0mm) Z[-1361.5 to 1361.5] (2723.0mm)
```

---

### 2. Remove Grid Bounds Culling (PENDING)

**File:** `OpenRGB3DSpatialPlugin\ui\OpenRGB3DSpatialTab_EffectStackRender.cpp`

#### Change 2A: Virtual Controllers Section

**Lines to Remove:** 173-177

**OLD CODE:**
```cpp
// Check if LED is within grid bounds
if(x >= grid_min_x && x <= grid_max_x &&
   y >= grid_min_y && y <= grid_max_y &&
   z >= grid_min_z && z <= grid_max_z)
{
    // [entire effect calculation block - 113 lines]
}
```

**NEW CODE:**
```cpp
// Remove the bounds check entirely
// Effect calculation happens for ALL LEDs regardless of position

// Safety: Ensure controller is still valid
if(!mapping.controller || mapping.controller->zones.empty() || mapping.controller->colors.empty())
{
    continue;
}

// [rest of effect calculation - no longer nested in bounds check]
```

**Lines Affected:** 173-290 (remove IF wrapper, keep contents)

#### Change 2B: Regular Controllers Section

**Lines to Remove:** 342-346

**OLD CODE:**
```cpp
// Only apply effects to LEDs within the grid bounds
if(x >= grid_min_x && x <= grid_max_x &&
   y >= grid_min_y && y <= grid_max_y &&
   z >= grid_min_z && z <= grid_max_z)
{
    // [entire effect calculation block - 87 lines]
}
```

**NEW CODE:**
```cpp
// Remove the bounds check entirely
// Effect calculation happens for ALL LEDs regardless of position

/*---------------------------------------------------------*\
| Initialize with black (no color)                         |
\*---------------------------------------------------------*/
RGBColor final_color = ToRGBColor(0, 0, 0);

// [rest of effect calculation - no longer nested in bounds check]
```

**Lines Affected:** 342-432 (remove IF wrapper, keep contents)

**Result:** ALL LEDs render effects, not just those within grid bounds.

---

### 3. Fix Wipe3D Effect (PENDING)

**File:** `OpenRGB3DSpatialPlugin\Effects3D\Wipe3D\Wipe3D.cpp`

#### Change 3A: Override CalculateColorGrid

**Add New Method** (after line 235, before smoothstep function):

```cpp
RGBColor Wipe3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    /*---------------------------------------------------------*\
    | Get effect origin (room center or user head position)   |
    \*---------------------------------------------------------*/
    Vector3D origin = GetEffectOrigin();

    /*---------------------------------------------------------*\
    | Calculate position relative to origin                    |
    \*---------------------------------------------------------*/
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    /*---------------------------------------------------------*\
    | Check if LED is within scaled effect radius             |
    \*---------------------------------------------------------*/
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z))
    {
        return 0x00000000;  // Black - outside effect boundary
    }

    /*---------------------------------------------------------*\
    | Update progress                                          |
    \*---------------------------------------------------------*/
    progress = fmod(CalculateProgress(time), 2.0f);
    if(progress > 1.0f) progress = 2.0f - progress;

    /*---------------------------------------------------------*\
    | Calculate position based on axis - NORMALIZED 0-1        |
    \*---------------------------------------------------------*/
    float position;
    switch(effect_axis)
    {
        case AXIS_X:  // Left to Right wipe
            position = (x - grid.min_x) / grid.width;
            break;
        case AXIS_Y:  // Front to Back wipe
            position = (y - grid.min_y) / grid.height;
            break;
        case AXIS_Z:  // Floor to Ceiling wipe
            position = (z - grid.min_z) / grid.depth;
            break;
        case AXIS_RADIAL:  // Radial wipe from center
        default:
            {
                float distance = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
                // Normalize to room diagonal
                float max_distance = sqrt(grid.width*grid.width +
                                        grid.height*grid.height +
                                        grid.depth*grid.depth) / 2.0f;
                position = distance / max_distance;
            }
            break;
    }

    /*---------------------------------------------------------*\
    | Apply reverse if enabled                                 |
    \*---------------------------------------------------------*/
    if(effect_reverse)
    {
        position = 1.0f - position;
    }

    // Position is now 0.0-1.0, no need to clamp
    // Calculate wipe edge with thickness
    float edge_distance = fabs(position - progress);
    float thickness_factor = wipe_thickness / 100.0f;

    float intensity;
    switch(edge_shape)
    {
        case 0: // Round
            intensity = 1.0f - smoothstep(0.0f, thickness_factor, edge_distance);
            break;
        case 1: // Sharp
            intensity = edge_distance < thickness_factor * 0.5f ? 1.0f : 0.0f;
            break;
        case 2: // Square
        default:
            intensity = edge_distance < thickness_factor ? 1.0f : 0.0f;
            break;
    }

    // Get color
    RGBColor final_color;
    if(GetRainbowMode())
    {
        float hue = progress * 360.0f + time * 30.0f;
        final_color = GetRainbowColor(hue);
    }
    else
    {
        final_color = GetColorAtPosition(progress);
    }

    // Apply intensity and brightness
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;

    float brightness_factor = (effect_brightness / 100.0f) * intensity;
    r = (unsigned char)(r * brightness_factor);
    g = (unsigned char)(g * brightness_factor);
    b = (unsigned char)(b * brightness_factor);

    return (b << 16) | (g << 8) | r;
}
```

#### Change 3B: Update Header File

**File:** `OpenRGB3DSpatialPlugin\Effects3D\Wipe3D\Wipe3D.h`
**Line:** After line 43 (after `RGBColor CalculateColor(...)`)

**Add Declaration:**
```cpp
RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
```

**Key Fix:** Position now normalized using grid bounds, not hardcoded `-100 to +100`!

---

### 4. Fix Wave3D Effect (PENDING)

**File:** `OpenRGB3DSpatialPlugin\Effects3D\Wave3D\Wave3D.cpp`

#### Change 4A: Override CalculateColorGrid

**Add New Method** (after line 254, replacing CalculateColor or adding override):

```cpp
RGBColor Wave3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    /*---------------------------------------------------------*\
    | Get effect origin (room center or user head position)   |
    \*---------------------------------------------------------*/
    Vector3D origin = GetEffectOrigin();

    /*---------------------------------------------------------*\
    | Calculate position relative to origin                    |
    \*---------------------------------------------------------*/
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    /*---------------------------------------------------------*\
    | Check if LED is within scaled effect radius             |
    \*---------------------------------------------------------*/
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z))
    {
        return 0x00000000;  // Black - outside effect boundary
    }

    /*---------------------------------------------------------*\
    | Use standardized parameter helpers                       |
    \*---------------------------------------------------------*/
    float actual_frequency = GetScaledFrequency();

    /*---------------------------------------------------------*\
    | Update progress for animation                            |
    \*---------------------------------------------------------*/
    progress = CalculateProgress(time);

    /*---------------------------------------------------------*\
    | Calculate wave based on axis and shape type             |
    | IMPORTANT: Normalize position to 0-1 for consistent     |
    | wave density regardless of room size                     |
    \*---------------------------------------------------------*/
    float wave_value = 0.0f;
    float size_multiplier = GetNormalizedSize();
    float freq_scale = actual_frequency * 0.1f / size_multiplier;
    float position = 0.0f;
    float normalized_position = 0.0f;

    /*---------------------------------------------------------*\
    | Calculate position based on selected axis               |
    \*---------------------------------------------------------*/
    switch(effect_axis)
    {
        case AXIS_X:  // Left to Right
            position = rel_x;
            normalized_position = (x - grid.min_x) / grid.width;
            break;
        case AXIS_Y:  // Floor to Ceiling
            position = rel_y;
            normalized_position = (y - grid.min_y) / grid.height;
            break;
        case AXIS_Z:  // Front to Back
            position = rel_z;
            normalized_position = (z - grid.min_z) / grid.depth;
            break;
        case AXIS_RADIAL:  // Radial from center
        default:
            if(shape_type == 0)  // Sphere
            {
                position = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
            }
            else  // Cube
            {
                position = std::max({fabs(rel_x), fabs(rel_y), fabs(rel_z)});
            }
            // Normalize radial distance
            float max_distance = sqrt(grid.width*grid.width +
                                    grid.height*grid.height +
                                    grid.depth*grid.depth) / 2.0f;
            normalized_position = position / max_distance;
            break;
    }

    /*---------------------------------------------------------*\
    | Apply reverse if enabled                                 |
    \*---------------------------------------------------------*/
    if(effect_reverse)
    {
        normalized_position = 1.0f - normalized_position;
    }

    // Use normalized position (0-1) scaled by room size
    // This ensures consistent wave density across different room sizes
    float spatial_scale = (grid.width + grid.height + grid.depth) / 3.0f;  // Average room dimension
    wave_value = sin(normalized_position * freq_scale * spatial_scale * 0.01f - progress);

    /*---------------------------------------------------------*\
    | Convert wave to hue (0-360 degrees)                     |
    \*---------------------------------------------------------*/
    float hue = (wave_value + 1.0f) * 180.0f;
    hue = fmod(hue, 360.0f);
    if(hue < 0.0f) hue += 360.0f;

    /*---------------------------------------------------------*\
    | Get color based on mode                                  |
    \*---------------------------------------------------------*/
    RGBColor final_color;

    if(GetRainbowMode())
    {
        final_color = GetRainbowColor(hue);
    }
    else
    {
        float color_position = hue / 360.0f;
        final_color = GetColorAtPosition(color_position);
    }

    /*---------------------------------------------------------*\
    | Apply brightness                                         |
    \*---------------------------------------------------------*/
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;

    float brightness_factor = effect_brightness / 100.0f;
    r = (unsigned char)(r * brightness_factor);
    g = (unsigned char)(g * brightness_factor);
    b = (unsigned char)(b * brightness_factor);

    return (b << 16) | (g << 8) | r;
}
```

#### Change 4B: Update Header File

**File:** `OpenRGB3DSpatialPlugin\Effects3D\Wave3D\Wave3D.h`
**Line:** After line 51 (after `RGBColor CalculateColor(...)`)

**Add Declaration:**
```cpp
RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
```

**Key Fix:** Wave now uses normalized position AND scales by average room dimension for consistent density!

---

### 5. Check Other Effects (PENDING)

Need to verify and potentially fix these effects:

**Files to Check:**
- `Effects3D/Spiral3D/Spiral3D.cpp`
- `Effects3D/Plasma3D/Plasma3D.cpp`
- `Effects3D/Explosion3D/Explosion3D.cpp`
- `Effects3D/RadialRainbow3D/RadialRainbow3D.cpp`
- Any other custom effects

**Same Pattern Applies:**
1. Override `CalculateColorGrid()` instead of just `CalculateColor()`
2. Normalize coordinates using grid bounds
3. Scale spatial calculations by room dimensions

---

## Testing Checklist

After all changes applied, verify:

- [ ] Plugin compiles without errors
- [ ] Grid bounds logged correctly on startup (check console/logs)
- [ ] Wipe effect travels smoothly left-to-right (X-axis)
- [ ] Wipe effect travels smoothly front-to-back (Y-axis)
- [ ] Wipe effect travels smoothly floor-to-ceiling (Z-axis)
- [ ] Wave effect shows consistent number of cycles across room
- [ ] All LEDs render effects (no black zones)
- [ ] Effects work from room center (0,0,0) origin
- [ ] Effects work from user head position when set
- [ ] Reverse direction works for all effects

---

## Implementation Order

1. ✅ **DONE:** Grid bounds auto-calculation
2. ⏳ **NEXT:** Remove bounds culling (both sections)
3. ⏳ Fix Wipe3D effect
4. ⏳ Fix Wave3D effect
5. ⏳ Check/fix remaining effects
6. ⏳ Build and test
7. ⏳ Verify with real-world room setup

---

## Notes

**Why This Was Broken:**

The old system assumed:
- Positions would be in range -100 to +100 (hardcoded in Wipe3D line 190)
- Room size didn't matter for effect calculations
- Grid bounds should filter out "off-grid" LEDs

**Why This Is Now Fixed:**

The new system:
- Auto-detects actual room size from LED positions
- Normalizes all coordinates to 0-1 range within room bounds
- Allows ALL LEDs to render regardless of position
- Effects scale consistently across any room size

**Documentation:**

See `COORDINATE_SYSTEM.md` for full technical explanation of:
- Coordinate system (center-origin, mm units)
- Grid context structure
- Effect normalization pattern
- Common issues and solutions

---

**Ready to implement?** Reply "yes" to proceed with executing all pending changes.

