# Screen Mirror 3D - Development Log

## The Core Challenge
Traditional 2D ambilight software maps LEDs around screen edges to specific pixel positions. In 3D space, LEDs are scattered throughout the room at various positions and distances from screens. **How do we map 3D LED positions to 2D screen pixels?**

---

## Approaches Tried

### 1. Perpendicular Projection (FAILED)
**Date:** Session 1
**Concept:** Drop a perpendicular line from LED to screen plane, sample where it intersects.
**Implementation:** `ProjectPointOntoPlane()` - calculate perpendicular intersection point, convert to UV coordinates.

**Problems:**
- All LEDs at similar Y-depth but different X positions projected to nearly the same UV coordinates
- Keyboard spanning X=255-272mm all sampled U=0.46-0.52 (6% cluster in center)
- Distance calculation used perpendicular distance (38mm) instead of full 3D distance (450mm)
- **Result:** Everything clustered in screen center, no spatial variation

**User Feedback:** "everything is coming from the centre of the screen", "my pc case fan lights are lighting up when things are on the far right screen and they are on the far left of my setup"

---

### 2. Ray Tracing Toward Screen Center (PARTIAL)
**Date:** Session 1
**Concept:** Each LED casts a ray toward the screen's center point, sample where ray intersects screen.
**Implementation:** `RayTracePlane()` with direction = LED→screen_center

**Problems:**
- Only considered single screen, didn't handle multi-monitor setups properly
- Didn't account for "transparent screen" requirement (LEDs behind screen need to see through)
- Still caused some clustering since all rays aimed at one point

**Why Abandoned:** User needed multi-monitor support with LEDs seeing all screens simultaneously

---

### 3. Edge Zone Sampling with Dominant Direction (PARTIALLY WORKING)
**Date:** Session 2
**Concept:** Determine which "zone" LED is in relative to screen (left/right/top/bottom), sample from that edge zone.
**Implementation:**
```cpp
if (abs_z > abs_x && abs_z > abs_y) {
    // Vertically dominant - LED above or below screen
    if (offset_z < 0) {
        // BELOW screen - sample bottom edge at V=0.85
        result.v = 1.0f - edge_zone_depth;  // Fixed at bottom 15%
        result.u = (led_x - screen_left) / screen_width;  // Spread horizontally
    }
}
```

**Problems:**
- **Color averaging/blurring:** Multiple LEDs in same zone all sampled from same narrow band (e.g., all keyboard LEDs at V=0.85)
- **Top bias on left/right edges:** Speaker LEDs on sides sampled from top of vertical range instead of their actual Z height
- Fixed edge zones (15% of screen) meant limited color variation
- User: "if there are only 1 or 2 colours on the screen it kinda gives the right effect but once you get 4 or 5 colours they all mix together"

**What Worked:**
- Left/right separation worked correctly
- Top/bottom separation worked for keyboard vs overhead LEDs
- Screen rotation handling worked after transformation to local coordinate space

**What Failed:**
- Per-LED color variation insufficient
- Edge zone concept too restrictive

---

### 4. Direct Spatial Mapping (CURRENT - TESTING)
**Date:** Session 3
**Concept:** Directly map LED's 3D position to screen UV coordinates. Treat screen as a "magic window" where position determines sample.
**Implementation:**
```cpp
// Transform LED to screen's local coordinate space (handles rotation)
Vector3D local_offset = InverseRotate(led_pos - screen_center);

// Direct mapping: X position → U coordinate, Z position → V coordinate
result.u = (local_offset.x + half_width) / screen_width;
result.v = 1.0f - ((local_offset.z + half_height) / screen_height);  // Flip V
```

**Advantages:**
- Simple, predictable behavior
- Each LED samples based on its exact 3D position
- Handles rotated screens via local coordinate transformation
- No arbitrary edge zones or clustering

**Potential Issues:**
- LEDs outside screen bounds clamp to edges (may want different behavior)
- No concept of "viewing angle" or perspective
- May not feel natural for LEDs far from screens

**Status:** Currently implemented, awaiting user testing with 4-color quadrant video

---

## Supporting Systems That Work

### Distance Falloff (WORKING)
**Three modes:**
- Linear: Gradual fade
- Inverse Square: Realistic physics (1 / (1 + dist²))
- Exponential: Sharp cutoff (exp(-dist * 2))

**User-selectable reference point for distance calculation:**
- **User Position** (recommended): Distance from viewer's reference point
- **Room Center**: Distance from center of room

