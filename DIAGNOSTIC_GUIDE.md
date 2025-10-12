# Diagnostic Test 3D - User Guide

## Overview

The **Diagnostic Test 3D** effect is a comprehensive testing tool designed to verify that your 3D grid positioning system is working correctly. It provides visual feedback and detailed logging to help identify issues with:

- World position calculations
- Grid boundary detection
- 3D axis orientation
- Effect scale and coverage
- Transform system (rotation, translation)

## How to Use

### 1. Build and Run

After rebuilding the plugin, the "Diagnostic Test 3D" effect will appear in your effect list.

### 2. Add to Effect Stack

1. Go to the **Effect Stack** tab
2. Click **"Add Effect"**
3. Select **"Diagnostic Test 3D"** from the dropdown
4. Click **"Start Effect"**

### 3. Test Modes

The diagnostic effect has **7 test modes** accessible via dropdown:

#### Mode 0: X-Axis Gradient (Left → Right)
- **What it shows**: Red at the leftmost edge, green at the rightmost edge
- **What to look for**:
  - Smooth gradient from red to green as you move left to right
  - If all LEDs are the same color, X-axis may not be changing
  - If gradient is vertical/diagonal, your controller rotation might be off

#### Mode 1: Y-Axis Gradient (Bottom → Top)
- **What it shows**: Red at bottom, green at top
- **What to look for**:
  - Smooth gradient from red to green as you move bottom to top
  - If gradient is horizontal, Y and X axes might be swapped
  - Verify this matches your physical setup orientation

#### Mode 2: Z-Axis Gradient (Front → Back)
- **What it shows**: Red at front (negative Z), green at back (positive Z)
- **What to look for**:
  - THIS IS THE KEY TEST FOR TRUE 3D
  - If you have LEDs at different depths and they show different colors, Z-axis is working
  - If all LEDs at different Z positions show the same color, Z-axis is NOT being used
  - This indicates whether your grid is truly 3D or just 2D

#### Mode 3: Radial Distance (Center → Out)
- **What it shows**: Rainbow colors based on distance from origin
- **What to look for**:
  - LEDs near the center should be similar colors
  - LEDs far from center should show different colors
  - Verifies spherical/radial distance calculations

#### Mode 4: Grid Corners (8 Points)
- **What it shows**: Pulsing white lights at the 8 corner positions of your grid bounding box
- **What to look for**:
  - You should see exactly 8 pulsing points (or fewer if corners are outside your LED layout)
  - Identifies the extremes of your grid
  - Dim blue everywhere else

#### Mode 5: Distance Rings
- **What it shows**: Concentric rings expanding from the center
- **What to look for**:
  - Rings should be spherical/circular
  - Rings should animate outward
  - Tests both distance calculation and animation timing

#### Mode 6: Axis Planes (XYZ Split)
- **What it shows**:
  - Red for negative X region
  - Green for positive Y region
  - Blue for positive Z region
- **What to look for**:
  - Helps identify which region of 3D space each LED occupies
  - Verifies axis orientation matches your expectations

### 4. Use the "Log Grid Diagnostics" Button

Click the **"Log Grid Diagnostics to Console"** button to output detailed information:

```
[DiagnosticTest3D] ========================================
[DiagnosticTest3D] DIAGNOSTIC TEST STARTED
[DiagnosticTest3D] ========================================
[DiagnosticTest3D] Current Parameters:
[DiagnosticTest3D]   Speed: 50
[DiagnosticTest3D]   Brightness: 100
[DiagnosticTest3D]   Frequency: 50
[DiagnosticTest3D]   Size: 50
[DiagnosticTest3D]   Scale: 100
[DiagnosticTest3D]   Normalized Scale: 1.00
[DiagnosticTest3D]   Scale Radius: 10.00
[DiagnosticTest3D]   Test Mode: 2
[DiagnosticTest3D] ========================================
```

### 5. Check the Console Logs

The effect automatically logs position samples every second:

```
[DiagnosticTest3D] Position Sample: world(-2.50, 1.00, 3.00) rel(-2.50, 1.00, 3.00) dist=4.06
[DiagnosticTest3D] Bounds: X[-5.00 to 4.00] Y[-4.50 to 4.50] Z[-4.50 to 4.50] Dist[0.00 to 7.81]
```

**What to analyze:**
- **world(x, y, z)**: The world position after transforms are applied
- **rel(x, y, z)**: Position relative to effect origin
- **dist**: Distance from origin
- **Bounds**: Min/max values for each axis and distance

## Common Issues and Solutions

