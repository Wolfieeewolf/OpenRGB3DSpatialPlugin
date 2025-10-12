# Local Position vs World Position - Complete Explanation

**Date:** 2025-10-12

## Quick Answer

**Yes, they COULD be the same** - but they're different because we need **two separate coordinate spaces** for flexibility:

- **Local Position:** LED's position relative to its own controller (controller's local coordinate system)
- **World Position:** LED's position in the room (global coordinate system shared by all controllers)

**Both use the same units (grid units)** - but different origins and coordinate spaces!

## The Two Coordinate Systems

### 1. Local Position (Controller Space)
- **Origin:** Center of the controller itself (0,0,0)
- **Purpose:** Define LED layout within the controller
- **Never changes:** Fixed when controller is created
- **Example:** LED #5 on a strip might be at local position (5, 0, 0) - 5 units from controller center

### 2. World Position (Room Space)
- **Origin:** Front-left-floor corner of room (0,0,0)
- **Purpose:** Define LED position in the actual room
- **Changes when:** Controller is moved, rotated, or scaled
- **Example:** Same LED might be at world position (50.0, 38.4, 18.5) after controller transform

## Why We Need Both

### Scenario: LED Strip Controller

**Local Position (Controller's Perspective):**
```
LED Strip Layout (centered at controller origin):
LED 0: local_position = (-2.5, 0, 0)  ← 2.5 units left of center
LED 1: local_position = (-1.5, 0, 0)
LED 2: local_position = (-0.5, 0, 0)
LED 3: local_position = ( 0.5, 0, 0)
LED 4: local_position = ( 1.5, 0, 0)
LED 5: local_position = ( 2.5, 0, 0)  ← 2.5 units right of center
```

**World Position (Room's Perspective):**
```
After placing controller at room position (50, 40, 20):
LED 0: world_position = (47.5, 40, 20)  ← local + controller position
LED 1: world_position = (48.5, 40, 20)
LED 2: world_position = (49.5, 40, 20)
LED 3: world_position = (50.5, 40, 20)
LED 4: world_position = (51.5, 40, 20)
LED 5: world_position = (52.5, 40, 20)
```

**After rotating controller 90° around Z-axis:**
```
LED 0: world_position = (50, 37.5, 20)  ← Same local, different world!
LED 1: world_position = (50, 38.5, 20)
LED 2: world_position = (50, 39.5, 20)
LED 3: world_position = (50, 40.5, 20)
LED 4: world_position = (50, 41.5, 20)
LED 5: world_position = (50, 42.5, 20)
```

## The Transformation Pipeline

```
Local Position → [Transform] → World Position
   (fixed)      (move/rotate)    (calculated)
```

### Step-by-Step Calculation (ControllerLayout3D.cpp lines 215-239)

```cpp
// 1. Start with local position (relative to controller center)
Vector3D local = led_pos->local_position;  // e.g., (2.5, 0, 0)

// 2. Apply rotation (controller can be rotated in room)
Vector3D rotated = ApplyRotation(local, controller_rotation);

// 3. Apply translation (move to controller's room position)
led_pos->world_position.x = rotated.x + ctrl_transform->transform.position.x;
led_pos->world_position.y = rotated.y + ctrl_transform->transform.position.y;
led_pos->world_position.z = rotated.z + ctrl_transform->transform.position.z;
```

**Example:**
```
Local: (2.5, 0, 0)
Controller Position: (50, 40, 20)
Controller Rotation: (0, 0, 90°)

Step 1: local = (2.5, 0, 0)
Step 2: rotated = (0, 2.5, 0)  ← 90° Z rotation
Step 3: world = (0 + 50, 2.5 + 40, 0 + 20) = (50, 42.5, 20)
```

## Code References

### Local Position Creation (ControllerLayout3D.cpp:82-86)

```cpp
// Create local position based on grid layout
led_pos.local_position.x = (float)x_pos;
led_pos.local_position.y = (float)y_pos;
led_pos.local_position.z = (float)z_pos;

// Initially, world = local (before any transforms)
led_pos.world_position = led_pos.local_position;
```

### Local Position Centering (ControllerLayout3D.cpp:113-123)

Controllers are centered around (0,0,0) in local space:

```cpp
// Find center of all LEDs
float center_x = (min_x + max_x) / 2.0f;
float center_y = (min_y + max_y) / 2.0f;
float center_z = (min_z + max_z) / 2.0f;

// Shift all LEDs so controller is centered at origin
for(unsigned int i = 0; i < positions.size(); i++)
{
    positions[i].local_position.x -= center_x;
    positions[i].local_position.y -= center_y;
    positions[i].local_position.z -= center_z;
    positions[i].world_position = positions[i].local_position;
}
```

### World Position Calculation (ControllerLayout3D.cpp:215-239)

```cpp
// Get LED's local position (relative to controller)
Vector3D local = led_pos->local_position;

// Apply rotation matrices (X, Y, Z rotations)
Vector3D rotated = ApplyRotationMatrices(local, rotation_x, rotation_y, rotation_z);

// Translate to world space (add controller's room position)
led_pos->world_position.x = rotated.x + ctrl_transform->transform.position.x;
led_pos->world_position.y = rotated.y + ctrl_transform->transform.position.y;
led_pos->world_position.z = rotated.z + ctrl_transform->transform.position.z;
```

## Where Each Is Used

### Local Position Used For:
- **Viewport Rendering (LEDViewport3D.cpp:441, 457):** Drawing LEDs relative to controller
- **Controller Layout:** Defining LED strip/matrix patterns
- **Transform Calculations:** Input to rotation/translation math

### World Position Used For:
- **Effect Calculations:** All effects receive `world_position` coordinates
- **Spatial Queries:** Finding nearest LEDs in room space
- **Distance Calculations:** Measuring LED-to-LED distances in room
- **Effect Origins:** Calculating distance from room center or reference points

## Visual Example: Keyboard in Room

```
LOCAL SPACE (Keyboard's perspective):
     Y
     ↑
     |  [Q][W][E][R]
     |  [A][S][D][F]
     |  [Z][X][C][V]
     +----------→ X
    (0,0,0)

Local positions:
Q = (-3, 2, 0)    W = (-2, 2, 0)    E = (-1, 2, 0)    R = (0, 2, 0)
A = (-3, 1, 0)    S = (-2, 1, 0)    D = (-1, 1, 0)    F = (0, 1, 0)
Z = (-3, 0, 0)    X = (-2, 0, 0)    C = (-1, 0, 0)    V = (0, 0, 0)
```

```
WORLD SPACE (Room's perspective):
    Z (height)
    ↑
    |
    |     [Keyboard at position (100, 50, 10)]
    |            rotated 45° around Z-axis
    |
    +----------→ Y (depth)
   /
  / X (width)

World positions (after transform):
Q = (97.9, 47.4, 10)    W = (98.6, 48.2, 10)    ...
A = (98.2, 48.2, 10)    S = (98.9, 49.0, 10)    ...
Z = (98.5, 49.0, 10)    X = (99.2, 49.8, 10)    ...
```

## Same Units, Different Origins

**Important:** Both use **grid units (1 unit = 10mm)**, but:

| Aspect | Local Position | World Position |
|--------|---------------|----------------|
| **Units** | Grid units (1 = 10mm) | Grid units (1 = 10mm) ✓ Same! |
| **Origin** | Controller center (0,0,0) | Room corner (0,0,0) ✗ Different! |
| **Changes** | Never (fixed layout) | Yes (when controller moves/rotates) |
| **Purpose** | Define controller layout | Position in room |
| **Used by** | Viewport rendering | Effect calculations |

## Why Not Just Use World Position?

**We could**, but then:

❌ **Every time you move a controller**, you'd have to recalculate ALL LED positions
❌ **No reusable layouts** - each controller would need custom LED positions
❌ **No rotation** - can't easily rotate a controller without recalculating everything
❌ **No prefabs** - can't have "LED strip template" that works anywhere

**With local + world:**

✅ **Controller layouts are reusable** - local positions define the pattern
✅ **Transforms are simple** - just update position/rotation, world positions auto-calculate
✅ **Efficient** - only recalculate world positions when transform changes
✅ **Flexible** - same LED strip layout can be placed anywhere, rotated, scaled

## The Grid Coordinate System

**Both local and world positions use the same underlying grid system:**

- **Corner-Origin System:** Front-left-floor corner = (0,0,0)
- **Grid Units:** 1 unit = 10mm (configurable via `grid_scale_mm`)
- **Axes:**
  - X = Width (left to right, 0 → positive)
  - Y = Depth (front to back, 0 → positive)
  - Z = Height (floor to ceiling, 0 → positive)

**The only difference is the origin point:**
- Local: Controller's center is (0,0,0)
- World: Room's corner is (0,0,0)

## Summary

**Local Position:**
- Fixed LED layout within controller
- Centered at controller origin
- Never changes
- Used for rendering and transforms

**World Position:**
- LED position in room space
- Calculated from local + transform
- Changes when controller moves/rotates
- Used for effects and spatial queries

**Both use grid units, but different coordinate origins!**

This separation gives us the **flexibility** to move, rotate, and scale controllers without redefining their LED layouts. It's the same concept used in all 3D graphics engines (Unity, Unreal, etc.) - you need both object-local space and world space!
