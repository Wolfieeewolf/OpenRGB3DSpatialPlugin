# Quick Start Guide - 3D Spatial Setup

**For users with ZERO coding experience!**

---

## 🎯 What You Need

- ✅ Tape measure
- ✅ Calculator (or phone)
- ✅ 5-10 minutes
- ✅ Your room with RGB devices

---

## Step 1: Pick Your Corner (30 seconds)

**Stand in your room.**

Pick which wall will be "front":
- Door wall? ✓
- Window wall? ✓
- TV wall? ✓
- Doesn't matter - just pick one!

**Now find the corner:**
- Where "front" wall meets left wall (when facing forward)
- At floor level
- **This corner is (0,0,0) - your starting point!**

💡 **Tip:** Put a piece of tape there to remember!

---

## Step 2: Measure Your Room (2 minutes)

### **Get 3 measurements:**

**1. Width (left wall to right wall)**
```
Stand at corner, measure across to opposite wall
Example: 12 feet = 366cm = 3660mm
```

**2. Depth (front wall to back wall)**
```
Stand at corner, measure to back wall
Example: 8 feet = 244cm = 2440mm
```

**3. Height (floor to ceiling)**
```
Measure from floor to ceiling
Example: 9 feet = 274cm = 2740mm
```

### **Convert to Millimeters:**

| Feet | Millimeters | Quick Calc |
|------|-------------|------------|
| 8 ft | 2440 mm | feet × 305 |
| 9 ft | 2740 mm | feet × 305 |
| 10 ft | 3050 mm | feet × 305 |
| 12 ft | 3660 mm | feet × 305 |

Or use this: https://www.google.com/search?q=12+feet+to+mm

---

## Step 3: Enter Room Size (30 seconds)

**In OpenRGB 3D Spatial tab:**

1. Click **"Grid Settings"** tab (left side)
2. Check the box: ☑ **Use Manual Room Size**
3. Enter your measurements:
   ```
   Width (X):  [3660] mm  ← Your width
   Depth (Y):  [2440] mm  ← Your depth
   Height (Z): [2740] mm  ← Your height
   ```

Done! Room is configured! 🎉

---

## Step 4: Position Your First LED (3 minutes)

Let's position your keyboard as an example.

### **A. Measure Keyboard Dimensions**

```
Width:  30cm (left to right)
Depth:  10cm (front to back)
Height: 2cm (thickness)
```

### **B. Measure from Corner to Keyboard's LEFT EDGE**

```
From left wall to keyboard's left side: 40cm
From front wall to keyboard's front side: 50cm
From floor to desk surface: 75cm
```

### **C. Calculate CENTER Position**

**X (Width):**
```
Left edge: 40cm
Half of keyboard width: 30cm ÷ 2 = 15cm
Center X = 40 + 15 = 55cm
```

**Y (Depth):**
```
Front edge: 50cm
Half of keyboard depth: 10cm ÷ 2 = 5cm
Center Y = 50 + 5 = 55cm
```

**Z (Height):**
```
Desk height: 75cm
Half of keyboard height: 2cm ÷ 2 = 1cm
Center Z = 75 + 1 = 76cm
```

### **D. Enter Position**

**In OpenRGB:**
1. Select your keyboard controller
2. Go to **"Position & Rotation"** tab
3. Enter positions (÷10 because grid scale is 10mm):
   ```
   Position X: 55  (550mm ÷ 10)
   Position Y: 55  (550mm ÷ 10)
   Position Z: 76  (760mm ÷ 10)
   ```

### **E. Check Viewport**

Look at the 3D view - does keyboard appear roughly where it should be?
- ✅ Yes? Perfect!
- ❌ No? Adjust ±5 units until it looks right

---

## Step 5: Test an Effect! (1 minute)

1. Click **"Effect Stack"** tab
2. Click **"Add Effect"** button
3. Choose **"3D Wipe"**
4. Set **Axis** to **"X"** (left-right)
5. Set **Zone** to **"All Controllers"**
6. Watch the wipe travel from left wall to right wall!

🎉 **It works!**

---

## 🚀 Quick Tips

### **Tip 1: Don't Stress About Precision**

±5cm error? **No problem!**
Room-scale effects look great even with rough measurements.

**Perfect is the enemy of good.** Get close, then adjust if needed.

---

### **Tip 2: Measure to Center, Not Edge**

Always measure to the **CENTER** of each device:
- Not the left edge
- Not the right edge
- The middle point!

---

### **Tip 3: Use the Viewport**

The 3D view is your friend!
- Does it **look roughly right**?
- Then it **IS right**! ✓

---

### **Tip 4: Grid Scale Options**

