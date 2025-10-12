# Coordinate System Update - Corner-Origin System

**Date:** 2025-10-10
**Status:** ✅ IMPLEMENTED

---

## 🎯 What Changed

The plugin now uses a **corner-origin coordinate system** with the origin (0,0,0) at the **front-left-floor corner** of the room.

---

## 📐 New Coordinate System

### **Origin Point: (0,0,0) = Front-Left-Floor Corner**

Stand in your room facing forward:
- **Origin** = Where the front wall, left wall, and floor all meet
- All measurements start from this corner

### **Axis Directions**

```
             Rear/Back
               ↑ +Y
               |
               |
Left ←---------+--------→ Right
               |        +X
               |
            (0,0,0)
        Front-Left-Floor Corner

        +Z = Up from floor
```

- **X-axis (Width)**: 0 (left wall) → positive (right wall)
- **Y-axis (Depth)**: 0 (front wall) → positive (back wall)
- **Z-axis (Height)**: 0 (floor) → positive (ceiling)

---

## 🆕 New Features

### **1. Manual Room Dimensions**

You can now set your room size manually!

**UI Location:** Grid Settings Tab

```
☑ Use Manual Room Size
Width (X):  [3668] mm  ← Your room width (left to right)
Depth (Y):  [2423] mm  ← Your room depth (front to back)
Height (Z): [2723] mm  ← Your room height (floor to ceiling)
```

**When to use:**
- ✅ You know your exact room dimensions
- ✅ You want effects to travel across the full room (not just LED positions)
- ✅ You have LEDs that don't cover the whole room
- ✅ You want LEDs outside the room (hallway, door, etc.)

**When NOT to use:**
- ❌ You want to experiment quickly (use auto-detect)
- ❌ Your LEDs ARE your room boundaries

### **2. Auto-Detect Mode (Default)**

Leave "Use Manual Room Size" **unchecked** to auto-detect from LED positions.

**How it works:**
1. System finds the furthest LED in each direction
2. Creates a bounding box from those positions
3. Uses that as room dimensions

**Good for:** Quick setup, testing, temporary arrangements

---

## 📏 How to Measure Your LEDs

### **What You're Measuring**

The **CENTER** of each controller (device), measured from the front-left-floor corner.

### **Step-by-Step Example: Keyboard**

**1. Find keyboard dimensions:**
- Width: 30cm
- Depth: 10cm
- Height: 2cm (thickness)

**2. Measure from corner to keyboard's LEFT EDGE:**
- From left wall to keyboard's left edge: 40cm

**3. Add half the width:**
- Keyboard center X = 40cm + (30cm ÷ 2) = 40 + 15 = **55cm**

**4. Repeat for Y and Z:**
- Front wall to front edge: 50cm → Center Y = 50 + 5 = **55cm**
- Floor to desk: 75cm → Center Z = 75 + 1 = **76cm**

**5. Convert to grid units:**
- If grid scale = 10mm/unit
- Position X = **55** (grid units)
- Position Y = **55**
- Position Z = **76**

### **Quick Conversion**

| Grid Scale | What It Means | Example |
|------------|---------------|---------|
| 1mm/unit | Direct millimeters | Position 500 = 500mm |
| 10mm/unit | Centimeters | Position 50 = 500mm (50cm) |
| 25.4mm/unit | Inches | Position 20 = 508mm (20 inches) |
| 100mm/unit | Decimeters | Position 5 = 500mm (5dm) |

---

## 🎨 How Effects Work Now

### **With Manual Room Size (Recommended)**

**Example: Wipe Effect Left-to-Right**

**Your Setup:**
- Room width = 3668mm (X-axis)
- Keyboard at X=500mm
- Monitor at X=1000mm
- Wall strip at X=3600mm

**What Happens:**
1. Wipe starts at X=0 (left wall)
2. At 13.6% progress → Keyboard lights up (500 ÷ 3668)
3. At 27.3% progress → Monitor lights up (1000 ÷ 3668)
4. At 98.1% progress → Wall strip lights up (3600 ÷ 3668)
5. Wipe ends at X=3668mm (right wall)

**The wipe travels across the ROOM, not just the LEDs!**

### **With Auto-Detect**

**Example: Same Setup**

**What Happens:**
1. System detects max X = 3600mm (wall strip)
2. Wipe starts at X=0 (leftmost position)
3. Wipe ends at X=3600mm (rightmost LED)
4. Wipe appears instant if LEDs are close together!

**The wipe travels between the LEDs only.**

---

## 🔧 What Was Changed in Code

### **Files Modified:**

1. **`OpenRGB3DSpatialTab.h`**
   - Added room dimension variables
   - Added UI control pointers

2. **`OpenRGB3DSpatialTab.cpp`**
   - Added room dimension UI controls (lines 570-612)
   - Added signal connections (lines 639-657)
   - Save room dimensions to profile (lines 2768-2771)
   - Load room dimensions from profile (lines 2957-3008)

3. **`OpenRGB3DSpatialTab_EffectStackRender.cpp`**
   - Complete rewrite of grid bounds calculation (lines 40-127)
   - Now uses manual room size OR auto-detect
   - Origin fixed at (0,0,0) front-left-floor corner
   - Updated logging messages

4. **`LEDViewport3D.cpp`**
   - Disabled floor constraint function (line 1368-1374)
   - Y-axis no longer locked to positive values

---

## 🧪 Testing Checklist

### **Before Testing:**
```bash
cd D:\MCP\OpenRGB3DSpatialPlugin
qmake OpenRGB3DSpatialPlugin.pro
nmake  # or make on Linux/Mac
```

