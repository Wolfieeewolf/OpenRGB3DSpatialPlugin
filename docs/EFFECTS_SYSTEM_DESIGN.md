# OpenRGB 3D Spatial Effects System - Advanced Design

## 🎯 **Vision: Room-Scale 3D LED Ecosystem**
Create immersive, physics-based lighting effects that respond to real room layout and user-defined reference points.

---

## 🏗️ **System Architecture**

### **Core Components:**
1. **Reference Point System** - User-defined spatial anchors
2. **Multi-Point Effects** - Effects using multiple reference points
3. **Universal Axis Controls** - X/Y/Z/Radial axes for all effects
4. **Effect Direction System** - Forward/reverse controls
5. **3D Grid Mapping** - Room-scale coordinate system

---

## 📍 **Reference Point System**

### **Built-in Reference Points:**
- 👤 **User** - Green stick figure (adjustable position)
- 🖥️ **Monitor** - Primary display
- 🪑 **Chair** - Gaming/office chair
- 🏢 **Desk** - Computer desk surface
- 🔊 **Speakers** - Left/right speaker positions
- 🚪 **Door** - Room entrance
- 🪟 **Window** - Natural light source
- 🛏️ **Bed** - Sleeping area
- 📺 **TV** - Entertainment center
- ⚙️ **Custom** - User-defined points

### **Reference Point Properties:**
```cpp
struct ReferencePoint3D {
    int id;                    // Unique identifier
    string name;               // "Gaming Monitor", "Left Speaker"
    ReferencePointType type;   // Monitor, Chair, etc.
    float x, y, z;            // 3D position in room grid
    bool visible;             // Show/hide in viewport
    RGBColor color;           // Display color
}
```

---

## 🌟 **Multi-Point Effects System**

### **Effect Categories:**

#### **🎆 Single-Point Effects** (Traditional)
- **Explosion** - Radiates from one point
- **Breathing Sphere** - Pulses around one point
- **Spiral** - Rotates around one point

#### **⚡ Dual-Point Effects** (Advanced)
- **Lightning Arc** - Electricity between two points
- **Wipe Between** - Smooth transition point A → point B
- **Tug-of-War** - Competing effects from two sources
- **Orbit** - Effect rotates between two anchor points

#### **🌈 Multi-Point Effects** (Ultra Advanced)
- **Network Pulse** - Connections between all selected points
- **Ripple Storm** - Multiple ripples from each point
- **Point Constellation** - Effects form patterns using all points
- **Zone Influence** - Each point controls nearby LEDs

### **Multi-Point Configuration:**
```cpp
struct MultiPointConfig {
    vector<int> reference_point_ids;  // [monitor, chair, speakers]
    int primary_point_id;            // Main effect origin
    int secondary_point_id;          // Secondary origin
    bool use_all_points;             // Use all vs primary/secondary
    float point_influence;           // Strength (0.0-1.0)
}
```

---

## 🧭 **Universal Axis System**

### **All Effects Support:**
- **🡲 X-Axis** - Left wall → Right wall
- **🡱 Y-Axis** - Front wall → Back wall
- **🡹 Z-Axis** - Floor → Ceiling
- **🌀 Radial** - Outward from center point
- **🎯 Custom** - User-defined direction vector
- **🔄 Reverse** - Flip effect direction

### **Effect Examples:**
- **Wave on X-Axis**: Sweeps left → right across room
- **Wave on Y-Axis**: Sweeps front → back
- **Wave on Z-Axis**: Rises floor → ceiling
- **Radial Wave**: Ripples outward from reference point
- **Reverse Wave**: All above, but backwards

---

## 🎮 **Effect Scenarios (Examples)**

### **🏠 Home Office Setup**
**Reference Points:**
- User: (0, 0, 0) - Chair center
- Monitor: (0, -2, 1) - In front of user
- Desk: (0, -1, -1) - Desktop surface
- Speakers: (-3, -2, 1) & (3, -2, 1) - Left/right

