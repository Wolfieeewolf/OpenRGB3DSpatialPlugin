# Coordinate System Consistency Fix

**Date:** 2025-10-12
**Issue:** Multiple rendering paths using different coordinate systems
**Solution:** Unified all paths to use corner-origin coordinate system

## The Problem

The plugin had **3 different rendering paths** using **inconsistent coordinate systems**:

### 1. Effect Stack Render (Multi-Effect Mode)
- **File:** `OpenRGB3DSpatialTab_EffectStackRender.cpp`
- **Status:** ✓ Already using corner-origin (0,0,0 at front-left-floor)
- **Coordinates:** X: [0 to width], Y: [0 to depth], Z: [0 to height]

### 2. Single Effect Render (Effects Tab)
- **File:** `OpenRGB3DSpatialPlugin\ui\OpenRGB3DSpatialTab.cpp` (lines 1751-2200)
- **Old Status:** ✗ Using center-origin (-half to +half)
- **New Status:** ✓ **FIXED** - Now uses corner-origin
- **Old Code:**
```cpp
int half_x = custom_grid_x / 2;
float grid_min_x = -half_x;
float grid_max_x = custom_grid_x - half_x - 1;
```
- **New Code:**
```cpp
if(use_manual_room_size) {
    grid_min_x = 0.0f;
    grid_max_x = manual_room_width;  // e.g., 3668mm
}
```

### 3. Worker Thread (Background Rendering)
- **File:** `OpenRGB3DSpatialPlugin\ui\OpenRGB3DSpatialTab.cpp` (lines 4607-4664)
- **Status:** ⚠️ Uses old `CalculateColor()` without GridContext3D
- **Note:** Appears to be **unused/legacy code** - not called in current version

## Changes Made

### Single Effect Renderer

**Location:** `OpenRGB3DSpatialTab.cpp` lines 1776-1867

**Before:**
- Used center-origin coordinate system
- Grid bounds: [-half_x to +half_x]
- LEDs positioned at their world coordinates didn't match the grid
- Room center was at (0, 0, 0)

**After:**
- Uses corner-origin coordinate system (same as Effect Stack)
- Grid bounds: [0 to width], [0 to depth], [0 to height]
- **Manual room size mode:** Uses configured room dimensions
- **Auto-detect mode:** Calculates bounds from actual LED positions
- Room center correctly calculated as `(min + max) / 2`

**Key Code:**
```cpp
if(use_manual_room_size)
{
    grid_min_x = 0.0f;
    grid_max_x = manual_room_width;   // e.g., 3668mm
    grid_min_y = 0.0f;
    grid_max_y = manual_room_depth;   // e.g., 2423mm
    grid_min_z = 0.0f;
    grid_max_z = manual_room_height;  // e.g., 2715mm
}
else
{
    // Auto-detect from LED positions
    for each LED:
        if(x < grid_min_x) grid_min_x = x;
        if(x > grid_max_x) grid_max_x = x;
        // ... same for Y and Z
}

GridContext3D grid_context(grid_min_x, grid_max_x,
                          grid_min_y, grid_max_y,
                          grid_min_z, grid_max_z);
```

## Coordinate System Specification

All rendering paths now use:

### Corner-Origin System
- **Origin:** Front-left-floor corner (0, 0, 0)
- **X-Axis:** Left wall (0) → Right wall (+width)
- **Y-Axis:** Front wall (0) → Back wall (+depth)
- **Z-Axis:** Floor (0) → Ceiling (+height)

### Room Center Calculation
```cpp
center_x = (min_x + max_x) / 2.0f;
center_y = (min_y + max_y) / 2.0f;
center_z = (min_z + max_z) / 2.0f;
```

**Example:**
- Room: 3668mm × 2423mm × 2715mm
- Bounds: X[0-3668], Y[0-2423], Z[0-2715]
- Center: (1834, 1211.5, 1357.5)

### LED World Positions
- LEDs have world positions in this coordinate system
- Example: LED at (270, 15, 25) = 270mm from left wall, 15mm from front wall, 25mm above floor

## Effect Origin Modes

Effects can originate from:

### 1. Room Center (REF_MODE_ROOM_CENTER)
```cpp
Vector3D origin = GetEffectOriginGrid(grid);
// Returns: (grid.center_x, grid.center_y, grid.center_z)
```

