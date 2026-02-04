# Remaining API v5 Fixes

The `next_profile_updates` branch requires extensive changes. We've completed the core files, but several UI files still need updates.

## Completed ✅
- `ControllerLayout3D.cpp` - Converted to per-zone API
- `VirtualController3D.cpp` - Updated to use getters
- `OpenRGB3DSpatialPlugin.cpp/h` - Fixed callback signature
- `ui/OpenRGB3DSpatialTab.h` - Made LoadLayoutFromJSON public
- `ui/CustomControllerDialog.cpp` - Converted to per-zone API
- `ui/LEDViewport3D.cpp` - Updated getters
- `ui/OpenRGB3DSpatialTab_Audio.cpp` - Updated getters

## Remaining ❌

### ui/OpenRGB3DSpatialTab_ObjectCreator.cpp (24 instances)
**Direct member access:**
- Line 132: `controller->name` → `controller->GetName()`
- Line 788: `controller->GetZones()` → Use `GetZoneCount()`, `GetZoneName(idx)`, etc.
- Line 799: `controller->GetLEDs()` → Use `GetLEDCount()`, `GetLEDName(idx)`, etc.
- Lines 1890-1905: Multiple direct accesses to `name`, `description`, `location`, `vendor`, `serial`, `version`, `type`
- Lines 2950, 3043, 3558, 3574, 3641: `GetZones()` calls
- Line 3044, 3588: `GetLEDs()` calls

**Pattern to replace:**
```cpp
// OLD:
const std::vector<zone>& zones = controller->GetZones();
for(size_t i = 0; i < zones.size(); i++) {
    std::string zone_name = zones[i].name;
    unsigned int led_count = zones[i].leds_count;
}

// NEW:
std::size_t zone_count = controller->GetZoneCount();
for(std::size_t i = 0; i < zone_count; i++) {
    std::string zone_name = controller->GetZoneName((unsigned int)i);
    unsigned int led_count = controller->GetZoneLEDsCount((unsigned int)i);
}
```

### ui/OpenRGB3DSpatialTab_Zones.cpp (CRITICAL - namespace conflict)
**Problem**: Local variable `Zone3D* zone` (line 36) conflicts with OpenRGB's `zone` struct type.

**Solution**:
1. Rename local `Zone3D* zone` variables to `Zone3D* zone3d` or `Zone3D* custom_zone`
2. Update all references in the function
3. Then convert `GetZones()` calls to per-zone API

**Lines affected**: 36, 60-64, 153-156

### ui/OpenRGB3DSpatialTab_EffectStackRender.cpp (6 instances)
- Lines 337-338: `GetZones()`, `GetColors()`
- Lines 365-366: `GetZones()`, `GetColors()`
- Line 388: `GetZones()`

**Pattern for GetColors():**
```cpp
// OLD:
const std::vector<RGBColor>& colors = controller->GetColors();
RGBColor color = colors[led_idx];

// NEW:
RGBColor color = controller->GetColor(led_idx);
```

## API Reference

### Zone Access (no vector getters)
- `GetZoneCount()` → `std::size_t`
- `GetZoneName(unsigned int zone)` → `std::string`
- `GetZoneLEDsCount(unsigned int zone)` → `unsigned int`
- `GetZoneType(unsigned int zone)` → `zone_type`
- `GetZoneStartIndex(unsigned int zone)` → `unsigned int`
- `GetZoneMatrixMap(unsigned int zone)` → `const unsigned int*`
- `GetZoneMatrixMapWidth(unsigned int zone)` → `unsigned int`
- `GetZoneMatrixMapHeight(unsigned int zone)` → `unsigned int`

### LED Access (no vector getters)
- `GetLEDCount()` → `std::size_t`
- `GetLEDName(unsigned int led)` → `std::string`
- `GetLEDValue(unsigned int led)` → `unsigned int`

### Color Access
- `GetColor(unsigned int led)` → `RGBColor`
- `GetColorsPointer()` → `RGBColor*` (for bulk access)

### Device Info
- `GetName()` → `std::string`
- `GetVendor()` → `std::string`
- `GetDescription()` → `std::string`
- `GetVersion()` → `std::string`
- `GetSerial()` → `std::string`
- `GetLocation()` → `std::string`
- `GetDeviceType()` → `device_type`

## Recommendation

This is a **massive refactor** (50+ changes across 3 large files). Options:

1. **Continue manual fixes** - Time-consuming but thorough
2. **Create helper script** - Automated search/replace with validation
3. **Wait for OpenRGB stable release** - `next_profile_updates` may still change

**Current status**: The `next` branch is a work-in-progress migration. The `master` branch remains stable and functional with the current OpenRGB API.
