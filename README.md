# OpenRGB 3D Spatial LED Control Plugin

**Beta Plugin** - Advanced 3D spatial lighting effects for OpenRGB devices with game integration support.

Transform your RGB setup into a spatially-aware lighting system. Position devices in 3D space, define reference points throughout your room, and apply physics-based effects that respond to real-world coordinates.

## 🌟 Overview

This plugin extends OpenRGB with a complete 3D spatial lighting framework:
- **Interactive 3D viewport** with OpenGL visualization
- **8 spatial effects** with physics-based calculations
- **Reference point system** for room-aware effects
- **Zone management** for targeted control
- **Custom 3D grids** from multiple devices
- **Extensible architecture** for new effects
- **API ready** for game/external integration (planned)

## ✨ Features

### 3D Spatial Effects (8 Implemented)

| Effect | Description | Unique Parameters |
|--------|-------------|-------------------|
| **Wave3D** | Expanding wave propagation | Shape (Circles/Squares) |
| **Wipe3D** | Directional wipe transitions | Thickness, Edge Shape (Sharp/Smooth/Gradient) |
| **Spiral3D** | Rotating spiral patterns | Pattern (Spiral/Galaxy/Tornado), Arms (1-12), Gap Size |
| **Plasma3D** | Organic plasma clouds | Pattern (Clouds/Liquid/Fire/Lightning) |
| **DNAHelix3D** | DNA double helix rotation | Helix Radius |
| **Explosion3D** | Radial shockwave | Intensity, Multiple Waves |
| **BreathingSphere3D** | Pulsing sphere | Size |
| **Spin3D** | 3D rotation effects | Surface (Sphere/Cylinder/Cube/Plane), Arms |

**Common Effect Controls (All Effects):**
- **Speed** (1-100): Animation speed with quadratic curve
- **Brightness** (1-100): Effect intensity
- **Frequency** (1-100): Pattern density with quadratic curve
- **Size** (1-100): Pattern scale (0.1x to 2.0x)
- **Scale** (10-200%): Coverage area (0.1x to 2.0x)
- **FPS Limiter** (1-60): Performance control
- **Axis Selection**: X/Y/Z/Radial/Custom
- **Reverse Direction**: Flip effect direction
- **Rainbow Mode**: Automatic color cycling
- **Multi-Color**: Up to 8 custom colors with add/remove

### 3D Viewport & Visualization

**Interactive OpenGL Viewport:**
- Real-time 3D LED visualization
- Device bounding boxes with names
- LED point cloud rendering
- Reference point markers (smiley face for user, spheres for others)
- Grid overlay with configurable dimensions
- Floor plane visualization
- Smooth camera controls

**Camera Controls:**
- **Middle Mouse Drag**: Rotate camera around view center
- **Shift+Middle / Right Mouse**: Pan camera (strafe)
- **Mouse Scroll**: Zoom in/out
- **Left Click**: Select controllers or reference points

**Grid System:**
- Configurable dimensions (X×Y×Z)
- Grid snap toggle for precise positioning
- Scale in mm per grid unit (1-100mm)
- Visual grid overlay

### Gizmo Manipulation System

**3-Mode Transformation Gizmo:**

1. **Move Mode** (Default)
   - X-axis (Red arrow): Left/Right movement
   - Y-axis (Green arrow): Up/Down movement
   - Z-axis (Blue arrow): Forward/Back movement
   - Center cube: Cycle gizmo modes
   - Grid snapping support

2. **Rotate Mode**
   - X-axis (Red ring): Pitch rotation
   - Y-axis (Green ring): Yaw rotation
   - Z-axis (Blue ring): Roll rotation
   - Grab handles on rings for precise control

3. **Freeroam Mode**
   - Purple stick with cube handle
   - Camera-oriented movement
   - Free positioning in screen space

**Gizmo Features:**
- Ray casting for precise axis picking
- Highlighted axes on hover (white)
- Grid snap toggle (1mm to 100mm)
- Works with both controllers and reference points
- Click center sphere/cube to cycle modes

### Reference Point System

**10+ Reference Point Types:**

| Type | Use Case |
|------|----------|
| **User** | Head position for user-centric effects |
| **Monitor/Screen** | Display-oriented effects |
| **Chair** | Seating position effects |
| **Desk** | Workspace lighting |
| **Left/Right Speaker** | Audio-reactive positioning |
| **Door** | Entrance/exit effects |
| **Window** | Natural light integration |
| **Bed** | Bedroom scene effects |
| **TV** | Entertainment area |
| **Custom** | Any other point of interest |

