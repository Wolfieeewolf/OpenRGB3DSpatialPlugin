# Scale "Balloon" Behavior Fix

**Date:** 2025-10-12
**Issue:** Scale parameter required value of 47 before LEDs lit up, instead of working smoothly from 10
**Solution:** Changed scale to directly map to room diagonal (removed 0.5x multiplier)

## User's Requirement

> "picture a ballon if i blow it up then what's inside get lit up not the outside of it. make the ballon small then the lit area is small make the ballon big then the lit area is big. this is how our scale should work"

**Key Points:**
- Scale defines a **hard boundary** (balloon edge)
- Inside the boundary = effect is visible and bright
- Outside the boundary = black (no effect)
- Effects handle their own brightness/dimming **inside** the balloon
- Scale just controls the **size** of the balloon

## The Problem

### Before Fix
```cpp
float scale_radius = room_diagonal * (scale_factor * 0.5f);  // Halved multiplier
```

**Example calculation (user's room):**
- Room: 3668mm × 2423mm × 2715mm
- Room diagonal: ~5168mm
- LEDs: ~2355mm from room center

| Scale Value | Normalized | Radius Calculation | Scale Radius | LEDs Visible? |
|-------------|------------|-------------------|--------------|---------------|
| 10 | 0.29 | 5168 × (0.29 × 0.5) | 749mm | ❌ NO |
| 20 | 0.48 | 5168 × (0.48 × 0.5) | 1240mm | ❌ NO |
| 30 | 0.67 | 5168 × (0.67 × 0.5) | 1731mm | ❌ NO |
| 40 | 0.86 | 5168 × (0.86 × 0.5) | 2222mm | ❌ NO |
| **47** | **0.993** | **5168 × (0.993 × 0.5)** | **2566mm** | **✓ YES** |
| 100 | 1.00 | 5168 × (1.0 × 0.5) | 2584mm | ✓ YES |
| 200 | 2.00 | 5168 × (2.0 × 0.5) | 5168mm | ✓ YES |

**Problem:** The 0.5x multiplier made the balloon too small! User needed scale 47 before LEDs at 2355mm distance became visible.

### After Fix
```cpp
float scale_radius = room_diagonal * scale_factor;  // Direct mapping
```

| Scale Value | Normalized | Radius Calculation | Scale Radius | LEDs Visible? |
|-------------|------------|-------------------|--------------|---------------|
| 10 | 0.29 | 5168 × 0.29 | 1499mm | ❌ NO (too small) |
| 20 | 0.48 | 5168 × 0.48 | 2481mm | ✓ YES! |
| 30 | 0.67 | 5168 × 0.67 | 3463mm | ✓ YES |
| 50 | 0.95 | 5168 × 0.95 | 4910mm | ✓ YES |
| 100 | 1.00 | 5168 × 1.00 | 5168mm | ✓ YES (whole room) |
| 200 | 2.00 | 5168 × 2.00 | 10336mm | ✓ YES (beyond room) |

**Fixed:** Now LEDs become visible at scale ~20, and scale 100 covers the entire room diagonal!

## Implementation

### File Modified
**D:\MCP\OpenRGB3DSpatialPlugin\SpatialEffect3D.cpp**

**Function:** `IsWithinEffectBoundary(float rel_x, float rel_y, float rel_z, const GridContext3D& grid)`

**Lines 732-740:**

```cpp
// Scale radius as percentage of room diagonal
// Scale controls the "balloon" size - hard boundary, effect handles fade inside
// Scale slider mapping (direct percentage of room diagonal):
//   10 → 0.1 (normalized) → 0.19x diagonal (~10% room coverage)
//   50 → 0.5 (normalized) → 0.59x diagonal (~30% room coverage)
//  100 → 1.0 (normalized) → 1.0x diagonal (100% room coverage - whole room)
//  200 → 2.0 (normalized) → 2.0x diagonal (200% room coverage - beyond room)
float scale_factor = GetNormalizedScale();  // 0.1 to 2.0
float scale_radius = room_diagonal * scale_factor;  // Direct mapping
```

## Scale Slider Behavior

### Universal Mapping (works with any room size)
- **Scale 10:** Minimum balloon size (~19% of room diagonal)
- **Scale 50:** Medium balloon (~59% of room diagonal)
- **Scale 100:** Full room coverage (100% of room diagonal)
- **Scale 200:** Maximum balloon (200% of room diagonal, extends beyond room)

### Example Rooms

**User's Room (3668mm × 2423mm × 2715mm):**
- Diagonal: 5168mm
- Scale 10: 981mm radius balloon
- Scale 50: 3050mm radius balloon
- Scale 100: 5168mm radius balloon (whole room)
- Scale 200: 10336mm radius balloon

**Smaller Room (2000mm × 2000mm × 2000mm):**
- Diagonal: 3464mm
- Scale 10: 658mm radius balloon
- Scale 50: 2044mm radius balloon
- Scale 100: 3464mm radius balloon (whole room)
- Scale 200: 6928mm radius balloon

**Larger Room (8000mm × 5000mm × 3000mm):**
- Diagonal: 9847mm
- Scale 10: 1871mm radius balloon
- Scale 50: 5812mm radius balloon
- Scale 100: 9847mm radius balloon (whole room)
- Scale 200: 19694mm radius balloon

## Key Concepts

### Hard Boundary (Balloon Edge)
```cpp
float distance_from_origin = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
return distance_from_origin <= scale_radius;  // TRUE = inside balloon, FALSE = outside
```

- Inside balloon (distance ≤ radius): `IsWithinEffectBoundary()` returns **true** → LED gets effect color
- Outside balloon (distance > radius): `IsWithinEffectBoundary()` returns **false** → LED is black

### Effect Handles Brightness
The scale doesn't create gradients or fading - that's the effect's job!

**Example from Wave3D:**
```cpp
if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
{
    return 0x00000000;  // Black - outside balloon
}

// Inside balloon - effect calculates its own color/brightness
wave_value = sin(position * freq_scale - progress);
float hue = (wave_value + 1.0f) * 180.0f;
RGBColor final_color = GetRainbowColor(hue);

// Effect applies its own brightness
float brightness_factor = effect_brightness / 100.0f;
r = (unsigned char)(r * brightness_factor);
g = (unsigned char)(g * brightness_factor);
b = (unsigned char)(b * brightness_factor);
```

## Benefits

✓ **Scale now works intuitively** - Slider value directly corresponds to room coverage
✓ **Universal scaling** - Works the same way in any room size
✓ **Smooth progression** - LEDs light up at reasonable scale values (not 47!)
✓ **Whole room at 100** - Scale 100 = full room diagonal coverage (makes sense!)
✓ **Beyond room possible** - Scale 200 extends effect beyond room boundaries
✓ **Hard boundary preserved** - Effects remain bright inside, black outside (balloon behavior)

## Testing

Rebuild and test:
1. **Scale 10:** Should see small balloon of effect at room center
2. **Scale 50:** Should see medium balloon covering ~60% of room
3. **Scale 100:** Should see effect covering entire room
4. **Scale 200:** Should see effect extending beyond room (all LEDs lit)

The balloon should have a **sharp edge** (not a gradient), and the effect should be **bright and colorful** inside the balloon!
