# OpenRGB 3D Spatial LED Control Plugin

**Beta/Concept Plugin** - Experimental 3D spatial lighting effects for OpenRGB devices.

Position your RGB devices in 3D space and apply spatial effects that respond to LED coordinates and reference points.

## Features

- **3D Viewport**: Position devices in 3D space with interactive OpenGL viewport
- **Spatial Effects**: Physics-based effects that respond to LED position in 3D space
- **Reference Point System**: Multiple reference points for perception-based effects
- **Zone Management**: Group controllers into zones for targeted effect control
- **Custom Controllers**: Create virtual 3D grids from multiple devices
- **Layout Profiles**: Save/load positions with auto-load support
- **Universal Axis Controls**: X/Y/Z/Radial axes with forward/reverse for all effects
- **Effect Registration System**: Modular effect architecture for easy expansion
- **Unified Origin System**: All effects use configurable reference point origins

### Currently Working ✅

**Effects:**
- **Wave3D** - 3D wave propagation with shape controls (circles/squares)
- **Wipe3D** - Directional wipe transitions across space
- **Spiral3D** - Rotating spiral patterns in 3D
- **Plasma3D** - Organic plasma cloud animations
- **DNAHelix3D** - DNA double helix rotation effect
- **Explosion3D** - Radial explosion from origin point
- **BreathingSphere3D** - Pulsing sphere effect
- **Spin3D** - 3D rotation effects

**Core Systems:**
- 3D viewport with gizmo controls for device positioning
- Custom controller creation and LED mapping
- Layout save/load with JSON persistence
- **Multi-reference point system** (10+ reference point types)
- **Zone management** for grouping and targeting controllers
- Universal effect base class with common controls
- Rainbow mode and multi-color support
- Frequency, speed, brightness controls
- Effect auto-registration system
- **Unified origin system** - all effects support reference point origins

**UI Features:**
- Interactive 3D viewport (rotate/pan/zoom)
- Device selection and positioning
- Color picker with add/remove color support
- Axis selection (X/Y/Z/Radial)
- **Reference points**: User, Monitor, Chair, Desk, Speakers, Door, Window, Bed, TV, Custom
- **Zone management**: Create, edit, delete zones with controller selection
- **Effect origin selection**: Room Center or any reference point
- **Zone filtering**: Apply effects to specific zones only

### Currently Broken/Non-Functional 🔴

**Known Issues:**
- **Some device positioning** - Gizmo controls may not work correctly for all device types
- **Viewport rendering issues** - Occasional flickering or rendering artifacts on some systems
- **Performance** - Not optimized for high LED counts (>500 LEDs may lag)

### Not Yet Implemented ❌

**Planned Features:**
- Dual-point effects (Lightning Arc, Orbit Between)
- Multi-point effects (Network Pulse, Zone Influence)
- Effect chaining/sequencing
- Effect stacking (multiple simultaneous effects on different zones)
- Game integration API
- Motion tracking support
- Custom gizmo for reference points
- Reference point rotation controls

**Known Limitations:**
- No multi-point effect support yet
- No effect stacking/layering yet
- No external trigger integration
- Reference point gizmos not yet implemented

### Controls

- **Middle Mouse**: Rotate camera around view center
- **Shift+Middle / Right Mouse**: Pan camera
- **Scroll**: Zoom in/out
- **Left Click**: Select and move devices
- **Gizmo**: Drag X/Y/Z axes to position selected device

## Installation

### Requirements
- OpenRGB 0.9+
- Qt 5.15+ (for building)

### Quick Install

1. Download the plugin for your platform
2. Copy to OpenRGB plugins folder:
   - Windows: `%APPDATA%\OpenRGB\plugins\`
   - Linux: `~/.config/OpenRGB/plugins/`
   - macOS: `~/Library/Application Support/OpenRGB/plugins/`
3. Restart OpenRGB

### Build from Source

```bash
git clone https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin.git
cd OpenRGB3DSpatialPlugin
git submodule update --init --recursive

# Windows
qmake OpenRGB3DSpatialPlugin.pro
nmake release

# Linux
qmake OpenRGB3DSpatialPlugin.pro PREFIX=/usr
make -j$(nproc)
sudo make install

# macOS
qmake OpenRGB3DSpatialPlugin.pro
make -j8
```

## Usage

### Basic Setup

1. Open OpenRGB → **3D Spatial** tab
2. Select device from list → **Add to 3D View**
3. Use gizmo or drag to position device in 3D space
4. Select effect from dropdown
5. Adjust speed, brightness, colors, and axis
6. Click **Start Effect**

### Reference Point System

The plugin supports multiple reference points for positioning effects in your room:

**Available Reference Points:**
- User (head position)
- Monitor/Screen
- Chair
- Desk
- Speakers (Left/Right)
- Door
- Window
- Bed
- TV
- Custom points

**Using Reference Points:**
1. Go to **Reference Points** tab
2. Click **Add Reference Point** and choose type
3. Set position using X/Y/Z controls
4. Optionally set color for visual identification
5. In **Effects** tab, select reference point from **Origin** dropdown
6. Effects will now originate from selected reference point

**Coordinate System:**
- X: Left (-) to Right (+)
- Y: Front (-) to Back (+)
- Z: Floor (-) to Ceiling (+)

### Zone Management

Group controllers into zones for targeted effect control:

1. Go to **Zones** tab
2. Click **Create Zone** and enter a name
3. Select controllers to include in the zone
4. In **Effects** tab, select zone from **Zone** dropdown
5. Effects will only apply to controllers in the selected zone

**Use Cases:**
- "Desk" zone: Apply effects to keyboard, mouse, desk strip only
- "Wall" zone: Target wall-mounted devices
- "Ceiling" zone: Control overhead lighting separately

### Save/Load Layouts

- **Save Layout**: Store device positions, reference points, and zones to JSON file
- **Load Layout**: Restore saved layout from file
- **Auto-load**: Enable checkbox to automatically load layout on startup
- Layouts include device positions, reference points, zones, and effect settings

### Custom Controllers

- **Create Custom Controller**: Build virtual 3D grids from multiple physical devices
- Map individual LEDs from physical devices to grid positions
- Supports device/zone/LED level granularity
- Useful for creating unified effects across separate hardware

## Known Issues

- **Beta/experimental** - expect bugs and incomplete features
- **Performance** - Not optimized for high LED counts (>500 LEDs may lag)
- **Device compatibility** - Requires "Direct" mode, some devices may have limited support
- **Platform** - Primarily tested on Windows, Linux/macOS may have viewport rendering issues
- **Gizmo controls** - May not work correctly for all device types
- **Viewport rendering** - Occasional flickering or artifacts on some systems

## Contributing

Contributions welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

Follow OpenRGB coding style - C-style conventions, no `auto`, indexed loops.

## Support

If you find this plugin useful, consider supporting development:

☕ **[Buy me a pizza!](https://buymeacoffee.com/Wolfieee)** (I don't drink coffee)

## Links

- [OpenRGB](https://openrgb.org)
- [Issues](https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin/-/issues)
- [OpenRGB GitLab](https://gitlab.com/CalcProgrammer1/OpenRGB)

## Credits

Created by Wolfieee with assistance from Claude Code (Anthropic AI)

Built for the OpenRGB community

## License

GPL-2.0-only

---

**Note**: Experimental plugin - use at your own risk. Some devices may have limited support.