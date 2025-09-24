# OpenRGB 3D Spatial LED Control Plugin

A production-ready OpenRGB plugin that enables advanced 3D spatial lighting effects across your RGB devices. Position devices in 3D space and apply synchronized effects that respond to spatial coordinates.

## Features

### 🎨 Advanced Spatial Effects (25 Effects)

**Wave Effects:**
- Wave X/Y/Z - Directional waves along each axis
- Radial Wave - Expanding concentric waves
- Ripple - Multi-layered interference patterns

**Animated Effects:**
- Rain - Cascading droplets from top to bottom
- Fire - Dynamic flame simulation with flicker
- Plasma - Smooth multi-dimensional color transitions
- Meteor - Shooting star trails with fade
- DNA Helix - Double helix spiral animation

**Geometric Effects:**
- Spiral - Rotating spiral patterns in 3D space
- Orbit - Planetary orbit simulation
- Sphere Pulse - Expanding/contracting spheres
- Cube Rotate - Rotating cube outline
- Breathing Sphere - Pulsing sphere with smooth breathing

**Room Effects:**
- Room Sweep - Wall-to-wall scanning beam
- Corners - Sequential corner illumination
- Vertical Bars - Animated vertical light bars
- Explosion - Expanding shockwave from center

**Transition Effects:**
- Wipe Top-Bottom - Vertical color wipe
- Wipe Left-Right - Horizontal color wipe
- Wipe Front-Back - Depth-based color wipe

**LED-Specific Effects:**
- LED Sparkle - Random LED twinkles
- LED Chase - Sequential LED runner
- LED Twinkle - Smooth breathing twinkle

### 🎮 Interactive 3D Controls

- **Device Positioning**: Drag and drop devices in 3D space
- **Custom 3D Controllers**: Create virtual devices from multiple physical devices
- **Profile Management**: Save/load layouts with auto-load on startup
- **Real-time Preview**: See effects update live in 3D viewport
- **Mouse Controls**:
  - Middle Mouse - Rotate camera
  - Shift + Middle Mouse / Right Mouse - Pan camera
  - Scroll Wheel - Zoom in/out
  - Left Click - Select and move devices

### 💾 Layout Management

- **Save/Load Profiles**: Store multiple spatial layouts
- **Auto-load on Startup**: Automatically restore your preferred layout
- **Custom Controllers**: Group multiple devices into virtual 3D grids
- **Import/Export**: Share custom controller configurations

### ⚙️ Effect Controls

- **Speed**: 1-100 (adjustable animation speed)
- **Brightness**: 0-100% (master brightness control)
- **Color Gradients**: Start/end color selection for gradient effects
- **Real-time Adjustment**: Modify parameters while effects are running

## Installation

### Prerequisites