**Reference Point Features:**
- Unique ID and custom name
- 3D position (X, Y, Z in mm)
- Rotation (X, Y, Z in degrees) - *structure defined, UI planned*
- Scale (X, Y, Z multipliers)
- Display color for visual identification
- Visibility toggle
- Gizmo manipulation support ✅
- JSON save/load persistence

**Coordinate System:**
- **X-axis**: Left (-) to Right (+)
- **Y-axis**: Front (-) to Back (+)
- **Z-axis**: Floor (-) to Ceiling (+)
- **Origin**: Room center at (0, 0, 0)

**Effect Origin Modes:**
- **Room Center**: Default (0, 0, 0)
- **User Position**: Effects originate from user head
- **Any Reference Point**: Choose from all defined points
- **Custom Point**: Per-effect override (planned)

### Zone Management System

**Organize controllers into logical groups:**

**Zone Features:**
- Create named zones (e.g., "Desk", "Wall", "Ceiling")
- Add/remove controllers from zones
- Effect targeting by zone
- Zone-specific effect parameters
- JSON persistence

**Use Cases:**
- **Desk Zone**: Keyboard + Mouse + Desk strip
- **Wall Zone**: Wall-mounted LED strips
- **Ceiling Zone**: Overhead lighting
- **Ambient Zone**: Background accent lights
- **Gaming Zone**: Peripherals only

**Zone-Based Effect Control:**
- Select "All Controllers" for room-wide effects
- Select specific zone to target only that area
- Combine with reference points for precision effects
- Performance optimization (skip non-zone LEDs)

### Controller Layout System

**Physical Device Positioning:**

**LED Spacing Presets:**
- **Dense Strip** (10mm): Densely packed LED strips
- **Keyboard** (19mm): Standard keyboard layout
- **Sparse Strip** (33mm): Sparse LED strips
- **LED Cube** (50mm): 3D LED cube projects
- **Custom**: User-defined X/Y/Z spacing

**Granularity Levels:**
- **Whole Device**: Treat entire device as single unit
- **Zone Level**: Position each zone separately
- **LED Level**: Individual LED positioning

**Automatic Layout Generation:**
- Detects device type (keyboard, strip, etc.)
- Generates appropriate grid layout
- Applies physical spacing in mm
- Centers LEDs at local origin
- Applies controller transform

**Supported Device Types:**
- LED strips (linear layouts)
- Keyboards (matrix layouts)
- Mice (single zone)
- Custom 3D grids (virtual controllers)

### Custom Virtual Controllers

**Create unified 3D grids from multiple devices:**

**Features:**
- Define grid dimensions (W×H×D)
- Map individual LEDs to grid positions
- Layer-based editing (Z-axis tabs)
- Color preview visualization
- Import/Export to JSON
- Supports all granularity levels

**Use Cases:**
- Combine multiple strips into single wall grid
- Create 3D LED cube from discrete controllers
- Unify ceiling panels into single logical device
- Build complex multi-device patterns

**LED Mapping Dialog:**
- Layer selection (Z-axis)
- Click grid cells to assign LEDs
- Color-coded cell visualization
- Real-time controller color feedback
- Clear/reassign cells

### Layout Profiles

**Save/Load Complete Room Configurations:**

**Saved Data:**
- All controller positions/rotations
- LED spacing and granularity settings
- All reference points with properties
- All zones and their membership
- Grid configuration
- Effect settings (planned)

**Profile Management:**
- Save layout to named JSON file
- Load layout from file
- Delete unused layouts
- Auto-load on startup (2-second delay)
- Profile dropdown selection

**File Locations:**
- **Windows**: `%APPDATA%\OpenRGB\plugins\3d_spatial_layouts\`
- **Linux**: `~/.config/OpenRGB/plugins/3d_spatial_layouts\`
- **macOS**: `~/Library/Application Support/OpenRGB/plugins/3d_spatial_layouts\`

### Effect System Architecture

**Modular & Extensible Design:**

**Auto-Registration System:**
```cpp
// Effects automatically register on load
REGISTER_EFFECT_3D(Wave3D);
EFFECT_REGISTERER_3D("Wave3D", "3D Wave", "3D Spatial",
                     [](){return new Wave3D;})
