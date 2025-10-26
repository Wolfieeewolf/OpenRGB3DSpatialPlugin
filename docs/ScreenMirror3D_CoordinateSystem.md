# Screen Mirror 3D - Coordinate System Facts

## Critical Findings from Code Investigation

### The Coordinate System (FACTS)
1. **ALL coordinates in `CalculateColorGrid(x, y, z)` are in GRID UNITS**
   - Default: 1 grid unit = 10mm
   - These are NOT millimeters, NOT pixels, but GRID UNITS

2. **Axes Definition (OpenGL Y-up standard):**
   - X-axis: LEFT → RIGHT (0 = left wall, positive = right wall)
   - Y-axis: FLOOR → CEILING (0 = floor, positive = ceiling) - Y-up standard
   - Z-axis: FRONT → BACK (0 = front wall, positive = back wall)
   - Origin (0,0,0) = front-left-floor corner of room

3. **DisplayPlane3D position:**
   - `GetTransform().position` = **CENTER** of the display in GRID UNITS
   - Width and Height are in MILLIMETERS (GetWidthMM(), GetHeightMM())
   - Must convert: `grid_units = millimeters / grid_scale_mm`

4. **Screen bounds:**
   - Left edge: `center.x - (width_mm / 2) / grid_scale_mm`
   - Right edge: `center.x + (width_mm / 2) / grid_scale_mm`
   - Bottom edge: `center.y - (height_mm / 2) / grid_scale_mm`  (Y-up: Y is vertical)
   - Top edge: `center.y + (height_mm / 2) / grid_scale_mm`

### Current Bug in Our Code

**THE PROBLEM:**
We're mixing units! We calculate in millimeters but receive coordinates in grid units!

```cpp
// WRONG - we're doing this:
float half_width = plane.GetWidthMM() * 0.5f;  // In MM
result.u = (local_offset.x + half_width) / plane.GetWidthMM();
// But local_offset.x is in GRID UNITS after rotation!
```

**What we should be doing:**
```cpp
// Convert screen dimensions to grid units
float half_width_units = (plane.GetWidthMM() * 0.5f) / grid_scale_mm;
float half_height_units = (plane.GetHeightMM() * 0.5f) / grid_scale_mm;

// LED position comes in grid units
// Screen position is in grid units
// Calculate offset in grid units
float offset_x_units = led_x_grid - screen_center_x_grid;
float offset_y_units = led_y_grid - screen_center_y_grid;  // Y-up: Y is vertical

// Map to UV (0-1)
result.u = (offset_x_units + half_width_units) / (plane.GetWidthMM() / grid_scale_mm);
result.v = (offset_y_units + half_height_units) / (plane.GetHeightMM() / grid_scale_mm);
```

### Example with Real Numbers

**Your Setup (from JSON):**
- Grid scale: 10mm per grid unit
- Office Main screen: position X=271.9, Y=100, Z=28 (in grid units, Y-up: Y is height)
- Screen size: 615mm wide × 365mm tall
- Keyboard LED: position X=255.1, Y=73, Z=66.2 (in grid units, Y-up: Y is height)

**Convert to same units:**
- Screen center: (271.9, 100, 28) grid units = (2719mm width, 1000mm height, 280mm depth)
- Screen half-width: 615mm / 2 = 307.5mm = 30.75 grid units
- Screen half-height: 365mm / 2 = 182.5mm = 18.25 grid units
- Keyboard LED: (255.1, 73, 66.2) grid units = (2551mm width, 730mm height, 662mm depth)

**Calculate offsets (grid units) - Y-up coordinate system:**
- offset_x = 255.1 - 271.9 = -16.8 grid units (LED is LEFT of screen center)
- offset_y = 73 - 100 = -27.0 grid units (LED is BELOW screen center, Y-up)

**Calculate UV:**
- U = (-16.8 + 30.75) / (615/10) = 13.95 / 61.5 = 0.227 (LEFT side)
- V = (-27.0 + 18.25) / (365/10) = -8.75 / 36.5 = -0.24 (BELOW screen, out of bounds)

**Screen bounds check (Y-up):**
- Screen bottom: 100 - 18.25 = 81.75 grid units
- Screen top: 100 + 18.25 = 118.25 grid units
- Keyboard at Y=73 is **BELOW** the screen (73 < 81.75)!

So keyboard should sample from BOTTOM edge (V=1.0) or be out of bounds!

### The Real Issue (Y-up coordinate system)

**Your keyboard is BELOW the bottom edge of your screen!**
- Keyboard Y=73 grid units = 730mm from floor
- Screen bottom edge: 100 - 18.25 = 81.75 grid units = 817.5mm from floor
- Keyboard is 87.5mm (almost 9cm) BELOW the screen!

**This makes physical sense:**
- Screen sitting on desk at ~817mm height (bottom edge)
- Keyboard sitting on desk at ~730mm height
- The height difference matches a typical desk setup

**But you said:**
> "keyboard is 430mm away from my middle monitors bottom edge in the centre"

This 430mm is the Z-distance (depth), not Y-distance (height)!

### Correct Interpretation (Y-up)

Looking at your JSON positions with correct Y-up understanding:
- Screen CENTER at Y=100 grid units = 1000mm from floor
- Screen height = 365mm, so:
  - Bottom edge: 1000 - 182.5 = 817.5mm from floor
  - Top edge: 1000 + 182.5 = 1182.5mm from floor
- Keyboard at Y=73 grid units = 730mm from floor

**This makes perfect sense:**
- Keyboard is on desk at 730mm height
- Screen bottom is at 817.5mm height (87.5mm above keyboard)
- Screen center is at 1000mm height (1 meter from floor)

The Z coordinate difference (66.2 - 28 = 38.2 grid units = 382mm) represents the depth difference between keyboard and screen, which is close to the 430mm you mentioned!
