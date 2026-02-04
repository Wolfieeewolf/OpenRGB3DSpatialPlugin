# Migration to OpenRGB API v5 (`next_profile_updates`)

This document tracks the changes needed to support OpenRGB's `next_profile_updates` branch (API v5).

## Major Changes in OpenRGB API v5

### 1. **New Plugin Profile API**
Plugins can now save/load state as part of the main OpenRGB profile:
```cpp
virtual void OnProfileAboutToLoad() = 0;
virtual void OnProfileLoad(nlohmann::json profile_data) = 0;
virtual nlohmann::json OnProfileSave() = 0;
virtual unsigned char* OnSDKCommand(unsigned int command, unsigned char* data, unsigned int size, unsigned int* out_size) = 0;
```

### 2. **RGBController Per-Item API**
The vector-returning methods have been replaced with per-item accessors:
- `controller->GetZones()` → `controller->GetZoneCount()`, `controller->GetZoneName(idx)`, `controller->GetZoneStartIndex(idx)`
- `controller->GetLEDs()` → `controller->GetLEDCount()`, `controller->GetLEDName(idx)`
- `controller->GetColors()` → `controller->GetColor(idx)`, `controller->SetColor(idx, color)`

### 3. **RGBController Members Now Protected**
Direct member access is **BREAKING**. Must use getters:
- `controller->name` → `controller->GetName()`
- `controller->description` → `controller->GetDescription()`
- `controller->location` → `controller->GetLocation()`
- `controller->vendor` → `controller->GetVendor()`
- `controller->serial` → `controller->GetSerial()`
- `controller->version` → `controller->GetVersion()`
- `controller->type` → `controller->GetDeviceType()`

### 4. **ResourceManager API Updates**
- `WaitForDeviceDetection()` → `WaitForDetection()`
- `RegisterDeviceListChangeCallback()` → `RegisterResourceManagerCallback()`
- Callback signature changed to include `unsigned int update_reason` parameter

### 5. **New Dependencies**
- `StringUtils.h/cpp` - String utility functions used by RGBController

---

## Files Updated (15 total)

### ✅ Plugin Core
- [x] `OpenRGB3DSpatialPlugin.h` - Add new profile API methods
- [x] `OpenRGB3DSpatialPlugin.cpp` - Implement profile API methods, update ResourceManager calls
- [x] `OpenRGB3DSpatialPlugin.pro` - Add StringUtils to build

### ✅ Core Components
- [x] `ControllerLayout3D.cpp` - Update to per-item zone/LED API
- [x] `VirtualController3D.cpp` - Update controller matching to use getters

### ✅ UI Components
- [x] `ui/OpenRGB3DSpatialTab.h` - Make LoadLayoutFromJSON public
- [x] `ui/OpenRGB3DSpatialTab_Zones.cpp` - Fix namespace conflict, update to per-item API
- [x] `ui/OpenRGB3DSpatialTab_ObjectCreator.cpp` - Update 24 instances to per-item API
- [x] `ui/OpenRGB3DSpatialTab_EffectStackRender.cpp` - Update 6 instances to per-item API
- [x] `ui/CustomControllerDialog.cpp` - Update 12 instances to per-item API
- [x] `ui/LEDViewport3D.cpp` - Update 3 instances to per-item API

---

## Implementation Summary

### Phase 1: Core API Changes ✅ **COMPLETE**
- [x] Added `OnProfileAboutToLoad()`, `OnProfileLoad()`, `OnProfileSave()`, `OnSDKCommand()` to plugin interface
- [x] Implemented basic profile load for layout JSON
- [x] Updated `WaitForDeviceDetection()` → `WaitForDetection()`
- [x] Updated `RegisterDeviceListChangeCallback()` → `RegisterResourceManagerCallback()`
- [x] Updated callback signature to include `update_reason` parameter
- [x] Added StringUtils to build system

### Phase 2: Per-Item API Migration ✅ **COMPLETE**
All files updated to use the new per-item API:
- Replaced `GetZones()` vector access with `GetZoneCount()` + `GetZoneName(idx)`
- Replaced `GetLEDs()` vector access with `GetLEDCount()` + `GetLEDName(idx)`
- Replaced `GetColors()` vector access with `GetColor(idx)` + `SetColor(idx, color)`
- Used `GetZoneStartIndex(idx)` for LED global index calculations
- Replaced all direct member access with getter methods

**Key Fixes:**
- Fixed namespace conflict in `OpenRGB3DSpatialTab_Zones.cpp` (local `zone` variable vs `OpenRGB::zone` struct)
- Fixed undeclared identifier errors after removing vector references
- Updated all tooltip, info display, and validation logic

### Phase 3: Compilation ✅ **COMPLETE**
- [x] All 15 files successfully updated
- [x] Plugin compiles successfully on `next` branch
- [x] Only remaining warning is from OpenRGB's own code (C4267 in RGBController.cpp:1411)

---

## Next Steps

### Phase 4: Testing & Validation
- [ ] Build OpenRGB from `next_profile_updates` branch
- [ ] Test plugin loading and initialization
- [ ] Test device detection and layout generation
- [ ] Test effect rendering
- [ ] Test custom controllers
- [ ] Test profile save/load

### Phase 5: Full Profile Implementation
- [ ] Implement complete `OnProfileSave()` to save all plugin state:
  - Layout (zones, reference points, display planes)
  - Custom controllers
  - Effect stack
  - Effect profiles
  - Stack presets
- [ ] Implement complete `OnProfileLoad()` to restore all plugin state
- [ ] Test profile persistence across OpenRGB restarts

### Phase 6: Critical Bug Fixes (from SAVE_LOAD_AUDIT.md)
- [ ] Fix zones not being saved
- [ ] Fix reference points not being saved
- [ ] Fix display planes not being saved

---

## Status

✅ **API v5 Migration: COMPILATION COMPLETE**

- [x] Branch created: `next`
- [x] Submodule updated to `next_profile_updates` (2105319d)
- [x] Phase 1: Core API ✅
- [x] Phase 2: Per-Item API Migration ✅
- [x] Phase 3: Compilation ✅
- [ ] Phase 4: Testing & Validation
- [ ] Phase 5: Full Profile Implementation
- [ ] Phase 6: Critical Bug Fixes

**The plugin now compiles successfully with OpenRGB API v5!** 🎉

The `master` branch remains stable with the old API. The `next` branch is ready for testing with OpenRGB's `next_profile_updates` branch.
