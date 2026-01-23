# Effect 3D Rotation Refactoring Plan

## Overview
Replace confusing axis/surface/coverage controls with intuitive 3D rotation controls that allow effects to be rotated around their origin point, similar to how controllers/devices can be rotated.

## Current Problems
1. **Spin3D** has confusing `surface_type` (Floor, Ceiling, Walls, etc.) and `axis` options that overlap
2. **Coverage** system (Entire Room, Floor, Ceiling, Walls) is redundant with rotation
3. **Axis** selection (X, Y, Z, Radial) doesn't work well for 3D spatial effects
4. Effects don't feel like they're properly utilizing 3D space
5. Multiple overlapping controls create confusion

## Proposed Solution
Replace all axis/surface/coverage controls with:
- **3D Rotation Controls**: Yaw (horizontal), Pitch (vertical), Roll (twist) sliders
- **Rotation around Origin**: Effects rotate around their selected origin point
- **Visual Feedback**: Show effect rotation in viewport (optional gizmo/visualization)

## Implementation Plan

### Phase 1: Base System Changes

#### 1.1 Update SpatialEffect3D.h
- [ ] Remove `effect_axis`, `axis_combo`, `axis_none`, `effect_reverse`
- [ ] Remove `effect_coverage`, `coverage_combo`
- [ ] Add rotation controls:
  - `effect_rotation_yaw` (0-360 degrees, horizontal rotation)
  - `effect_rotation_pitch` (0-360 degrees, vertical rotation)
  - `effect_rotation_roll` (0-360 degrees, twist rotation)
- [ ] Add rotation sliders: `rotation_yaw_slider`, `rotation_pitch_slider`, `rotation_roll_slider`
- [ ] Add helper method: `TransformPointByRotation(float x, float y, float z, const Vector3D& origin)`

#### 1.2 Update SpatialEffect3D.cpp
- [ ] Remove axis combo UI creation (lines ~207-230)
- [ ] Remove coverage combo UI creation (lines ~232-251)
- [ ] Add rotation sliders UI (3 sliders: Yaw, Pitch, Roll, 0-360 degrees)
- [ ] Add rotation transformation helper function
- [ ] Update `SaveSettings()` to save rotation instead of axis/coverage
- [ ] Update `LoadSettings()` to load rotation (with migration from old axis/coverage)
- [ ] Remove `OnAxisChanged()`, `OnReverseChanged()` slot handlers
- [ ] Add `OnRotationChanged()` slot handler

#### 1.3 Update SpatialEffectTypes.h
- [ ] Keep `Rotation3D` struct (already exists)
- [ ] Remove or deprecate `EffectAxis` enum (or keep for backward compatibility)
- [ ] Update `SpatialEffectParams` to use `Rotation3D` instead of `axis`

### Phase 2: Effect-Specific Updates

#### 2.1 Spin3D
- [ ] Remove `surface_type` and `surface_combo`
- [ ] Remove all `surface_type` switch cases (0-11)
- [ ] Update `CalculateColorGrid()` to:
  - Get effect origin from `GetEffectOriginGrid()`
  - Apply rotation transformation to LED positions
  - Calculate spin based on rotated coordinates
  - Spin arms should radiate from origin in rotated space
- [ ] Simplify UI: Remove surface combo, keep only arms slider
- [ ] Update `SaveSettings()`/`LoadSettings()` to remove surface_type

#### 2.2 Wave3D
- [ ] Remove axis selection logic
- [ ] Apply rotation to wave direction
- [ ] Wave should propagate in rotated direction from origin

#### 2.3 Wipe3D
- [ ] Remove axis selection
- [ ] Apply rotation to wipe direction
- [ ] Wipe should move along rotated axis from origin

#### 2.4 Tornado3D
- [ ] Remove axis selection
- [ ] Apply rotation to tornado axis
- [ ] Tornado should spiral along rotated axis from origin

#### 2.5 Spiral3D
- [ ] Remove axis selection
- [ ] Apply rotation to spiral axis
- [ ] Spiral should twist along rotated axis from origin

#### 2.6 All Other Effects
- [ ] Review each effect in `Effects3D/` directory
- [ ] Remove axis/coverage dependencies
- [ ] Apply rotation transformation where applicable
- [ ] Test each effect with rotation controls

### Phase 3: Coordinate Transformation

#### 3.1 Rotation Math
Create helper function in `SpatialEffect3D`:
```cpp
Vector3D TransformPointByRotation(float x, float y, float z, 
                                  const Vector3D& origin,
                                  float yaw, float pitch, float roll) const
{
    // Translate to origin
    float tx = x - origin.x;
    float ty = y - origin.y;
    float tz = z - origin.z;
    
    // Apply rotations (order: Yaw -> Pitch -> Roll)
    // Yaw (rotation around Y axis)
    float yaw_rad = yaw * M_PI / 180.0f;
    float cos_yaw = cosf(yaw_rad);
    float sin_yaw = sinf(yaw_rad);
    float nx = tx * cos_yaw - tz * sin_yaw;
    float nz = tx * sin_yaw + tz * cos_yaw;
    ty = ty; // Y unchanged by yaw
    
    // Pitch (rotation around X axis)
    float pitch_rad = pitch * M_PI / 180.0f;
    float cos_pitch = cosf(pitch_rad);
    float sin_pitch = sinf(pitch_rad);
    float ny = ty * cos_pitch - nz * sin_pitch;
    nz = ty * sin_pitch + nz * cos_pitch;
    nx = nx; // X unchanged by pitch
    
    // Roll (rotation around Z axis)
    float roll_rad = roll * M_PI / 180.0f;
    float cos_roll = cosf(roll_rad);
    float sin_roll = sinf(roll_rad);
    float fx = nx * cos_roll - ny * sin_roll;
    float fy = nx * sin_roll + ny * cos_roll;
    float fz = nz; // Z unchanged by roll
    
    // Translate back
    return {fx + origin.x, fy + origin.y, fz + origin.z};
}
```