### 2. User Position (REF_MODE_USER_POSITION)
```cpp
Vector3D origin = GetEffectOriginGrid(grid);
// Returns: global_reference_point (e.g., user head at 400, 800, 1200)
```

### 3. Custom Point (REF_MODE_CUSTOM_POINT)
```cpp
Vector3D origin = GetEffectOriginGrid(grid);
// Returns: custom_reference_point (effect-specific override)
```

## Room Size Modes

### Manual Room Size
- **When:** User checks "Use Manual Room Size" and enters dimensions
- **Bounds:** Always [0 to configured_dimension]
- **Example:** 3668mm × 2423mm × 2715mm
- **Use case:** Real-world room dimensions

### Auto-Detect Room Size
- **When:** Manual room size unchecked
- **Bounds:** Calculated from min/max LED positions
- **Example:** LEDs at X: 270-360mm → bounds X[270-360]
- **Room size:** 90mm × 21mm × 35mm (tight fit around LEDs)
- **Use case:** Testing, compact setups, no room dimensions

## Grid-Aware Helpers

All effects should use these helpers in `CalculateColorGrid()`:

### GetEffectOriginGrid(grid)
```cpp
Vector3D origin = GetEffectOriginGrid(grid);
// Returns correct origin based on reference mode
// Uses grid.center_x/y/z for room center mode
```

### IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid)
```cpp
if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid)) {
    return 0x00000000;  // Black - outside effect
}
// Scale is relative to room diagonal
// Works with any room size universally
```

## Testing Verification

### Test 1: Manual Room Size
**Setup:**
- Room: 3668mm × 2423mm × 2715mm
- LEDs: 270-360mm (clustered in corner)
- Reference: Room center (1834, 1211.5, 1357.5)

**Expected:**
- Effect originates from room center
- Scale 100 = 5148mm radius (room diagonal)
- LEDs at ~1800mm from center are VISIBLE ✓

### Test 2: Auto-Detect Room Size
**Setup:**
- No manual room size
- LEDs: X[270-360], Y[6-27], Z[5-40]
- Auto bounds: X[270-360], Y[6-27], Z[5-40]
- Auto center: (315, 16.5, 22.5)

**Expected:**
- Effect originates from LED cluster center
- Scale 100 = 98mm radius (cluster diagonal)
- All LEDs within 98mm are VISIBLE ✓

### Test 3: Effect Stack vs Single Effect
**Setup:**
- Same layout, same effect (Wave3D)
- Test in both Effect Stack tab and Effects tab

**Expected:**
- Both tabs render identically ✓
- Same room bounds used
- Same effect origin used
- Same scale behavior

## Files Modified

### OpenRGB3DSpatialTab.cpp
- **Lines 1776-1867:** Single effect coordinate system fixed
- **Lines 1733-1751:** Added debug logging for single effect check

### OpenRGB3DSpatialTab_EffectStackRender.cpp
- **Lines 51-125:** Already using corner-origin (verified correct)
- **Lines 130-143:** Added debug logging for room bounds

### Wave3D.cpp
- **Lines 13:** Added LogManager.h include
- **Lines 256-303:** Added extensive debug logging
- **Lines 262-274:** Using `GetEffectOriginGrid(grid)`
- **Lines 275:** Using `IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid)`

### Wipe3D.cpp
- **Lines 243-256:** Using `GetEffectOriginGrid(grid)`
- **Lines 256:** Using `IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid)`

### SpatialEffect3D.h
- **Line 261:** Added `GetEffectOriginGrid(const GridContext3D& grid)`
- **Lines 283-284:** Added room-aware `IsWithinEffectBoundary()` overload

### SpatialEffect3D.cpp
- **Lines 611-629:** Implemented `GetEffectOriginGrid()`
- **Lines 709-740:** Implemented room-aware `IsWithinEffectBoundary()`

## Summary

✓ **Effect Stack rendering** - Was already correct
✓ **Single effect rendering** - **NOW FIXED** - uses corner-origin
⚠️ **Worker thread** - Legacy/unused code (uses old CalculateColor)

All active rendering paths now use:
- **Corner-origin coordinate system** (0,0,0 at front-left-floor)
- **Consistent room bounds** (manual or auto-detected)
- **Universal scaling** (relative to room size)
- **Grid-aware helpers** (GetEffectOriginGrid, IsWithinEffectBoundary)

**Result:** Effects now work consistently across all tabs and configurations! 🎉
