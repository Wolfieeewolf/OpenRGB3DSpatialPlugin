# Reference Point System

## Overview

The Reference Point System allows effects to originate from specific spatial locations in your room, making lighting effects more immersive and contextually aware.

## User Head Position

### Concept

The **User Head Position** represents where the user's eyes/head are located in 3D space. This is the primary reference point for perception-based effects.

**Why the head?**
- The head/eyes are the perception origin - what you see and experience
- Effects calculated from head position feel natural and immersive
- Future game integration will map in-game camera to head position
- Enables directional effects (explosion to your left, pulse from above, etc.)

### Visualization

The user head is visualized in the 3D viewport as a **green smiley face**:
- Circle represents the head
- Two dots for eyes
- Curved line for smile
- Always faces forward (toward negative Z)

### Default Behavior

**Mode: Room Center (Default)**
- All effects originate from room center (0, 0, 0)
- Traditional behavior - simple and predictable
- Good for symmetrical setups

**Mode: User Head Position**
- Effects originate from user head location
- Perception-based - effects feel like they're happening "from your perspective"
- Required for immersive, directional effects

### How to Enable

1. **Position your head** using X/Y/Z spin boxes
   - X: Left (-) to Right (+)
   - Y: Floor (0) to Ceiling (+)
   - Z: Front (-) to Back (+)

2. **Check "Use User Head as Effect Origin"**
   - Effects now calculate from your head position
   - Works with all effects automatically

3. **Adjust in real-time**
   - Move the head position sliders
   - Effects update immediately
   - See visual feedback in 3D viewport

## Effect Architecture

### Levels of Control

**Level 1: Global Default (Current Implementation)**
```
☐ Use User Head as Effect Origin
```
- Applies to all effects by default
- Simple on/off toggle
- Effects automatically use the selected mode

**Level 2: Per-Effect Override (Future)**
```
Effect Origin: [Global Default ▼]
              [User Head Position]
              [Room Center]
              [Custom Point...]
```
- Individual effects can override global setting
- Advanced users can mix and match
- Still respects global default if not overridden

**Level 3: Multi-Point Effects (Future)**
```
Start Point: [User Head ▼]
End Point:   [Monitor   ▼]
```
- Effects like "Lightning Arc" need two points
- "Network Pulse" connects multiple points
- Full reference point library available

## Code Implementation

### Base Class Support

All effects inherit from `SpatialEffect3D`, which provides:

```cpp
// Get the effect origin based on current mode
Vector3D origin = GetEffectOrigin();

// Returns:
// - (0, 0, 0) if REF_MODE_ROOM_CENTER
// - User head position if REF_MODE_USER_POSITION
// - Custom point if REF_MODE_CUSTOM_POINT (future)
```

### Example Usage in Effects

```cpp
RGBColor Wave3D::CalculateColor(float x, float y, float z, float time)
{
    // Get effect origin (room center or user head)
    Vector3D origin = GetEffectOrigin();

    // Calculate distance from origin to LED
    float dx = x - origin.x;
    float dy = y - origin.y;
    float dz = z - origin.z;
    float distance = sqrtf(dx*dx + dy*dy + dz*dz);

    // Wave propagates from origin outward
    float wave = sinf(distance * 0.5f - time);

    return GetColorAtPosition(wave);
}
```

## Future: Game Integration

The reference point system is designed with game integration in mind:

### Vision

```
Game: Creeper explodes at position (5, 2, -3) relative to player
  ↓
Plugin API receives: effect="explosion", world_pos=(5, 2, -3)
  ↓
System calculates: "That's behind and to your left"
  ↓
Triggers Explosion3D effect from that spatial location
  ↓
LEDs behind and to your left light up with explosion
```

### Implementation Ready

- User head = game camera position
- Game sends relative positions (forward/back/left/right)
- System translates to room coordinates
- Effects trigger from correct location

### API Sketch (Future)

```cpp
void TriggerSpatialEffect(string effect_name, Vector3D game_position)
{
    // Convert game coordinates to room space relative to user head
    Vector3D room_pos = ConvertGameToRoomSpace(game_position, user_head_position);

    // Create effect with custom reference point
    SpatialEffect3D* effect = CreateEffect(effect_name);
    effect->SetCustomReferencePoint(room_pos);
    effect->SetUseCustomReference(true);
    effect->Start();
}
```

## Tips for Users

### Finding Your Head Position

1. Sit in your normal gaming/work position
2. Estimate where your head is in the room grid
3. Adjust X/Y/Z until the green head appears at your actual location
4. Enable "Use User Head as Effect Origin"
5. Test with a simple effect like "Wave" or "Explosion"

### Best Practices

- **Start simple**: Test with room center first, then try user head mode
- **Match reality**: Position the head where you actually sit
- **Per-effect later**: Most effects work great with global setting
- **Save layouts**: Different head positions for different chairs/setups
- **Visual feedback**: Keep "Show User Head" enabled while positioning

### Effect Examples

**Explosion from User Head**
- Set head position to your chair
- Enable user head mode
- Run Explosion effect
- Feels like explosion radiates from your perspective

**Wave Across Room**
- User at (0, 0, 0) in center
- Wave sweeps through space
- You see it pass by your position

**Ceiling Lightning** (Future)
- Reference points: User Head, Ceiling Center
- Lightning effect between two points
- Dramatic overhead electrical arc

## Technical Notes

### Coordinate System

- **Origin**: Room center (0, 0, 0) by convention
- **X Axis**: Left wall (-X) to Right wall (+X)
- **Y Axis**: Floor (0) to Ceiling (+Y)
- **Z Axis**: Front wall (-Z) to Rear wall (+Z)
- **User Head**: Can be positioned anywhere in this space

### Performance

- Reference point calculation is negligible overhead
- Same performance as room center mode
- No additional memory usage
- Fully compatible with all existing effects

### Compatibility

- **Backwards compatible**: Default is room center (existing behavior)
- **Effect agnostic**: All effects support reference points automatically
- **Layout saving**: User head position saved/loaded with layouts
- **Future-proof**: Architecture supports multi-point effects

---

**This system transforms static lighting into dynamic, perception-aware experiences! 🌟**