#### 3.2 Usage Pattern
Each effect's `CalculateColorGrid()` should:
1. Get origin: `Vector3D origin = GetEffectOriginGrid(grid);`
2. Transform LED position: `Vector3D rotated = TransformPointByRotation(x, y, z, origin, yaw, pitch, roll);`
3. Calculate effect using rotated coordinates
4. Effect pattern is now in rotated space relative to origin

### Phase 4: UI Updates

#### 4.1 Base Controls
- [ ] Replace axis combo with 3 rotation sliders:
  - "Rotation Yaw (Horizontal)" 0-360°
  - "Rotation Pitch (Vertical)" 0-360°
  - "Rotation Roll (Twist)" 0-360°
- [ ] Add reset button to set all rotations to 0
- [ ] Add tooltips explaining rotation around origin

#### 4.2 Effect-Specific UI
- [ ] Remove surface/axis/coverage combos from all effects
- [ ] Simplify effect UIs to focus on effect-specific parameters
- [ ] Update tooltips to mention rotation controls

### Phase 5: Migration & Backward Compatibility

#### 5.1 Settings Migration
- [ ] In `LoadSettings()`, detect old axis/coverage settings
- [ ] Convert old axis to approximate rotation:
  - AXIS_X → Yaw=90° or -90°
  - AXIS_Y → Pitch=90° or -90°
  - AXIS_Z → Yaw=0°, Pitch=0°
  - AXIS_RADIAL → Keep as-is (radial effects may need special handling)
- [ ] Convert old coverage to rotation hints (optional)

#### 5.2 Testing
- [ ] Test each effect with rotation controls
- [ ] Verify effects rotate correctly around origin
- [ ] Test with different origin points (room center, user position, custom)
- [ ] Verify saved/loaded settings work correctly

### Phase 6: Documentation & Cleanup

#### 6.1 Code Cleanup
- [ ] Remove unused `EffectAxis` enum (or mark deprecated)
- [ ] Remove unused coverage enum/constants
- [ ] Update comments and documentation

#### 6.2 User Documentation
- [ ] Update effect descriptions to mention rotation
- [ ] Create guide on using rotation controls
- [ ] Document origin point system

## Files to Modify

### Core Files
- `SpatialEffect3D.h` - Add rotation members and methods
- `SpatialEffect3D.cpp` - Remove axis/coverage UI, add rotation UI and math
- `SpatialEffectTypes.h` - Update structs (optional)

### Effect Files (All in `Effects3D/`)
- `Spin3D/Spin3D.h` - Remove surface_type
- `Spin3D/Spin3D.cpp` - Remove surface logic, add rotation
- `Wave3D/Wave3D.cpp` - Remove axis, add rotation
- `Wipe3D/Wipe3D.cpp` - Remove axis, add rotation
- `Tornado3D/Tornado3D.cpp` - Remove axis, add rotation
- `Spiral3D/Spiral3D.cpp` - Remove axis, add rotation
- `Plasma3D/Plasma3D.cpp` - Review and update
- `DNAHelix3D/DNAHelix3D.cpp` - Review and update
- `Lightning3D/Lightning3D.cpp` - Review and update
- All other effects in `Effects3D/` directory

## Benefits
1. **Simpler UI**: 3 rotation sliders instead of multiple combos
2. **More Intuitive**: Users can visualize rotation like rotating a controller
3. **Better 3D Utilization**: Effects properly use 3D space
4. **Consistent**: All effects use same rotation system
5. **Flexible**: Can rotate effects in any direction around any origin

## Potential Issues & Solutions

### Issue: Radial Effects
**Solution**: Radial effects (like explosions) may not need rotation, or rotation could affect the radial pattern's orientation.

### Issue: Backward Compatibility
**Solution**: Migration code in `LoadSettings()` to convert old axis/coverage to rotation.

### Issue: Performance
**Solution**: Rotation transformation is simple math, should be fast. Can optimize if needed.

### Issue: Effect-Specific Needs
**Solution**: Some effects may need special handling. Review each effect individually.

## Implementation Order
1. Phase 1: Base system (SpatialEffect3D)
2. Phase 2: Spin3D (most complex, good test case)
3. Phase 2: Other effects (one by one)
4. Phase 3: Coordinate transformation (test thoroughly)
5. Phase 4: UI polish
6. Phase 5: Migration & testing
7. Phase 6: Documentation

## Estimated Effort
- Phase 1: 2-3 hours
- Phase 2: 4-6 hours (all effects)
- Phase 3: 1-2 hours
- Phase 4: 1 hour
- Phase 5: 2-3 hours
- Phase 6: 1 hour
**Total: ~11-16 hours**