### **Test 1: Manual Room Size**
1. ✅ Enable "Use Manual Room Size"
2. ✅ Set Width=3668, Depth=2423, Height=2723
3. ✅ Place LED at X=50, Y=50, Z=50
4. ✅ Run Wipe left-to-right
5. ✅ Wipe should start at left wall and end at right wall
6. ✅ Check logs: "Room bounds (MANUAL)"

### **Test 2: Auto-Detect**
1. ✅ Disable "Use Manual Room Size"
2. ✅ Add LEDs at various positions
3. ✅ Run any effect
4. ✅ Check logs: "Room bounds (AUTO-DETECT)"
5. ✅ Verify bounds match LED positions

### **Test 3: Position Measurements**
1. ✅ Place controller at (0,0,0)
2. ✅ Should appear at front-left-floor corner in viewport
3. ✅ Place controller at (366, 242, 272) with 10mm scale
4. ✅ Should appear at center of room (3660mm, 2420mm, 2720mm)

### **Test 4: Save/Load**
1. ✅ Set manual room dimensions
2. ✅ Save layout profile
3. ✅ Close OpenRGB
4. ✅ Reopen and load profile
5. ✅ Room dimensions should be restored

### **Test 5: Reference Points**
1. ✅ Create user reference point at (100, 80, 118)
2. ✅ Should appear at that position in viewport
3. ✅ Run effect with "User Position" origin
4. ✅ Effect should emanate from that point

---

## ⚠️ Breaking Changes

### **Position Values Changed Meaning**

**Before (Center-Origin):**
- X=0 was center of room
- Negative values were left of center
- Positive values were right of center

**After (Corner-Origin):**
- X=0 is left wall
- All values are positive (from corner)
- X=half_room_width is now center

**What This Means:**
- ⚠️ Existing layouts will need position adjustments
- ⚠️ Controllers may appear in different locations
- ⚠️ Reference points will be offset

**Migration Path:**
1. Load old layout
2. Note where controllers appear
3. Remeasure from corner
4. Update positions
5. Save as new layout

---

## 📖 User Guide (Simple Version)

### **Quick Start: Measure Your Room**

**1. Find the front-left corner**
- Pick which wall is "front" (door? window? doesn't matter!)
- Find where front wall meets left wall meets floor
- This is (0,0,0)

**2. Measure your room**
- Width: left wall to right wall
- Depth: front wall to back wall
- Height: floor to ceiling
- Enter in millimeters (or use grid scale for other units)

**3. Enable manual room size**
- ☑ Use Manual Room Size
- Enter your measurements
- Effects now travel across full room!

**4. Position your LEDs**
- For each controller, measure from corner to its CENTER
- Enter in grid units (divide mm by grid scale)
- Use viewport to verify placement

**5. Test an effect**
- Add Wipe effect to stack
- Set axis to X (left-right)
- Watch it travel from left wall to right wall!

---

## 🎓 Advanced Tips

### **Tip 1: Grid Scale for Easy Math**

- **Grid Scale = 10mm:** Think in centimeters
- **Grid Scale = 1mm:** Think in millimeters (precise!)
- **Grid Scale = 25.4mm:** Think in inches (imperial)
- **Grid Scale = 100mm:** Think in decimeters (quick estimates)

### **Tip 2: LEDs Outside Room**

Want an LED in the hallway? Use it beyond room bounds!

**Example:**
- Room depth = 2423mm (Y: 0 to 2423)
- Door LED in hallway = Y: **-300** (30cm outside front wall)
- Works perfectly! Effects can extend beyond room.

### **Tip 3: Measuring Complex Shapes**

For devices with complex LED arrangements:
1. Measure the device center roughly
2. Enter position
3. Look at viewport
4. Adjust ±5-10 units until it looks right
5. Close enough is perfect!

### **Tip 4: Reference Points**

- User head position: Sit normally, measure to your eyes
- TV center: Measure to screen center
- Speaker: Measure to speaker cone
- Custom: Any interesting origin point!

---

## 🆘 Troubleshooting

### **"My LEDs are in the wrong place!"**

**Check:**
1. Did you measure to CENTER of device (not edge)?
2. Is grid scale correct (10mm = default)?
3. Did you multiply/divide correctly?
4. Check viewport - does it roughly match reality?

**Fix:** Adjust positions ±10 units and re-check

### **"Wipe effect is instant!"**

**Cause:** Using auto-detect and LEDs are close together

**Fix:** Enable manual room size and set actual room dimensions

### **"Effect doesn't reach all LEDs!"**

**Check:**
1. Are LEDs outside room bounds?
2. Is manual room size smaller than LED positions?

**Fix:** Increase room dimensions OR use auto-detect

### **"Y-axis still won't go negative!"**

**Check:** Did you rebuild after update?

**Fix:**
```bash
cd D:\MCP\OpenRGB3DSpatialPlugin
nmake clean
qmake
nmake
```

---

## 📚 Related Documentation

- **COORDINATE_SYSTEM.md** - Technical deep dive
- **IMPLEMENTATION_COMPLETE.md** - Recent fixes summary
- **FIXES_APPLIED_2025-10-10.md** - Detailed changelog

---

## ✅ Summary

**New coordinate system:**
- Origin at front-left-floor corner (0,0,0)
- All positions measured from this corner
- Manual room size OR auto-detect
- Position sliders use grid scale
- Effects travel across full room
- LEDs can be outside room bounds

**To use:**
1. Measure room dimensions (optional)
2. Enable manual room size (optional)
3. Measure LEDs from corner
4. Enter positions in grid units
5. Enjoy room-scale effects! 🎉

---

**Questions? Check the other docs or ask for help!**
