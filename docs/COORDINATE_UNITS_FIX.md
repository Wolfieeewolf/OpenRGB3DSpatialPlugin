# Coordinate Units Mismatch Fix

**Date:** 2025-10-12
**Issue:** Effect appeared to originate from "way off to the right" instead of room center
**Root Cause:** GridContext3D used millimeters while LED world_position used grid units

## The Problem

After fixing the GridContext3D dimension swap and all axis mappings, the user reported:

> "no it's still have the effect coming from the very very far right way off the grid. is it a UI issue? is our grid not rendering in the right spot. something is not matching up"

The effect calculations were mathematically correct (effect origin at room center), but visually appeared WAY off to the right.

## Root Cause Analysis

There was a **fundamental coordinate system mismatch** between different parts of the code:

### 1. Grid Context (OpenRGB3DSpatialTab.cpp:1791-1795)

```cpp
// BEFORE (WRONG):
grid_max_x = manual_room_width;   // 3668 mm
grid_max_y = manual_room_depth;   // 2423 mm
grid_max_z = manual_room_height;  // 2715 mm
```

This created a GridContext3D with dimensions in **MILLIMETERS**:
- Room center calculated as: (1834mm, 1211.5mm, 1357.5mm)

### 2. LED World Positions (ControllerLayout3D.cpp:237-239)

```cpp
led_pos->world_position.x = rotated.x + ctrl_transform->transform.position.x;
led_pos->world_position.y = rotated.y + ctrl_transform->transform.position.y;
led_pos->world_position.z = rotated.z + ctrl_transform->transform.position.z;
```

Where `ctrl_transform->transform.position` comes from the viewport in **GRID UNITS**:
- Example controller position: (50.0, 38.4, 18.5) grid units
- Example LED position: (30.0, 20.0, 2.5) grid units

### 3. Viewport Rendering (LEDViewport3D.cpp:649-650, 855-857)

```cpp
float max_x = use_manual_room_dimensions ? (room_width / 10.0f) : (float)grid_x;
float max_y = use_manual_room_dimensions ? (room_depth / 10.0f) : (float)grid_y;
float max_z = use_manual_room_dimensions ? (room_height / 10.0f) : (float)grid_z;
```

The viewport divides millimeters by 10 to get **GRID UNITS** (1 grid unit = 10mm).

### 4. Effect Calculation (OpenRGB3DSpatialTab.cpp:2024-2045)

```cpp
// LED position in GRID UNITS
float x = transform->led_positions[mapping_idx].world_position.x;  // e.g., 30.0
float y = transform->led_positions[mapping_idx].world_position.y;  // e.g., 20.0
float z = transform->led_positions[mapping_idx].world_position.z;  // e.g., 2.5

// Effect origin calculated from grid_context (was in MILLIMETERS!)
Vector3D origin = GetEffectOriginGrid(grid);  // Returns (1834, 1211.5, 1357.5) ❌
```

## The Mismatch

**LED at (30.0, 20.0, 2.5) grid units** was being compared to **effect origin at (1834, 1211.5, 1357.5) millimeters!**

This caused effects to appear at completely wrong positions:
- The effect thought the room center was at (1834, 1211.5, 1357.5)
- But LEDs were only at positions like (30, 20, 2.5)
- So effects appeared to come from EXTREMELY far right (1834 units away from LEDs at position 30!)

## The Fix

**File:** `D:\MCP\OpenRGB3DSpatialPlugin\ui\OpenRGB3DSpatialTab.cpp` (lines 1792-1797)

Convert manual room dimensions from millimeters to grid units BEFORE creating GridContext3D:

```cpp
// AFTER (CORRECT):
grid_min_x = 0.0f;
grid_max_x = manual_room_width / 10.0f;  // Convert mm to grid units
grid_min_y = 0.0f;
grid_max_y = manual_room_depth / 10.0f;  // Convert mm to grid units
grid_min_z = 0.0f;
grid_max_z = manual_room_height / 10.0f; // Convert mm to grid units
```

**User's room (3668mm × 2423mm × 2715mm):**

Before fix:
- Grid context: (0, 3668) × (0, 2423) × (0, 2715) mm
- Room center: (1834, 1211.5, 1357.5) mm ❌
- LED positions: (30, 20, 2.5) grid units ❌
- **MASSIVE MISMATCH!**

