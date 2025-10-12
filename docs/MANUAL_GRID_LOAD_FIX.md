# Manual Grid Checkbox Load Fix

**Date:** 2025-10-12
**Issue:** Manual room dimensions not applied to viewport on layout load
**Fixed:** Added viewport update call after loading manual room settings

## The Problem

When opening OpenRGB with a saved layout that had "Use Manual Room Size" enabled:
- ✅ The checkbox state was correctly loaded and checked
- ✅ The manual room dimension values were correctly loaded
- ✅ The spin boxes were correctly enabled/disabled
- ❌ **The viewport did NOT update to show the manual room dimensions**

User had to manually toggle the checkbox off and on to trigger the viewport update.

## Root Cause

The layout loading code was:
1. Loading `use_manual_room_size` from JSON ✓
2. Updating the checkbox UI with `setChecked()` ✓
3. Loading room dimension values ✓
4. **Missing:** Calling `viewport->SetRoomDimensions()` to apply the loaded values ✗

When the user toggled the checkbox, the `toggled` signal handler called `viewport->SetRoomDimensions()`, which is why that worked.

## The Fix

**File:** `OpenRGB3DSpatialTab.cpp` (Lines 3132-3136)

Added viewport update call after loading all room dimension values:

```cpp
// Update viewport with loaded manual room dimensions
if(viewport)
{
    viewport->SetRoomDimensions(manual_room_width, manual_room_depth, manual_room_height, use_manual_room_size);
}
```

This matches the same call made in the checkbox `toggled` signal handler (line 650).

## Location in Code

The fix is placed after loading all room settings:
1. Load `use_manual_size` flag (line 3087)
2. Update checkbox UI (lines 3088-3093)
3. Load `width` value (lines 3096-3106)
4. Load `depth` value (lines 3108-3118)
5. Load `height` value (lines 3120-3130)
6. **Update viewport with all loaded values (lines 3132-3136)** ← NEW

## Testing

After this fix, when loading a layout with manual room dimensions:

✅ Checkbox should be checked (if it was saved as checked)
✅ Room dimension spin boxes should show saved values
✅ Spin boxes should be enabled (if manual mode was enabled)
✅ **Viewport should immediately show the manual room grid** (no need to toggle checkbox)

## Related Code

**Checkbox toggle handler (lines 641-652):**
```cpp
connect(use_manual_room_size_checkbox, &QCheckBox::toggled, [this](bool checked) {
    use_manual_room_size = checked;
    if(room_width_spin) room_width_spin->setEnabled(checked);
    if(room_depth_spin) room_depth_spin->setEnabled(checked);
    if(room_height_spin) room_height_spin->setEnabled(checked);

    // Update viewport with new settings
    if(viewport)
    {
        viewport->SetRoomDimensions(manual_room_width, manual_room_depth, manual_room_height, use_manual_room_size);
    }
});
```

**Viewport method (LEDViewport3D.cpp:170):**
```cpp
void LEDViewport3D::SetRoomDimensions(float width, float depth, float height, bool use_manual)
{
    room_width = width;
    room_depth = depth;
    room_height = height;
    use_manual_room_dimensions = use_manual;
    update(); // Trigger viewport redraw with new dimensions
}
```

## Summary

✅ **Fixed:** Viewport now updates with manual room dimensions on layout load
✅ **No more toggling needed:** Manual grid appears correctly on startup
✅ **Consistent behavior:** Same viewport update logic used for both loading and toggling
