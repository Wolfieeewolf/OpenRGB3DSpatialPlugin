# Stack Presets Design Document

## Overview
Stack Presets allow users to save complete multi-effect configurations as reusable "scenes" that can be loaded from the Effects tab.

## User Workflow

### Creating a Stack Preset
1. Go to **Effect Stack** tab
2. Build a stack with multiple effects (e.g., Wave3D on Desk, Plasma3D on Wall, etc.)
3. Configure each effect's settings, zones, and blend modes
4. Click **"Save Stack As..."** button
5. Enter a name (e.g., "My Cool Room")
6. Preset is saved to `StackPresets/my_cool_room.stack.json`
7. Preset appears in:
   - **Effect Stack tab** → Saved Stack Presets list
   - **Effects tab** → Effects dropdown as "My Cool Room [Stack]"

### Loading a Stack Preset (from Effect Stack tab)
1. Go to **Effect Stack** tab
2. Select preset from "Saved Stack Presets" list
3. Click **"Load"** button
4. Current stack is replaced with saved preset
5. Effects start running automatically

### Using a Stack Preset (from Effects tab)
1. Go to **Effects** tab
2. Select "My Cool Room [Stack]" from dropdown
3. UI shows **simplified controls**:
   - Just **Start** and **Stop** buttons
   - No settings (preset is pre-configured)
4. Click **Start**:
   - Loads preset into Effect Stack tab
   - Starts rendering automatically
5. Click **Stop**:
   - Stops effect timer
   - Clears Effect Stack

### Editing a Stack Preset
1. Go to **Effect Stack** tab
2. Load the preset
3. Modify effects/settings as needed
4. Click **"Save Stack As..."** with same name
5. Confirm overwrite

### Deleting a Stack Preset
1. Go to **Effect Stack** tab
2. Select preset from list
3. Click **"Delete"** button
4. Confirm deletion
5. Preset removed from:
   - Saved Stack Presets list
   - Effects tab dropdown
   - Disk (.stack.json file deleted)

## Architecture

### Data Structures

#### StackPreset3D
```cpp
struct StackPreset3D {
    std::string name;                                      // User-friendly name
    std::vector<std::unique_ptr<EffectInstance3D>> effect_instances;

    nlohmann::json ToJson() const;
    static std::unique_ptr<StackPreset3D> FromJson(const nlohmann::json& j);
    static std::unique_ptr<StackPreset3D> CreateFromStack(...);
};
```

#### EffectInstance3D (existing)
```cpp
struct EffectInstance3D {
    std::string name;
    std::string effect_class_name;
    std::unique_ptr<SpatialEffect3D> effect;
    int zone_index;          // -1 = All Controllers
    BlendMode blend_mode;
    bool enabled;
    int id;
};
```

### File Storage
- **Location**: `~/.config/OpenRGB/plugins/OpenRGB3DSpatialPlugin/StackPresets/`
- **Format**: `{preset_name}.stack.json`
- **Content**:
```json
{
    "name": "My Cool Room",
    "effects": [
        {
            "name": "Wave3D",
            "effect_type": "Wave3D",
            "zone_index": 0,
            "blend_mode": 1,
            "enabled": true,
            "effect_settings": {
                "speed": 50,
                "brightness": 100,
                "colors": [[255, 0, 0], [0, 255, 0]]
            }
        },
        {
            "name": "Plasma3D",
            "effect_type": "Plasma3D",
            "zone_index": 1,
            "blend_mode": 2,
            "enabled": true,
            "effect_settings": { ... }
        }
    ]
}
```

## UI Layout

### Effect Stack Tab
```
┌─ Effect Stack Tab ─────────────────────────────────────┐
│ Active Effect Stack:                                    │
│ ☑ = enabled, ☐ = disabled. Double-click to toggle.     │
│ ┌────────────────────────────────────────────────────┐ │
│ │ ☑ Wave3D - Desk - Add                             │ │
│ │ ☑ Plasma3D - Front Wall - Multiply                │ │
│ │ ☑ Spiral3D - All - Screen                         │ │
│ └────────────────────────────────────────────────────┘ │
│         [+ Add Effect]  [- Remove Effect]              │
│                                                         │
│ Selected Effect Settings                                │
│ ┌────────────────────────────────────────────────────┐ │
│ │ Effect Type: [Wave3D ▼]                           │ │
│ │ Target Zone: [Desk ▼]                             │ │
│ │ Blend Mode:  [Add ▼]                              │ │
│ │ ... effect controls ...                            │ │
│ └────────────────────────────────────────────────────┘ │
│                                                         │
│ ───────────────────────────────────────────────────────│
│                                                         │
│ Saved Stack Presets:                                    │
│ ┌────────────────────────────────────────────────────┐ │
│ │ My Cool Room                                       │ │
│ │ Gaming Setup                                       │ │
│ │ Chill Vibes                                        │ │
│ └────────────────────────────────────────────────────┘ │
│  [Save Stack As...]  [Load]  [Delete]                  │
└─────────────────────────────────────────────────────────┘
```

