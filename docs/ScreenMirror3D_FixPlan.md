# Screen Mirror 3D - Clean Fix Plan

## Current Understanding (VERIFIED)

### Coordinate System
- ALL values in grid units (1 unit = 10mm default)
- Origin (0,0,0) = front-left-floor corner
- X = left→right, Y = front→back, Z = floor→ceiling

### User's Setup (Wolfieee.json)
**Office Main Screen:**
- Position: X=271.9, Y=100, Z=28 (grid units) = (2719mm, 1000mm, 280mm)
- Size: 615mm × 365mm
- Rotation: 0°, 0°, 0°
- Bounds in grid units:
  - Left: 271.9 - (615/2)/10 = 271.9 - 30.75 = 241.15
  - Right: 271.9 + 30.75 = 302.65
  - Bottom: 28 - (365/2)/10 = 28 - 18.25 = 9.75
  - Top: 28 + 18.25 = 46.25

**Keyboard:**
- Transform position: X=275, Y=73, Z=71 (grid units) = (2750mm, 730mm, 710mm)
- Rotation: X=90°, Y=0°, Z=0°
- This is the CENTER of the keyboard after rotation!

### The Problem with Rotation

When keyboard is rotated 90° around X-axis:
- Local Y becomes world Z
- Local Z becomes world -Y

The transform position (275, 73, 71) is the center AFTER rotation is applied!

So we can't just use world Z directly - we need to understand which part of the keyboard (which keys) are at which Z heights after rotation.

## The Correct Approach

**STOP trying to be clever with projections!**

### Simple World-Space Mapping

For each LED at position (x, y, z) in grid units:

1. **Find which screens it can "see"** (within falloff distance)

2. **For each screen, calculate UV based on WORLD coordinates:**
   ```
   // Convert screen dimensions to grid units
   screen_width_units = screen.GetWidthMM() / grid_scale_mm
   screen_height_units = screen.GetHeightMM() / grid_scale_mm

   // Screen is centered at screen.position
   screen_left = screen.position.x - screen_width_units/2
   screen_right = screen.position.x + screen_width_units/2
   screen_bottom = screen.position.z - screen_height_units/2
   screen_top = screen.position.z + screen_height_units/2

   // Map LED world position directly to UV
   U = (led.x - screen_left) / screen_width_units
   V = (led.z - screen_bottom) / screen_height_units

   // Clamp to [0, 1]
   U = clamp(U, 0.0, 1.0)
   V = clamp(V, 0.0, 1.0)
   ```

3. **NO rotation transformations needed!** Just use raw world coordinates.

4. **Sample from screen capture at UV, flipping V if needed for capture format**

## Why This Will Work

- LEDs with similar X positions sample similar U coordinates
- LEDs with similar Z positions sample similar V coordinates
- Screen rotation doesn't matter - we map based on screen's world bounds
- Simple, predictable, no coordinate confusion

## Implementation Steps

1. Remove all `local_offset` and rotation matrix code
2. Use pure world-space mapping
3. Convert screen dimensions from MM to grid units
4. Calculate screen bounds in world space
5. Map LED world position to UV
6. Handle screen capture V-flip separately

## Expected Results

**Keyboard (X≈255-291, Y=73, Z=71):**
- Screen bounds: X=241-303, Z=10-46
- Keyboard X in screen bounds ✓
- Keyboard Z=71 > screen top 46 → V will clamp to 1.0 → sample TOP edge
- U varies 0.23-0.80 across keyboard width ✓

**Speakers (X=205/334, Y=73, Z=18):**
- Left speaker X=205 < screen left 241 → U clamps to 0.0 → LEFT edge
- Right speaker X=334 > screen right 303 → U clamps to 1.0 → RIGHT edge
- Speakers Z=18 in screen range (10-46) → V=0.22 → sample 22% from bottom