**Why This Matters:** Using screen center for falloff caused asymmetric behavior (left side lit, right side dark with exponential falloff). Using user position makes falloff symmetric and perceptually correct.

### Multi-Monitor Blending (WORKING)
Collects contributions from all screens, blends based on:
- Distance falloff weight
- Blend zone percentage (0% = hard edges, 100% = full blend)

### V-Coordinate Flipping (WORKING)
Screen capture is upside-down (Y=0 at top), but our 3D coords have Z=0 at bottom.
**Solution:** `result.v = 1.0f - calculated_v` when sampling

### Rotation Handling (WORKING)
Monitors can be rotated (e.g., -20° for side monitors).
**Solution:** Transform LED position to screen's local coordinate space using rotation matrix before UV calculation

---

## Technical Learnings

### Coordinate Systems
- **World Space:** X=left-right, Y=depth (front-back), Z=height (floor-ceiling)
- **Grid Scale:** 10mm per grid unit
- **Screen Local Space:** X=left-right on screen, Y=perpendicular to screen, Z=up-down on screen
- **UV Space:** U=horizontal (0=left, 1=right), V=vertical (0=top, 1=bottom) - NOTE: V flipped from Z!

### Critical Math
```cpp
// Full 3D distance (not perpendicular!)
distance = sqrt(dx² + dy² + dz²)

// Transform to screen local space
local = InverseRotate(world_offset)  // Uses transposed rotation matrix

// UV mapping
U = (local.x + half_width) / width   // Maps [-width/2, +width/2] to [0, 1]
V = 1.0 - (local.z + half_height) / height  // Flipped!
```

### Common Pitfalls
1. **Using perpendicular distance instead of 3D distance** - causes clustering
2. **Forgetting V-coordinate flip** - causes top/bottom inversion
3. **Not handling screen rotation** - breaks with angled monitors
4. **Using screen center for falloff** - asymmetric results
5. **Fixed edge zones with dominant direction** - causes color averaging

---

## User Setup (Wolfieee)
- **3 Monitors:**
  - Office Main (center): X=271.9, Y=100, Z=28, rotation=0°
  - Office Secondary (right): X=332.3, Y=100, Z=39, rotation=-20°
  - Wolfieee (left): X=211.5, Y=100, Z=39, rotation=20°
- **User Position:** X=276, Y=100, Z=120 (sitting at desk)
- **Keyboard:** X=275, Y=73, Z=71 (on desk, in front of center monitor)
- **Speakers:**
  - Left: X=205, Y=73, Z=18 (low, left side)
  - Right: X=334, Y=73, Z=18 (low, right side)
- **PC Case:** X=172, Y=33, Z=65 (far left, inside desk)

---

## Open Questions

1. **Should LEDs outside screen bounds:**
   - Clamp to edge pixels? (current)
   - Sample from "extended virtual screen"?
   - Fade to black?
   - Be excluded entirely?

2. **Should we add perspective/viewing angle:**
   - Ray from user position through LED to screen?
   - Would this feel more natural?
   - Or would it break multi-monitor setups?

3. **Per-device manual mapping:**
   - Allow users to manually set UV offset/scale per device?
   - Too complex for general users?

4. **Volumetric screen concept:**
   - Treat screen as emitting 3D light cone/volume?
   - More realistic but computationally expensive?

---

## Next Steps

1. **Test current direct spatial mapping** with 4-color quadrant video
2. **Verify per-LED color separation** - each keyboard LED should show different color
3. **Check speaker positioning** - should sample bottom corners correctly
4. **Document results** - does it feel like good ambilight?
5. **Add fine-tuning if needed** - UV scale/offset, bounds handling options

---

## Success Criteria

✅ **Left speaker (X=205, Z=18)** should show **YELLOW** (bottom-left quadrant)
✅ **Right speaker (X=334, Z=18)** should show **RED** (bottom-right quadrant)
✅ **Keyboard LEDs** should show **gradient from left to right** (yellow→red)
✅ **No color averaging** - rainbow gradient should show distinct colors per LED
✅ **Multi-monitor** should blend naturally when content on different screens
✅ **Falloff** should be symmetric (left and right sides behave the same)

---

## Code References

- **Spatial Mapping:** `Geometry3DUtils.h` - `SpatialMapToScreen()`
- **Main Effect Logic:** `Effects3D/ScreenMirror3D/ScreenMirror3D.cpp` - `CalculateColorGrid()`
- **Screen Capture:** `ScreenCaptureManager` - platform-specific screen grabbing
- **Distance Falloff:** `Geometry3DUtils.h` - `ComputeFalloff()`