After fix:
- Grid context: (0, 366.8) × (0, 242.3) × (0, 271.5) grid units
- Room center: (183.4, 121.15, 135.75) grid units ✓
- LED positions: (30, 20, 2.5) grid units ✓
- **SAME COORDINATE SYSTEM!**

## Impact

Now the effect calculations use the same coordinate system as LED positions:
- Effect origin at room center: (183.4, 121.15, 135.75) grid units
- LED at (30.0, 20.0, 2.5) grid units
- Distance calculation: √((30-183.4)² + (20-121.15)² + (2.5-135.75)²) = correct!

## Visual Verification

After this fix:
- Effects will appear to originate from the actual room center in the viewport
- Scale balloon will expand correctly from the visual center of the room
- All spatial calculations will match what the user sees visually

## Why This Was So Confusing

The previous dimensional fixes (GridContext3D bug, axis swaps) were all **mathematically correct** but used the wrong units. The debug logs showed:

```
[Wave3D] Grid: Width=3668.0 Depth=2423.0 Height=2715.0  ← In millimeters
[Wave3D] Grid center: X=1834.0 Y=1211.5 Z=1357.5        ← In millimeters
[Wave3D] Effect origin: X=1834.0 Y=1211.5 Z=1357.5      ← In millimeters
```

This looked "correct" numerically (proper center of 3668mm wide room), but was using the wrong coordinate system!

After this fix, the logs will show:

```
[Wave3D] Grid: Width=366.8 Depth=242.3 Height=271.5     ← In grid units
[Wave3D] Grid center: X=183.4 Y=121.15 Z=135.75         ← In grid units
[Wave3D] Effect origin: X=183.4 Y=121.15 Z=135.75       ← In grid units
```

Now the origin coordinates are in the same ballpark as LED positions (30, 20, 2.5), making spatial calculations work correctly!

## Reference Points

**Good news:** Reference points are already in the correct coordinate system!

Reference points are positioned using the viewport gizmo and stored directly as grid units:
- `VirtualReferencePoint3D::ToJson()` saves `transform.position.x/y/z` directly
- These come from viewport gizmo which uses grid units
- No conversion needed for reference points!

Example:
- User positioned at (30.0, 20.0, 2.5) in viewport → Stored as (30.0, 20.0, 2.5) grid units ✓
- Room center at (183.4, 121.15, 135.75) → Calculated correctly from grid context ✓

Both are in grid units, so effects work correctly when using custom reference points!

## Testing

After rebuilding with this fix, verify:

1. **Effect origin visual position (Room Center mode):**
   - Effects should appear to originate from the CENTER of the room grid (visually)
   - Not from way off to the right

2. **Effect origin visual position (Custom Reference Point):**
   - Position a reference point in the viewport (e.g., User at front-left)
   - Select that reference point as effect origin
   - Effects should originate from that visual position
   - Should match where you placed the reference point visually

3. **Scale balloon behavior:**
   - Scale 1: Tiny bubble at visual room center
   - Scale 50: Halfway to corners from visual center
   - Scale 100: Reaches all corners of visible room
   - Scale 200: Extends beyond visible room

4. **Axis effects:**
   - X-axis (left-to-right): Should sweep across room width visually
   - Y-axis (front-to-back): Should sweep from front to back visually
   - Z-axis (floor-to-ceiling): Should sweep from bottom to top visually

5. **Radial mode:**
   - Should expand outward from visual room center as a sphere/cube
   - Not from off-screen to the right

## Summary

✅ **Fixed coordinate system mismatch** - GridContext3D now uses grid units (not mm)
✅ **Effect origin now in correct coordinate space** - Matches LED positions
✅ **Visual appearance will match calculations** - Effects originate where expected
✅ **All spatial math works correctly** - Same units throughout

This was the FINAL piece needed to make the corner-origin coordinate system work correctly! 🎉

## Related Documentation

- `GRIDCONTEXT_DIMENSION_FIX.md` - Fixed width/height/depth swap in GridContext3D
- `SCALE_BALLOON_EXPLAINED.md` - Explains scale calculation (now with correct units!)
- `AXIS_MAPPING_FIX.md` - Fixed Y/Z axis swap in effects (now with correct units!)
- `COORDINATE_SYSTEM.md` - Overall coordinate system documentation