```

**Standardized Parameter System:**
- Normalized values (0.0-1.0) with consistent curves
- Effect-specific scaling multipliers
- Universal progress calculator
- Common UI controls with visibility flags

**Base Class Benefits:**
- Automatic common controls (speed, brightness, etc.)
- Color management (rainbow, multi-color)
- Reference point integration
- Parameter normalization
- Axis selection
- Minimal boilerplate for new effects

**Effect Developer API:**
- Implement 4 virtual methods
- Define effect info metadata
- Create custom UI controls
- Write color calculation function
- Automatic registration and loading

## 📋 System Requirements

### Minimum Requirements
- **OpenRGB**: 0.9 or later
- **Qt**: 5.15+ (for building from source)
- **OpenGL**: 2.0+ compatible graphics
- **RAM**: 512MB available
- **CPU**: Dual-core processor

### Recommended Specifications
- **OpenRGB**: Latest version
- **Qt**: 5.15 or Qt 6.x
- **OpenGL**: 3.3+ for best performance
- **RAM**: 2GB+ for high LED counts
- **CPU**: Quad-core for smooth effects

### Performance Notes
- **<100 LEDs**: Excellent performance, 60 FPS
- **100-300 LEDs**: Good performance, 30-60 FPS
- **300-500 LEDs**: Acceptable, 30 FPS recommended
- **500+ LEDs**: May experience lag, reduce FPS or use zone filtering

## 🚀 Installation

### Pre-built Binaries

1. Download the latest release for your platform from [Releases](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/releases)
2. Copy plugin file to OpenRGB plugins folder:
   - **Windows**: `%APPDATA%\OpenRGB\plugins\OpenRGB3DSpatialPlugin.dll`
   - **Linux**: `~/.config/OpenRGB/plugins/libOpenRGB3DSpatialPlugin.so`
   - **macOS**: `~/Library/Application Support/OpenRGB/plugins/libOpenRGB3DSpatialPlugin.dylib`
3. Restart OpenRGB
4. Look for **3D Spatial** tab at the top

### Build from Source

**Prerequisites:**
- Qt 5.15+ development libraries
- C++17 compatible compiler
- OpenRGB source code (for headers)
- Git

**Clone Repository:**
```bash
git clone https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin.git
cd OpenRGB3DSpatialPlugin
git submodule update --init --recursive
```

**Windows (MSVC):**
```bash
qmake OpenRGB3DSpatialPlugin.pro
nmake release
# Output: release/OpenRGB3DSpatialPlugin.dll
```

**Windows (MinGW):**
```bash
qmake OpenRGB3DSpatialPlugin.pro
mingw32-make release
# Output: release/OpenRGB3DSpatialPlugin.dll
```

**Linux:**
```bash
qmake OpenRGB3DSpatialPlugin.pro PREFIX=/usr
make -j$(nproc)
sudo make install
# Output: /usr/lib/OpenRGB/plugins/libOpenRGB3DSpatialPlugin.so
```

**macOS:**
```bash
qmake OpenRGB3DSpatialPlugin.pro
make -j8
# Output: libOpenRGB3DSpatialPlugin.dylib
```

## 📖 Usage Guide

### Quick Start

1. **Launch OpenRGB** and navigate to the **3D Spatial** tab
2. **Add Controllers**:
   - Go to **Available Controllers** sub-tab
   - Select a device from the list
   - Choose LED spacing preset or set custom spacing
   - Click **Add to 3D View**
   - Device appears in viewport at origin
3. **Position Device**:
   - Click device in viewport to select
   - Drag with gizmo arrows to position
   - Use rotation/position spinboxes for precise control
   - Enable grid snap for alignment
4. **Run an Effect**:
   - Go to **Effects** sub-tab
   - Select effect from dropdown (e.g., "3D Wave")
   - Adjust speed, brightness, colors
   - Click **Start Effect**
5. **Save Layout**:
   - Click **Save Layout**
   - Enter a profile name
   - Enable **Auto-load** checkbox for automatic loading

### Advanced Workflows

#### Setting Up Reference Points

1. Go to **Reference Points** sub-tab
2. Click **Add Reference Point**
3. Select point type (e.g., "User")
4. Set X/Y/Z position:
   - Use spinboxes for precise values
   - Or click reference point in viewport and use gizmo
5. Set display color for visual identification
6. In **Effects** tab, change **Origin** to your reference point
7. Effects now originate from that position

**Example - User-Centric Explosion:**
- Set User reference point at your head position
- Select "Explosion3D" effect
- Set Origin to "User"
- Effect explodes outward from your position

#### Creating Zones

1. Go to **Zones** sub-tab
2. Click **Create Zone**
3. Enter zone name (e.g., "Desk Setup")
4. In the controller selection dialog:
   - Check controllers to include
   - Click OK
5. In **Effects** tab, select zone from **Zone** dropdown
6. Effects apply only to selected zone

**Example - Desk-Only Wave:**
- Create "Desk" zone with keyboard + mouse + strip
- Select "Wave3D" effect
- Set Zone to "Desk"
- Only desk devices show wave effect

#### Building Custom Controllers

1. Go to **Custom Controllers** sub-tab
2. Click **Create Custom Controller**
3. Set grid dimensions (e.g., 10×5×1 for wall grid)
4. Click through grid cells to assign LEDs:
   - Select source controller
   - Choose granularity (LED level recommended)
   - Select zone/LED index
   - Cell fills with color
5. Save custom controller
6. Add to 3D view like any other device

**Example - Unified Wall Grid:**
- 3 separate LED strips on wall
- Create 30×1×1 custom controller
- Map strip 1 to cells 0-9
- Map strip 2 to cells 10-19
- Map strip 3 to cells 20-29
- Now all strips act as single device

### Effect Parameter Guide

**Speed Parameter:**
- Low (1-20): Slow, meditative effects
- Medium (20-60): Standard animation speed
- High (60-100): Fast, energetic effects
- Uses quadratic curve (exponential feel)

**Frequency Parameter:**
- Low (1-20): Large, sparse patterns
- Medium (20-60): Balanced density
- High (60-100): Dense, detailed patterns
- Uses quadratic curve

**Size Parameter:**
- Low (1-50): Smaller pattern elements (0.1x-1.0x)
- High (51-100): Larger pattern elements (1.0x-2.0x)
- Linear scaling

**Scale Parameter:**
- Low (10-50%): Concentrated effect area
- High (51-200%): Expanded coverage
- Affects boundary radius

**Axis Selection:**
- **X-Axis**: Left-to-right movement
- **Y-Axis**: Front-to-back movement
- **Z-Axis**: Floor-to-ceiling movement
- **Radial**: Spherical expansion from origin
- **Reverse**: Flip direction (e.g., right-to-left)

### Keyboard Shortcuts

- **Middle Mouse + Drag**: Rotate camera
- **Shift + Middle/Right Mouse + Drag**: Pan camera
- **Mouse Wheel**: Zoom in/out
- **Left Click**: Select object
- **G**: Cycle gizmo mode (planned)
- **Del**: Delete selected controller (planned)

## 🔧 Troubleshooting

### Common Issues

**Plugin doesn't appear in OpenRGB:**
- Verify plugin file is in correct plugins folder
- Check file permissions (should be readable)
- Restart OpenRGB completely
- Check OpenRGB logs for plugin load errors

**Viewport is black/not rendering:**
- Update graphics drivers
- Check OpenGL support (run `glxinfo` on Linux)
- Try switching to software rendering (Qt environment variable)
- Reduce LED count or disable complex effects

**Effects are laggy/choppy:**
- Reduce LED count
- Lower FPS setting (try 15-20 FPS)
- Use zone filtering (apply effects to specific zones only)
- Close other GPU-intensive applications
- Reduce grid dimensions in viewport

**Gizmo not appearing:**
- Ensure controller/reference point is selected
- Check if gizmo is outside viewport (zoom out)
- Try switching gizmo modes
- Restart effect if running

**Device colors not updating:**
- Ensure device is in "Direct" mode
- Check if device is supported by OpenRGB
- Try stopping and restarting effect
- Check OpenRGB device list for errors

**Layout won't load:**
- Check JSON file exists in layouts folder
- Verify JSON syntax (use validator)
- Ensure all referenced controllers are connected
- Check OpenRGB logs for errors

### Performance Optimization

**For high LED counts (500+):**
1. Reduce FPS to 15-20
2. Enable zone filtering
3. Use simpler effects (Wave, Wipe vs Plasma)
4. Disable viewport updates (stop rendering)
5. Reduce grid resolution

**For slow UI response:**
1. Reduce custom controller grid size
2. Limit reference points to essential ones
3. Reduce number of zones
4. Close custom controller dialog when not in use

## 🛣️ Roadmap

### Planned Features

**High Priority:**
- 🔲 External API for game/trigger integration
- 🔲 Effect stacking/layering (multiple simultaneous effects)
- 🔲 Multi-zone effects (different effects per zone)
- 🔲 Effect developer contribution guide
- 🔲 Memory safety improvements (smart pointers)
- 🔲 Performance optimization (spatial partitioning, LUTs)
- 🔲 Thread safety for background effect calculations
- 🔲 Input validation and error handling

**Medium Priority:**
- 🔲 Dual-point effects (Lightning Arc between two points)
- 🔲 Multi-point effects (Network Pulse, Zone Influence)
- 🔲 Reference point rotation UI and effects
- 🔲 Effect chaining/sequencing system
- 🔲 Preset library (shareable effect configs)
- 🔲 Audio reactivity (frequency bands to zones)
- 🔲 Motion tracking integration (webcam, VR)

**Low Priority:**
- 🔲 Web UI for remote control
- 🔲 MQTT integration for home automation
- 🔲 Screen capture ambient lighting
- 🔲 Physics simulation (gravity, bouncing)
- 🔲 Particle system effects
- 🔲 Video playback on LED grid

### Future Effect Ideas

- **Lightning Arc** (dual-point)
- **Orbit Between** (dual-point rotation)
- **Network Pulse** (multi-point)
- **Zone Influence** (multi-zone interaction)
- **Fireflies** (particle swarm)
- **Rain** (physics-based)
- **Meteor Shower** (directional particles)
- **Black Hole** (gravity well)
- **Portal** (dual-point teleportation)

## 🤝 Contributing

Contributions are welcome! Whether you want to add effects, fix bugs, improve performance, or enhance the UI.

### Ways to Contribute

1. **Report Bugs**: Open issues with detailed reproduction steps
2. **Request Features**: Describe your use case and desired functionality
3. **Submit Code**: Fork, code, test, open pull request
4. **Write Effects**: Create new spatial effects (guide coming soon)
5. **Improve Docs**: Fix typos, clarify instructions, add examples
6. **Test**: Try on different platforms, report compatibility issues

### Development Guidelines

**Code Style:**
- Follow OpenRGB coding standards (see [CONTRIBUTING.md](docs/CONTRIBUTING.md))
- Use C-style conventions (no `auto`, indexed loops)
- Use snake_case for variables, CamelCase for functions/classes
- 4 spaces indentation (spaces, not tabs)
- Opening braces on own lines
- No `printf`/`cout` - use LogManager
- No `QDebug` - use LogManager

**Effect Development:**
1. Inherit from `SpatialEffect3D` base class
2. Implement 4 required methods:
   - `GetEffectInfo()` - Effect metadata
   - `SetupCustomUI()` - Custom controls
   - `UpdateParams()` - Parameter updates
   - `CalculateColor(x, y, z, time)` - Color calculation
3. Register with `REGISTER_EFFECT_3D(YourEffect)`
4. Use standardized parameter helpers
5. Test with multiple devices and LED counts

**Pull Request Process:**
1. Fork the repository
2. Create feature branch (`git checkout -b feature/amazing-effect`)
3. Commit changes with clear messages
4. Test thoroughly on your platform
5. Push to your fork
6. Open pull request with detailed description

**What We're Looking For:**
- ✅ New spatial effects
- ✅ Performance improvements
- ✅ Bug fixes
- ✅ UI enhancements
- ✅ Documentation improvements
- ✅ Cross-platform compatibility fixes

## 📜 License

**GPL-2.0-only**

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; version 2 only.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

## 🙏 Credits

**Created by:** Wolfieee (WolfieeewWolf)

**Development Assistance:** Claude Code (Anthropic AI)

**Built for:** The OpenRGB community

**Special Thanks:**
- CalcProgrammer1 for OpenRGB
- OpenRGB contributors and community
- Beta testers and early adopters

## 💬 Support & Community

**Get Help:**
- 🐛 [Report Issues](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/issues)
- 💬 [OpenRGB Discord](https://discord.gg/AQwjJPY)
- 📧 [Email Support](mailto:wolfieee@msn.com)

**Support Development:**
- ☕ [Buy me a pizza!](https://buymeacoffee.com/Wolfieee) *(I don't drink coffee)*

**Links:**
- [OpenRGB Official](https://openrgb.org)
- [OpenRGB GitLab](https://gitlab.com/CalcProgrammer1/OpenRGB)
- [Plugin Repository](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin)

## ⚠️ Disclaimer

This is **beta software**. While functional, it may contain bugs and incomplete features. Use at your own risk.

**Known Limitations:**
- Performance not optimized for 500+ LEDs
- Some devices may have limited compatibility
- Requires "Direct" mode (may conflict with device-native effects)
- Primarily tested on Windows (Linux/macOS may have issues)
- Experimental reference point system
- API not yet public/stable

**Safety Notes:**
- Save your layouts frequently
- Test effects on small LED counts first
- Monitor device temperatures during intensive effects
- Some effects may trigger photosensitivity (high speed + brightness)

---

**Made with ❤️ for the RGB community**

*Transform your RGB setup from reactive to spatial - because your lighting should know where it is in the room.*
