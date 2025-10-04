# OpenRGB 3D Spatial Plugin v2.8.0

## 🐛 Critical Bug Fixes

This release addresses critical crashes and UI bugs introduced during the smart pointer migration in v2.7.0.

### Memory Safety Fixes
- **Fixed plugin crash on load**: Initialized all UI pointers to nullptr in constructor to prevent undefined behavior
- **Fixed layout loading crash**: Resolved use-after-move bug where `ctrl_transform` was accessed after `std::move()`
- **Added null pointer safety**: Added comprehensive null checks when iterating smart pointer containers
- **Fixed smart pointer lifecycle**: Save necessary values before moving `unique_ptr` to avoid accessing moved-from objects

### UI Control Priority Fix
- **Fixed slider/spinbox control priority**: Controllers selected via gizmo now correctly respond to position/rotation sliders
- Previously, after selecting a reference point then clicking a controller, sliders would still control the reference point
- Applied fix to all 11 position and rotation controls (X/Y/Z sliders and spinboxes)

### Code Quality
- Updated `LEDViewport3D` to properly use `.get()` for raw pointer access from smart pointers
- Cleaned up temporary debug logging and exception handlers
- Re-enabled auto-load timer after testing

## 📥 Installation

1. Download `OpenRGB3DSpatialPlugin-v2.8.0.zip`
2. Extract `OpenRGB3DSpatialPlugin.dll`
3. Copy to your OpenRGB plugins folder (usually `C:\Program Files\OpenRGB\plugins\`)
4. Restart OpenRGB

## ✅ Testing Status

All core features tested and working:
- ✅ Add controllers to 3D space
- ✅ Create reference points
- ✅ Move/rotate objects with gizmo
- ✅ Control position/rotation with sliders
- ✅ Run spatial effects
- ✅ Save/load layouts

## 🔧 Technical Details

This release completes the smart pointer migration started in v2.7.0, ensuring memory safety while maintaining all existing functionality. The fixes prevent undefined behavior and ensure proper RAII resource management throughout the plugin.

---

🤖 Generated with [Claude Code](https://claude.com/claude-code)
