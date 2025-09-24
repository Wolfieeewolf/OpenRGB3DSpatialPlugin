# OpenRGB 3D Spatial LED Control Plugin

**Beta/Concept Plugin** - Experimental 3D spatial lighting effects for OpenRGB devices.

Position your RGB devices in 3D space and apply spatial effects that respond to LED coordinates.

## Features

- **3D Viewport**: Position devices in 3D space with mouse controls
- **25 Spatial Effects**: Wave, animated, geometric, and room-scanning effects
- **Custom Controllers**: Create virtual 3D grids from multiple devices
- **Layout Profiles**: Save/load positions with auto-load support

### Available Effects

Wave: X/Y/Z, Radial, Ripple
Animated: Rain, Fire, Plasma, Meteor, DNA Helix
Geometric: Spiral, Orbit, Sphere Pulse, Cube Rotate, Breathing Sphere
Room: Sweep, Corners, Vertical Bars, Explosion
Wipes: Top-Bottom, Left-Right, Front-Back
LED: Sparkle, Chase, Twinkle

### Controls

- **Middle Mouse**: Rotate camera
- **Shift+Middle / Right Mouse**: Pan camera
- **Scroll**: Zoom
- **Left Click**: Select/move devices

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

1. Open OpenRGB → **3D Spatial** tab
2. Select device → **Add to 3D View**
3. Drag to position in 3D space
4. Select effect, adjust speed/brightness/colors
5. **Start Effect**

### Save/Load Layouts

- **Save Layout**: Store device positions
- **Load Layout**: Restore saved layout
- **Auto-load**: Check box to load on startup

### Custom Controllers

- **Create Custom Controller**: Make virtual 3D grids
- Map LEDs from physical devices to grid positions
- Supports device/zone/LED level granularity

## Known Issues

- Beta/experimental - expect bugs
- Performance varies with device count
- Some effects may not work on all devices
- Requires direct device control (set to "Direct" mode)

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

Created by Wolfie with assistance from Claude Code (Anthropic AI)

Built for the OpenRGB community

## License

GPL-2.0-only

---

**Note**: Experimental plugin - use at your own risk. Some devices may have limited support.