### Issue 1: Z-Axis shows no gradient (Mode 2)
**Symptom**: All LEDs at different Z depths show the same color
**Diagnosis**: Your grid is likely 2D, not 3D
**Solutions**:
- Check your grid configuration (grid_x, grid_y, grid_z)
- Verify grid_z > 1
- Check LED spacing in Z dimension
- Review controller layout generation code

### Issue 2: All LEDs are black
**Symptom**: No colors visible in any test mode
**Diagnosis**: LEDs are outside the effect boundary
**Solutions**:
- Increase the **Scale** slider (try 150-200)
- Check console logs for bounds vs scale radius
- If `Scale Radius < max distance`, increase scale

### Issue 3: Wrong axis orientation
**Symptom**: X-axis gradient appears vertical
**Diagnosis**: Controller rotation is incorrect
**Solutions**:
- Check controller transform rotation values
- Verify your controller placement in the 3D viewport
- Test with Axis Planes mode (Mode 6) to identify orientation

### Issue 4: Bounds show Z[0.00 to 0.00]
**Symptom**: Console logs show no Z variation
**Diagnosis**: All LEDs have Z=0
**Solutions**:
- Grid is definitely 2D
- Check `ControllerLayout3D::GenerateCustomGridLayout()`
- Verify `z_pos = led_idx / (grid_x * grid_y)` is being calculated
- Confirm grid_z parameter is being used

### Issue 5: Effects don't respond to grid size changes
**Symptom**: Changing grid settings has no effect
**Diagnosis**: World positions not being recalculated
**Solutions**:
- Check if `world_positions_dirty` flag is set when grid changes
- Verify `UpdateWorldPositions()` is being called
- Ensure grid bounds are recalculated on parameter change

## Interpreting the Results

### Good 3D Grid System:
```
[DiagnosticTest3D] Bounds: X[-5.00 to 4.00] Y[-4.50 to 4.50] Z[-4.50 to 4.50]
```
- All three axes show variation
- Z range is significant (not 0 to 0)
- Mode 2 (Z-Axis) shows clear color gradient

### 2D Grid System (Problem):
```
[DiagnosticTest3D] Bounds: X[-5.00 to 4.00] Y[-4.50 to 4.50] Z[0.00 to 0.00]
```
- Z shows no variation
- All LEDs are on the same plane
- Mode 2 (Z-Axis) shows uniform color

### Boundary Issues:
```
[DiagnosticTest3D]   Scale Radius: 10.00
[DiagnosticTest3D] Bounds: ... Dist[0.00 to 15.00]
```
- If max distance (15.00) > scale radius (10.00), outer LEDs won't be lit
- Increase Scale parameter

## Advanced Diagnostics

### Check Effect Origin
The effect origin determines where (0,0,0) is located. Verify:
- Reference Mode is set correctly (Room Center, User Position, or Custom Point)
- If using custom reference point, verify coordinates are correct

### Check Transform System
1. Set all rotations to 0°
2. Set position to (0, 0, 0)
3. Run Mode 0, 1, 2 to establish baseline
4. Rotate controller 90° on one axis
5. Verify gradient rotates accordingly

### Check Grid Bounds Calculation
From console logs:
```cpp
// Grid bounds in OpenRGB3DSpatialTab_EffectStackRender.cpp
grid_min_x = -half_x
grid_max_x = custom_grid_x - half_x - 1
```
Verify these bounds encompass your world positions.

## What This Diagnostic Tells You

✅ **If Mode 2 (Z-Axis) shows a gradient**: Your 3D system is working!
❌ **If Mode 2 shows uniform color**: Your grid is 2D, not 3D
✅ **If Modes 0, 1, 2 all work differently**: All three axes are functional
⚠️ **If all LEDs are black in any mode**: Boundary/scale issue
✅ **If Mode 4 shows 8 pulsing points**: Grid corners are correctly identified

## Next Steps

After running diagnostics:

1. **If 3D is working**: Your effects should respect all three dimensions. Issues might be in individual effect algorithms.

2. **If 3D is NOT working**:
   - Review `ControllerLayout3D::GenerateCustomGridLayout()` at line 70-73
   - Verify grid_z is > 1
   - Check spacing calculations

3. **If effects still don't look right**:
   - Verify effect parameters (Speed, Frequency, Size, Scale)
   - Check effect-specific code for proper 3D calculations
   - Ensure effects use `world_position` not `local_position`

## Support

If you find issues, provide this information:
1. Test mode that fails
2. Console log output (bounds, samples)
3. Grid configuration (grid_x, grid_y, grid_z)
4. Controller type (virtual/physical)
5. Expected vs actual behavior