### Effects Tab (when regular effect selected)
```
┌─ Effects Tab ──────────────────────────────────────────┐
│ Effect: [Wave3D ▼]                                      │
│                                                         │
│ [Start Effect]  [Stop Effect]                          │
│                                                         │
│ Effect Settings:                                        │
│ ┌────────────────────────────────────────────────────┐ │
│ │ Speed: [========] 50                               │ │
│ │ Brightness: [==========] 100                       │ │
│ │ ... etc ...                                        │ │
│ └────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

### Effects Tab (when stack preset selected)
```
┌─ Effects Tab ──────────────────────────────────────────┐
│ Effect: [My Cool Room [Stack] ▼]                       │
│                                                         │
│ [Start Effect]  [Stop Effect]                          │
│                                                         │
│ This is a saved stack preset with pre-configured       │
│ settings. Click Start to load and run all effects.     │
│                                                         │
│ To edit this preset, go to the Effect Stack tab,       │
│ load it, modify it, and save with the same name.       │
└─────────────────────────────────────────────────────────┘
```

## Implementation Plan

### Phase 1: Core Infrastructure ✅
- [x] Create StackPreset3D data structure
- [x] Implement ToJson/FromJson serialization
- [x] Add UI to Effect Stack tab (list + buttons)
- [x] Implement Save/Load/Delete handlers
- [x] File persistence (.stack.json files)

### Phase 2: Effects Tab Integration (IN PROGRESS)
- [ ] Load presets on startup
- [ ] Register presets in EffectListManager with [Stack] suffix
- [ ] Populate Effects tab dropdown with presets
- [ ] Detect when preset is selected (vs regular effect)
- [ ] Show simplified UI for presets
- [ ] Implement Start button for presets (load stack + start timer)
- [ ] Implement Stop button for presets (stop timer + clear stack)

### Phase 3: Prevent Stacking Presets
- [ ] Filter presets from Effect Stack tab dropdown
- [ ] Only show individual effects in "Effect Type" combo
- [ ] Presets only appear in Effects tab, not Effect Stack tab

### Phase 4: Polish
- [ ] Error handling for corrupt preset files
- [ ] Visual distinction for presets in dropdown ([Stack] suffix)
- [ ] Update Effects tab when presets are saved/deleted
- [ ] Tooltips and help text

## Technical Challenges

### Challenge 1: Effect Registration
Stack presets need to appear in the Effects dropdown alongside regular effects. This requires:
- Extending EffectListManager3D to support "virtual" effects
- OR: Maintaining a separate list and merging at UI level

**Solution**: Maintain separate list, merge in UI dropdown population.

### Challenge 2: Effect Type Detection
When user selects an item from Effects dropdown, need to determine:
- Is it a regular effect? → Show full settings UI
- Is it a stack preset? → Show simplified UI

**Solution**: Store preset flag or check for "[Stack]" suffix in name.

### Challenge 3: Deep Copy
Loading a preset must create deep copies of all EffectInstance3D objects:
- Can't share effect instances between preset and active stack
- Settings changes shouldn't affect saved preset

**Solution**: Use ToJson/FromJson round-trip for deep copy.

### Challenge 4: UI State Management
Need to track:
- Which effect/preset is currently selected
- Whether effect is from preset or manual
- When to show full UI vs simplified UI

**Solution**: Add `current_effect_is_preset` bool flag.

## Testing Checklist

### Basic Functionality
- [ ] Create stack with 3 effects, save as preset
- [ ] Load preset from Effect Stack tab
- [ ] Verify all 3 effects restored with correct settings
- [ ] Delete preset, verify removed from list and disk

### Effects Tab Integration
- [ ] Preset appears in Effects dropdown with [Stack] suffix
- [ ] Selecting preset shows simplified UI
- [ ] Start button loads preset and starts rendering
- [ ] Stop button stops rendering
- [ ] Cannot add preset to Effect Stack tab

### Edge Cases
- [ ] Save preset with same name (overwrite)
- [ ] Load preset with no controllers in scene
- [ ] Delete preset while it's running
- [ ] Corrupt .stack.json file handling
- [ ] Empty stack (shouldn't allow save)
- [ ] Preset with disabled effects

### Cross-Tab Behavior
- [ ] Save preset in Effect Stack tab → appears in Effects tab
- [ ] Delete preset in Effect Stack tab → removed from Effects tab
- [ ] Load preset from Effects tab → visible in Effect Stack tab
- [ ] Modify running preset → doesn't affect saved version

## Future Enhancements
- Export/import presets for sharing
- Preset thumbnails/previews
- Preset categories/tags
- Preset search/filter
- Preset duplication
- Preset descriptions/notes