**Default: 10mm per unit** (think in centimeters)
- Position 50 = 500mm = 50cm

**Change to 1mm per unit** (for precision)
- Position 500 = 500mm = 50cm

**Change to 25.4mm per unit** (for inches!)
- Position 20 = 508mm ≈ 20 inches

Go to Grid Settings → Grid Scale to change.

---

## 📝 Worksheet Template

Print this or write on paper:

```
ROOM MEASUREMENTS:
Width (left to right):  _____ cm = _____ mm
Depth (front to back):  _____ cm = _____ mm
Height (floor to ceiling): _____ cm = _____ mm

CONTROLLER: _________________

Device Dimensions:
  Width:  _____ cm
  Depth:  _____ cm
  Height: _____ cm

From Corner to Device Edge:
  X (from left wall):   _____ cm
  Y (from front wall):  _____ cm
  Z (from floor):       _____ cm

Calculate Center:
  Center X = Edge X + (Width ÷ 2) = _____ cm
  Center Y = Edge Y + (Depth ÷ 2) = _____ cm
  Center Z = Edge Z + (Height ÷ 2) = _____ cm

Enter in OpenRGB (÷10 for grid units):
  Position X: _____
  Position Y: _____
  Position Z: _____
```

---

## 🎬 Example: Full Setup

**My Room:**
- Width: 12 feet = 3660mm
- Depth: 8 feet = 2440mm
- Height: 9 feet = 2740mm

**My Keyboard:**
- Dimensions: 30cm × 10cm × 2cm
- Left edge from corner: 40cm
- Front edge from corner: 50cm
- Desk height: 75cm
- **Center Position: (55, 55, 76)** in grid units

**My Monitor:**
- Dimensions: 60cm × 5cm × 40cm
- Left edge from corner: 100cm
- Front edge from corner: 45cm
- Desk height: 75cm
- Monitor bottom at desk level
- **Center Position: (130, 47.5, 95)** in grid units

**My PC Case (on floor):**
- Dimensions: 20cm × 45cm × 45cm
- Left edge from corner: 200cm
- Front edge from corner: 30cm
- On floor (Z=0)
- **Center Position: (210, 52.5, 22.5)** in grid units

---

## ❓ Common Questions

### **Q: What if I measure to the wrong edge by accident?**
**A:** No problem! The viewport will show it's off. Just adjust ±10-20 units until it looks right.

### **Q: My room has weird angles/alcoves. What do I do?**
**A:** Pick the main rectangular area as your "room." LEDs in alcoves can be outside the bounds - that's fine!

### **Q: I don't know my exact room size. Can I skip measuring?**
**A:** Yes! Uncheck "Use Manual Room Size" and it will auto-detect from your LED positions. Quick but less accurate for effects.

### **Q: What units are the position sliders?**
**A:** Grid units! Default is 10mm per unit (think centimeters). Change in Grid Settings → Grid Scale.

### **Q: Can I use inches instead of millimeters?**
**A:** Yes! Set Grid Scale to 25.4mm/unit, then enter positions in inches directly.

### **Q: Do I need to be exact?**
**A:** NO! ±5-10cm is totally fine. Room-scale effects are forgiving. Get close, check viewport, adjust if needed.

---

## 🎓 Next Steps

Once you have basics working:

1. **Add Reference Points**
   - Your head position (for effects centered on YOU)
   - TV center
   - Speaker positions

2. **Try Different Effects**
   - Wave (creates flowing patterns)
   - Spiral (rotates around center)
   - Explosion (radiates outward)

3. **Use Effect Stack**
   - Layer multiple effects
   - Try different blend modes
   - Save as presets

4. **Create Zones**
   - Group controllers
   - Target effects to specific zones
   - Build complex lighting scenes

---

## 📚 More Help

- **COORDINATE_SYSTEM_UPDATE_2025-10-10.md** - Detailed explanation
- **COORDINATE_SYSTEM.md** - Technical reference
- **IMPLEMENTATION_COMPLETE.md** - What's been fixed

---

## ✅ Checklist

Before you start:
- [ ] Picked your front-left corner
- [ ] Measured room (width, depth, height)
- [ ] Converted to millimeters
- [ ] Entered room size in Grid Settings
- [ ] Enabled "Use Manual Room Size"

For each controller:
- [ ] Measured device dimensions
- [ ] Measured from corner to device edge
- [ ] Calculated center position
- [ ] Entered in Position X/Y/Z
- [ ] Checked viewport - looks right?

Test:
- [ ] Added Wipe effect
- [ ] Effect travels across full room
- [ ] All LEDs light up correctly

---

**You're ready! Go measure and have fun! 🎉**

Need help? Check the other guides or just experiment - you can't break anything!
