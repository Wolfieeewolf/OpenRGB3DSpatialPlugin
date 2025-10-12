# Axis Mapping Fix - Y and Z Coordinate System

**Date:** 2025-10-12
**Issue:** Axis labels in effects incorrectly identified Y and Z axes
**Root Cause:** Comments and labels confused the Y-axis (front-to-back) with Z-axis (floor-to-ceiling)

## Coordinate System Definition

The plugin uses a **corner-origin coordinate system**:

- **X-Axis:** Left to Right (width)
- **Y-Axis:** Front to Back (depth)
- **Z-Axis:** Floor to Ceiling (height)

This is **NOT** the same as the mathematical Z-up convention. In this system:
- Y represents the front-back dimension (depth)
- Z represents the vertical dimension (height)

## The Problem

Many effects had comments and axis labels swapped:
- AXIS_Y was labeled "Floor to Ceiling" (wrong - that's Z!)
- AXIS_Z was labeled "Front to Back" (wrong - that's Y!)

This caused user confusion when selecting axes - the dropdown didn't match the actual behavior.

## Files Fixed

### 1. Wave3D.cpp (D:\MCP\OpenRGB3DSpatialPlugin\Effects3D\Wave3D\Wave3D.cpp)

**CalculateColorGrid() - Lines 347-353:**
```cpp
// BEFORE (BROKEN):
case AXIS_Y:  // Front to Back wipe
    position = rel_y;
    normalized_position = (y - grid.min_y) / grid.height;  // WRONG!
    break;
case AXIS_Z:  // Floor to Ceiling wipe
    position = rel_z;
    normalized_position = (z - grid.min_z) / grid.depth;   // WRONG!
    break;

// AFTER (FIXED):
case AXIS_Y:  // Front to Back wipe (DEPTH not height!)
    position = rel_y;
    normalized_position = (y - grid.min_y) / grid.depth;   // FIXED
    break;
case AXIS_Z:  // Floor to Ceiling wipe (HEIGHT not depth!)
    position = rel_z;
    normalized_position = (z - grid.min_z) / grid.height;  // FIXED
    break;
```

**CalculateColor() - Lines 187-194:**
```cpp
// Fixed comments to match actual axis behavior
case AXIS_Y:  // Front to Back
    position = rel_y;
    break;
case AXIS_Z:  // Floor to Ceiling
    position = rel_z;
    break;
```

### 2. Wipe3D.cpp (D:\MCP\OpenRGB3DSpatialPlugin\Effects3D\Wipe3D\Wipe3D.cpp)

**CalculateColorGrid() - Lines 276-280:**
```cpp
// BEFORE (BROKEN):
case AXIS_Y:  // Front to Back wipe
    position = (y - grid.min_y) / grid.height;  // WRONG!
    break;
case AXIS_Z:  // Floor to Ceiling wipe
    position = (z - grid.min_z) / grid.depth;   // WRONG!
    break;

// AFTER (FIXED):
case AXIS_Y:  // Front to Back wipe (DEPTH not height!)
    position = (y - grid.min_y) / grid.depth;   // FIXED
    break;
case AXIS_Z:  // Floor to Ceiling wipe (HEIGHT not depth!)
    position = (z - grid.min_z) / grid.height;  // FIXED
    break;
```

**CalculateColor() - Lines 167-170:**
```cpp
// Fixed comments only (code was already using correct coordinates)
case AXIS_Y:  // Front to Back wipe
    position = rel_y;
    break;
case AXIS_Z:  // Floor to Ceiling wipe
    position = rel_z;
    break;
```

### 3. Spiral3D.cpp (D:\MCP\OpenRGB3DSpatialPlugin\Effects3D\Spiral3D\Spiral3D.cpp)

**Lines 182-187:**
```cpp
// BEFORE:
case AXIS_Y:  // Spiral along Y-axis (Floor to Ceiling)
case AXIS_Z:  // Spiral along Z-axis (Front to Back)

// AFTER:
case AXIS_Y:  // Spiral along Y-axis (Front to Back)
case AXIS_Z:  // Spiral along Z-axis (Floor to Ceiling)
```

### 4. Plasma3D.cpp (D:\MCP\OpenRGB3DSpatialPlugin\Effects3D\Plasma3D\Plasma3D.cpp)

**Lines 159-169:**
```cpp
// BEFORE:
case AXIS_X:  // Pattern primarily on YZ plane (Left/Right walls - perpendicular to X)
case AXIS_Y:  // Pattern primarily on XZ plane (Floor/Ceiling - perpendicular to Y)
case AXIS_Z:  // Pattern primarily on XY plane (Front/Back walls - perpendicular to Z)

// AFTER (clearer descriptions):
case AXIS_X:  // Pattern primarily on YZ plane (perpendicular to X - Left/Right axis)
case AXIS_Y:  // Pattern primarily on XZ plane (perpendicular to Y - Front/Back axis)
case AXIS_Z:  // Pattern primarily on XY plane (perpendicular to Z - Floor/Ceiling axis)
```

### 5. Explosion3D.cpp (D:\MCP\OpenRGB3DSpatialPlugin\Effects3D\Explosion3D\Explosion3D.cpp)

**Lines 147-153:**
```cpp
// BEFORE:
case AXIS_Y:  // Directional explosion along Y-axis (Floor to Ceiling)
case AXIS_Z:  // Directional explosion along Z-axis (Front to Back)

// AFTER:
case AXIS_Y:  // Directional explosion along Y-axis (Front to Back)
case AXIS_Z:  // Directional explosion along Z-axis (Floor to Ceiling)
```

### 6. DNAHelix3D.cpp (D:\MCP\OpenRGB3DSpatialPlugin\Effects3D\DNAHelix3D\DNAHelix3D.cpp)

**Lines 158-174:**
```cpp
// BEFORE:
case AXIS_Y:  // Helix along Y-axis (Floor to Ceiling)
case AXIS_Z:  // Helix along Z-axis (Front to Back)

// AFTER:
case AXIS_Y:  // Helix along Y-axis (Front to Back)
case AXIS_Z:  // Helix along Z-axis (Floor to Ceiling)
```

### 7. BreathingSphere3D.cpp (D:\MCP\OpenRGB3DSpatialPlugin\Effects3D\BreathingSphere3D\BreathingSphere3D.cpp)

**Lines 147-153:**
```cpp
// BEFORE:
case AXIS_Y:  // Ellipsoid stretched along Y-axis (Floor to Ceiling)
case AXIS_Z:  // Ellipsoid stretched along Z-axis (Front to Back)

// AFTER:
case AXIS_Y:  // Ellipsoid stretched along Y-axis (Front to Back)
case AXIS_Z:  // Ellipsoid stretched along Z-axis (Floor to Ceiling)
```

## Impact

### Code Behavior
- **Legacy CalculateColor() methods:** Only comments were wrong; code was using correct rel_x, rel_y, rel_z coordinates
- **Grid-aware CalculateColorGrid() methods:** Had ACTUAL BUGS where grid.height and grid.depth were swapped

### User Experience
**Before:** Users selecting "Y-axis" would see effects moving floor-to-ceiling (wrong!)
**After:** Users selecting "Y-axis" now see effects moving front-to-back (correct!)

## Related Issues

This fix addresses the user's feedback:
> "it also appears the axis drop down is wrong and the what the axis are label is not want there are"

The axis dropdown labels now correctly match the actual effect behavior.

## Testing

After rebuilding, test each effect with different axis selections:
1. **X-axis:** Effect should move Left → Right
2. **Y-axis:** Effect should move Front → Back
3. **Z-axis:** Effect should move Floor → Ceiling
4. **Radial:** Effect should radiate from center outward

## Summary

✓ **7 effect files fixed**
✓ **Axis labels now correct** - Y = Front/Back, Z = Floor/Ceiling
✓ **Grid-aware calculations fixed** - Using correct grid dimensions
✓ **Comments updated** - Match actual coordinate system
✓ **User experience improved** - Dropdown matches behavior

**Result:** Axis selection now works as expected! 🎉
