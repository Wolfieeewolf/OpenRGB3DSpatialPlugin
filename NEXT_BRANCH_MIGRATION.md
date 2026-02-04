# Migration to OpenRGB API v5 (`next_profile_updates`)

This document tracks the changes needed to support OpenRGB's `next_profile_updates` branch (API v5).

## Major Changes in OpenRGB API v5

### 1. **New Plugin Profile API**
Plugins can now save/load state as part of the main OpenRGB profile:
```cpp
virtual void OnProfileAboutToLoad() = 0;
virtual void OnProfileLoad(nlohmann::json profile_data) = 0;
virtual nlohmann::json OnProfileSave() = 0;
```

### 2. **RGBController Members Now Protected**
Direct member access is **BREAKING**. Must use getters:
- `controller->name` → `controller->GetName()`
- `controller->description` → `controller->GetDescription()`
- `controller->location` → `controller->GetLocation()`
- `controller->vendor` → `controller->GetVendor()`
- `controller->serial` → `controller->GetSerial()`
- `controller->version` → `controller->GetVersion()`
- `controller->type` → `controller->GetType()`
- `controller->zones` → `controller->GetZones()` (returns `const std::vector<zone>&`)
- `controller->leds` → `controller->GetLEDs()` (returns `const std::vector<led>&`)
- `controller->colors` → `controller->GetColors()` (returns `std::vector<RGBColor>&`)
- `controller->modes` → `controller->GetModes()` (returns `const std::vector<mode>&`)
- `controller->active_mode` → `controller->GetActiveMode()` / `controller->SetActiveMode(int)`
- `controller->flags` → `controller->GetFlags()`

### 3. **ProfileManager Now Uses JSON**
- Profiles stored as `.json` (not binary `.orp`/`.ors`)
- Auto-load profiles (open, exit, resume, suspend) in ProfileManager settings
- `SaveProfile()` / `LoadProfile()` API changed

### 4. **SDK v6**
- Unique controller IDs
- Settings API via SDK
- Per-zone modes

---

## Files Requiring Updates

### Plugin Core
- [x] `OpenRGB3DSpatialPlugin.h` - Add new profile API methods
- [x] `OpenRGB3DSpatialPlugin.cpp` - Implement profile API methods

### Files with Direct Member Access (BREAKING)
- [ ] `VirtualController3D.cpp` (3 occurrences)
  - Line 119: `pos.controller->zones.size()` → `pos.controller->GetZones().size()`
  - Line 159: `controller->name` → `controller->GetName()`
  - Line 160: `controller->location` → `controller->GetLocation()`

- [ ] `ui/OpenRGB3DSpatialTab_ObjectCreator.cpp` (33 occurrences)
  - Line 783: `controller->name` → `controller->GetName()`
  - Line 788: `controller->zones.size()` → `controller->GetZones().size()`
  - Line 792: `controller->zones[i].name` → `controller->GetZones()[i].name`
  - Line 798: `controller->leds.size()` → `controller->GetLEDs().size()`
  - Line 802: `controller->leds[i].name` → `controller->GetLEDs()[i].name`
  - Lines 1069, 1073, 1082, 1093, 1097, 1109, 1117: Similar patterns
  - Lines 2570-2572, 2945, 2979, 3039-3070, 3552, 3566, 3578, 3633: More patterns

- [ ] `ui/OpenRGB3DSpatialTab_Audio.cpp` (1 occurrence)
  - Line 568: `controller->name` → `controller->GetName()`

- [ ] `ui/CustomControllerDialog.cpp` (52 occurrences)
  - Lines 353-375: `controller->zones`, `controller->leds` access
  - Lines 612-641: Multiple member accesses in tooltip generation
  - More throughout file

- [ ] `ui/LEDViewport3D.cpp` (5 occurrences)
  - Check for `controller->` member access

- [ ] `ControllerLayout3D.cpp` (6 occurrences)
  - Check for `controller->zones`, `controller->type`, `controller->leds` access

- [ ] `ui/OpenRGB3DSpatialTab_Zones.cpp` (7 occurrences)
  - Check for `controller->` member access

- [ ] `ui/OpenRGB3DSpatialTab_EffectStackRender.cpp` (10 occurrences)
  - Check for `controller->colors`, `controller->zones` access

---

## Implementation Plan

### Phase 1: Add Profile API (Required for Compilation)
1. Update `OpenRGB3DSpatialPlugin.h` with new methods
2. Implement stub methods in `OpenRGB3DSpatialPlugin.cpp`
3. Wire up to existing save/load system

### Phase 2: Replace Direct Member Access (BREAKING CHANGES)
1. VirtualController3D.cpp
2. OpenRGB3DSpatialTab_ObjectCreator.cpp
3. CustomControllerDialog.cpp
4. LEDViewport3D.cpp
5. ControllerLayout3D.cpp
6. OpenRGB3DSpatialTab_Zones.cpp
7. OpenRGB3DSpatialTab_EffectStackRender.cpp
8. OpenRGB3DSpatialTab_Audio.cpp

### Phase 3: Test & Validate
1. Build and fix any remaining compilation errors
2. Test layout save/load
3. Test effect stack
4. Test custom controllers
5. Test profile integration

### Phase 4: Save/Load Audit
Once compilation works, audit all save/load paths per CONTRIBUTING.md

---

## Status
- [x] Branch created: `next`
- [x] Submodule updated to `next_profile_updates` (2105319d)
- [ ] Phase 1: Profile API
- [ ] Phase 2: Member access migration
- [ ] Phase 3: Testing
- [ ] Phase 4: Save/load audit