- **OpenRGB** 0.9 or later ([Download](https://openrgb.org))
- **Qt** 5.15 or Qt 6.8+ (required for building)

### Quick Install (Pre-built)

1. Download the latest release for your platform
2. Extract the plugin file:
   - **Windows**: `OpenRGB3DSpatialPlugin.dll`
   - **Linux**: `libOpenRGB3DSpatialPlugin.so`
   - **macOS**: `libOpenRGB3DSpatialPlugin.dylib`

3. Copy to OpenRGB plugins directory:
   - **Windows**: `%APPDATA%\OpenRGB\plugins\`
   - **Linux**: `~/.config/OpenRGB/plugins/` or `/usr/lib/openrgb/plugins/`
   - **macOS**: `~/Library/Application Support/OpenRGB/plugins/`

4. Restart OpenRGB
5. Navigate to the "3D Spatial" tab

### Building from Source

#### Windows

```bash
# Install Qt and MSVC 2019/2022
git clone https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin.git
cd OpenRGB3DSpatialPlugin
git submodule update --init --recursive

# Build with Qt 5.15
qmake OpenRGB3DSpatialPlugin.pro
nmake release

# Or use the build script
.\scripts\build-windows.bat 5.15.0 2019 64
```

#### Linux

```bash
# Install dependencies (Debian/Ubuntu)
sudo apt install git build-essential qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools libusb-1.0-0-dev libhidapi-dev

# Clone and build
git clone https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin.git
cd OpenRGB3DSpatialPlugin
git submodule update --init --recursive

qmake OpenRGB3DSpatialPlugin.pro PREFIX=/usr
make -j$(nproc)
sudo make install
```

#### macOS

```bash
# Install Homebrew dependencies
brew install git qt@5 hidapi libusb mbedtls@2
brew link qt@5

# Clone and build
git clone https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin.git
cd OpenRGB3DSpatialPlugin
git submodule update --init --recursive

qmake OpenRGB3DSpatialPlugin.pro
make -j8
```

## Usage Guide

### Basic Setup

1. **Launch OpenRGB** and navigate to the **3D Spatial** tab
2. **Add Devices**: Your connected RGB devices appear in the left panel
3. **Position Devices**:
   - Select a device from the list
   - Click "Add to 3D View"
   - Drag the device to position it in 3D space
4. **Adjust Layout**: Use mouse controls to rotate/pan/zoom the viewport

### Creating Custom Controllers

1. Click **"Create Custom Controller"**
2. Set grid dimensions (Width × Height × Depth)
3. Assign LEDs from physical devices to grid positions
4. Supports multiple granularities: Device, Zone, or LED level
5. **Save** to reuse the virtual controller

### Using Effects

1. Select an effect from the **Effect Type** dropdown
2. Adjust **Speed** (1-100) and **Brightness** (0-100%)
3. Choose **Start/End Colors** for gradient effects
4. Click **"Start Effect"** to begin
5. Modify parameters in real-time while running

### Saving Layouts

1. Position all devices in 3D space
2. Click **"Save Layout"**
3. Enter a profile name
4. Check **"Auto-load on startup"** to restore automatically
5. Layouts are saved to: `%APPDATA%/OpenRGB/plugins/3DSpatial/layouts/`

## Project Structure

```
OpenRGB3DSpatialPlugin/
├── OpenRGB/                      # OpenRGB SDK (git submodule)
├── ui/                           # User interface components
│   ├── OpenRGB3DSpatialTab.*     # Main plugin tab
│   ├── LEDViewport3D.*           # 3D OpenGL viewport
│   └── CustomControllerDialog.*  # Custom controller creator
├── ControllerLayout3D.*          # LED position generation
├── SpatialEffects.*              # Effect calculation engine
├── VirtualController3D.*         # Custom controller system
└── OpenRGB3DSpatialPlugin.*      # Plugin interface implementation
```

## Development

### Adding New Effects

1. Add effect enum to `SpatialEffectType` in `SpatialEffects.h`:
   ```cpp
   SPATIAL_EFFECT_YOUR_EFFECT = 25,
   ```

2. Implement calculation in `SpatialEffects.cpp`:
   ```cpp
   RGBColor SpatialEffects::CalculateYourEffect(Vector3D pos, float time_offset)
   {
       // Your effect logic here
       return color;
   }
   ```

3. Add case to `UpdateLEDColors()` switch statement
4. Add effect name to UI in `OpenRGB3DSpatialTab.cpp`

### Code Style

This plugin follows [OpenRGB Contributing Guidelines](https://gitlab.com/CalcProgrammer1/OpenRGB/-/blob/master/CONTRIBUTING.md):
- C-style code conventions (C arrays, indexed loops, minimal std library)
- 4 spaces indentation (no tabs)
- Opening/closing braces on own lines
- Use LogManager for logging (no printf/cout)
- Cross-platform compatibility (Windows/Linux/macOS)

## Troubleshooting

### Plugin not loading
- Ensure OpenRGB 0.9+ is installed
- Check plugin is in correct directory
- Verify file permissions (should be readable/executable)
- Check OpenRGB logs for plugin loading errors

### Effects not working
- Ensure devices support direct color control
- Some devices may need to be set to "Direct" mode first
- Check that devices are added to 3D view
- Verify effect is started (not just selected)

### Performance issues
- Reduce number of devices in 3D view
- Lower effect speed setting
- Close other intensive applications
- Update graphics drivers for OpenGL support

## Contributing

Contributions are welcome! Please:
1. Follow OpenRGB coding style guidelines
2. Test on multiple platforms if possible
3. Keep commits focused and well-described
4. Update documentation for new features

## License

GPL-2.0-only - Consistent with OpenRGB project licensing

## Links

- **OpenRGB**: https://openrgb.org
- **Plugin Repository**: https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin
- **Issue Tracker**: https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin/-/issues
- **OpenRGB GitLab**: https://gitlab.com/CalcProgrammer1/OpenRGB

## Credits

Created for the OpenRGB community
Built with the OpenRGB Plugin SDK

---

**Note**: This plugin requires direct device control. Some RGB devices may have limited effect support depending on their controller implementation.