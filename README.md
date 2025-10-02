# OpenRGB 3D Spatial LED Control Plugin

**Beta/Concept Plugin** - Experimental 3D spatial lighting effects for OpenRGB devices.

Position your RGB devices in 3D space and apply spatial effects that respond to LED coordinates and reference points.

## Features

- **3D Viewport**: Position devices in 3D space with interactive OpenGL viewport
- **Spatial Effects**: Physics-based effects that respond to LED position in 3D space
- **Reference Point System**: User head position support for perception-based effects
- **Custom Controllers**: Create virtual 3D grids from multiple devices
- **Layout Profiles**: Save/load positions with auto-load support
- **Universal Axis Controls**: X/Y/Z/Radial axes with forward/reverse for all effects
- **Effect Registration System**: Modular effect architecture for easy expansion

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
- Reference point system (user head position support)
- Universal effect base class with common controls
- Rainbow mode and multi-color support
- Frequency, speed, brightness controls
- Effect auto-registration system

**UI Features:**
- Interactive 3D viewport (rotate/pan/zoom)
- Device selection and positioning
- Color picker with add/remove color support
- Axis selection (X/Y/Z/Radial)
- Effect origin modes (Room Center / User Head)

### Currently Broken/Non-Functional 🔴

**Known Broken Features:**
- **Custom Controller Creation** - UI exists but functionality incomplete/buggy
- **Layout Auto-load** - Checkbox present but auto-load on startup not working reliably
- **Some device positioning** - Gizmo controls may not work correctly for all device types
- **Effect persistence** - Effect settings not always saved/restored with layouts correctly
- **Viewport rendering issues** - Occasional flickering or rendering artifacts on some systems

### Not Yet Implemented ❌

**Planned Features:**
- Multi-point reference system (monitor, desk, speakers, etc.)
- Dual-point effects (Lightning Arc, Orbit Between)
- Multi-point effects (Network Pulse, Zone Influence)
- Per-effect origin override
- Custom reference points
- Effect chaining/sequencing
- Reference point templates
- Game integration API
- Motion tracking support

**Known Limitations:**
- Reference points limited to user head only (no monitor/desk/speakers yet)
- No multi-point effect support
- No external trigger integration
- Performance not optimized for high LED counts (>500 LEDs)

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

### Reference Point System (User Head)

1. Set your head position using X/Y/Z spin boxes in "Reference Point" section
2. Check **"Use User Head as Effect Origin"** to enable perception-based effects
3. Effects will now originate from your head position instead of room center
4. Adjust position in real-time to see effects update

**Coordinate System:**
- X: Left (-) to Right (+)
- Y: Floor (0) to Ceiling (+)
- Z: Front (-) to Back (+)

### Save/Load Layouts

- **Save Layout**: Store device positions and reference points to JSON file
- **Load Layout**: Restore saved layout from file
- **Auto-load**: Enable checkbox to automatically load layout on startup
- Layouts include device positions, user head position, and effect settings

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
- **Custom controllers** - Creation UI exists but functionality is buggy/incomplete
- **Auto-load layouts** - Feature unreliable, may not load on startup
- **Reference points** - Multi-point system designed but only user head currently functional

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