**Effect Ideas:**
1. **"Notification Pulse"** - Lightning between Monitor → User when email arrives
2. **"Focus Mode"** - Breathing sphere around User, dim everything else
3. **"Music Visualization"** - Ripples from both speakers toward center
4. **"Work Complete"** - Explosion from desk surface, celebration colors

### **🎮 Gaming Setup**
**Reference Points:**
- User: (0, 0, 0) - Gaming chair
- Monitor: (0, -3, 2) - Large gaming display
- PC: (2, -2, -1) - Tower location
- Streaming Camera: (0, 2, 3) - Behind user, elevated

**Effect Ideas:**
1. **"Health Critical"** - Red pulse from PC → User (low HP warning)
2. **"Victory Celebration"** - Multi-point explosion from all sources
3. **"Streaming Live"** - Orbit effect around User position
4. **"Game Ambience"** - Environmental effects based on game state

### **🛏️ Bedroom Setup**
**Reference Points:**
- User: (0, 0, 0) - Bed center
- TV: (-4, 0, 2) - Mounted on wall
- Door: (0, 5, 0) - Room entrance
- Window: (4, 0, 3) - Natural light

**Effect Ideas:**
1. **"Sunrise Sim"** - Warm light from Window → User
2. **"Movie Mode"** - Dim everything, spotlight TV area
3. **"Bedtime Routine"** - Slow fade from all points → center
4. **"Wake Up"** - Gentle pulse starting from User outward

---

## 🔧 **Implementation Benefits**

### **For Users:**
- **Intuitive Setup** - "My chair is here, monitor is there"
- **Realistic Effects** - Physics-based lighting behavior
- **Infinite Creativity** - Combine reference points for unique effects
- **Easy Visualization** - See all reference points in 3D viewport

### **For Effect Designers:**
- **Standardized API** - All effects use same reference system
- **Scalable Complexity** - Start simple, add multi-point later
- **Universal Controls** - X/Y/Z/Radial work for everything
- **Flexible Origins** - Any reference point can be effect source

### **For Advanced Users:**
- **Custom Automation** - Link effects to external triggers
- **Modular Design** - Mix and match reference points
- **Room Templates** - Save/load common room setups
- **Effect Chains** - Sequence multiple effects using different points

---

## 🚀 **Future Expansion Ideas**

### **Dynamic Reference Points:**
- **Motion Tracking** - User position updates in real-time
- **Device Integration** - Phone location as reference point
- **Smart Home Sync** - TV state, door sensors, etc.

### **Advanced Multi-Point Effects:**
- **Particle Systems** - LEDs behave like physics particles
- **Fluid Dynamics** - Light "flows" between reference points
- **Gravity Wells** - Reference points attract/repel effects
- **Temporal Effects** - Effects remember previous reference states

### **AI-Assisted Effects:**
- **Scene Recognition** - Auto-detect room layout
- **Mood Analysis** - Adjust effects based on activity
- **Adaptive Timing** - Effects learn user preferences
- **Smart Suggestions** - Recommend effects for detected scenarios

---

## 💡 **Implementation Priority**

### **Phase 1: Foundation**
1. ✅ Reference point data structures
2. ✅ Basic UI for managing reference points
3. ✅ Viewport visualization of points
4. ✅ Universal axis controls (X/Y/Z/Radial/Reverse)

### **Phase 2: Multi-Point**
1. 🔄 Dual-point effects (Lightning, Wipe Between)
2. 🔄 Multi-point configuration UI
3. 🔄 Effect blending system
4. 🔄 Reference point influence controls

### **Phase 3: Advanced**
1. ⏳ Complex multi-point effects (Network, Constellation)
2. ⏳ Effect chaining and sequences
3. ⏳ Save/load reference point templates
4. ⏳ External trigger integration

---

**This system transforms OpenRGB from simple lighting control into a true 3D environmental experience engine! 🌟**