# Scale Balloon System - Complete Explanation

**Date:** 2025-10-12
**Issue Fixed:** Debug logging was calculating scale incorrectly (showed full diagonal instead of half-diagonal)

## What is the Scale "Balloon"?

The **Scale** slider controls a spherical "balloon" of effect coverage. Think of it as an invisible sphere centered at the effect origin (usually room center). LEDs inside this sphere show the effect, LEDs outside are BLACK (off).

This is a **hard boundary** - there's no fade or gradient at the edge. Inside = effect visible, outside = black.

## How Scale Percentage Works

The scale slider (1-200) directly maps to a percentage of the room's **half-diagonal distance**:

### What is "Half-Diagonal"?

The **half-diagonal** is the distance from the room center to any corner:

```
Room dimensions: Width × Depth × Height (in mm)
Half-diagonal = √((Width/2)² + (Depth/2)² + (Height/2)²)
```

For your room (3668mm × 2423mm × 2715mm):
```
Half-width  = 3668 / 2 = 1834mm
Half-depth  = 2423 / 2 = 1211.5mm
Half-height = 2715 / 2 = 1357.5mm

Half-diagonal = √(1834² + 1211.5² + 1357.5²)
              = √(3,364,356 + 1,467,732 + 1,842,807)
              = √6,674,895
              = 2584mm
```

### Scale Mapping

The scale slider maps **directly** to percentage of half-diagonal:

| Scale | Percentage | Radius (your room) | Coverage |
|-------|------------|-------------------|----------|
| **1** | 1% | 2584 × 0.01 = **25.84mm** | Tiny bubble at center |
| **10** | 10% | 2584 × 0.10 = **258.4mm** | Small sphere ~10" |
| **25** | 25% | 2584 × 0.25 = **646mm** | Quarter of diagonal |
| **50** | 50% | 2584 × 0.50 = **1292mm** | Half diagonal (~halfway to corners) |
| **92** | 92% | 2584 × 0.92 = **2377mm** | Just reaches your front-left-floor LED! |
| **100** | 100% | 2584 × 1.00 = **2584mm** | **Reaches ALL corners - whole room** |
| **150** | 150% | 2584 × 1.50 = **3876mm** | Extends beyond room |
| **200** | 200% | 2584 × 2.00 = **5168mm** | Extends far beyond room |

## Why Scale 100 = Whole Room

At **Scale 100**, the balloon radius equals the half-diagonal (2584mm), which is exactly the distance from room center to any corner. This means:

- The balloon touches all 8 corners of the room
- All LEDs in the room are inside the balloon
- The effect covers the entire room perfectly

This is why **100** is the "natural" full-room scale, not some arbitrary value!

## Your LED Example

Your front-left-floor LED at **(300.3, 20.0, 25.1)** is:
- **Distance from center:** 2355.2mm

This LED will be:
- **Scale 1-91:** Outside balloon → **BLACK**
- **Scale 92+:** Inside balloon → **COLORED** (shows effect)

## The Bug I Just Fixed

### The Problem

The debug logging in `Wave3D.cpp` was calculating the scale radius **INCORRECTLY**:

```cpp
// WRONG! (What the debug code was doing)
float room_diagonal = sqrt(grid.width² + grid.height² + grid.depth²);
float scale_radius = room_diagonal * scale_factor;
```

This used the **FULL diagonal** (corner to opposite corner through center), giving:
```
Full diagonal = √(3668² + 2423² + 2715²) = 5168mm
Scale 100 radius = 5168mm ❌ WRONG!
```

But the actual boundary check was correctly using **half-diagonal**:
```cpp
// CORRECT! (What IsWithinEffectBoundary() was doing)
float half_width = grid.width / 2.0f;
float half_depth = grid.depth / 2.0f;
float half_height = grid.height / 2.0f;
float max_distance_from_center = sqrt(half_width² + half_depth² + half_height²);
float scale_radius = max_distance_from_center * scale_percentage;
```

This correctly gave:
```
Half-diagonal = √(1834² + 1211.5² + 1357.5²) = 2584mm
Scale 100 radius = 2584mm ✓ CORRECT!
```

### The Impact

The **effect was working correctly**, but the debug logs were showing **wrong scale radius values**. This might have been confusing when trying to understand why LEDs were black or colored at certain scale values.

### The Fix

**File:** `D:\MCP\OpenRGB3DSpatialPlugin\Effects3D\Wave3D\Wave3D.cpp` (lines 299-310)

Changed the debug logging to calculate scale the **exact same way** as `IsWithinEffectBoundary()`:

```cpp
// Calculate scale radius the SAME way as IsWithinEffectBoundary() does
float half_width = grid.width / 2.0f;
float half_depth = grid.depth / 2.0f;
float half_height = grid.height / 2.0f;
float max_distance_from_center = sqrt(half_width * half_width +
                                     half_depth * half_depth +
                                     half_height * half_height);
float scale_percentage = effect_scale / 100.0f;
float scale_radius = max_distance_from_center * scale_percentage;

LOG_WARNING("[Wave3D] Scale: effect_scale=%u%% max_distance=%.1f scale_radius=%.1f",
           effect_scale, max_distance_from_center, scale_radius);
```

Now the debug logs will show:
```
[Wave3D] Scale: effect_scale=100% max_distance=2584.0 scale_radius=2584.0
```

Which correctly matches the actual boundary calculation!

## Testing After Fix

After rebuilding with this fix, you should see:

### At Scale 1:
```
[Wave3D] Scale: effect_scale=1% max_distance=2584.0 scale_radius=25.8
[Wave3D] Within boundary: NO
```
Your LED (2355.2mm away) is outside the 25.8mm bubble → BLACK ✓

### At Scale 50:
```
[Wave3D] Scale: effect_scale=50% max_distance=2584.0 scale_radius=1292.0
[Wave3D] Within boundary: NO
```
Your LED (2355.2mm away) is outside the 1292mm bubble → BLACK ✓

### At Scale 92:
```
[Wave3D] Scale: effect_scale=92% max_distance=2584.0 scale_radius=2377.3
[Wave3D] Within boundary: YES
```
Your LED (2355.2mm away) is **just inside** the 2377.3mm bubble → COLORED ✓

### At Scale 100:
```
[Wave3D] Scale: effect_scale=100% max_distance=2584.0 scale_radius=2584.0
[Wave3D] Within boundary: YES
```
Your LED (2355.2mm away) is inside the 2584mm bubble → COLORED ✓
**All room corners are also inside** → WHOLE ROOM COVERED ✓

### At Scale 200:
```
[Wave3D] Scale: effect_scale=200% max_distance=2584.0 scale_radius=5168.0
[Wave3D] Within boundary: YES
```
Your LED (2355.2mm away) is well inside the 5168mm bubble → COLORED ✓

## Summary

✅ **Effect calculations were already correct** - using half-diagonal
✅ **Scale 100 = whole room** - reaches all corners perfectly
✅ **Debug logging now matches reality** - fixed to use half-diagonal
✅ **Scale slider is intuitive** - 1% to 200% of room radius

The effect should now work exactly as expected, and the debug logs will accurately reflect what's happening! 🎉
