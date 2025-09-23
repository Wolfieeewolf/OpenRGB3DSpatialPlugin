# OpenRGB 3D Spatial LED Control Plugin

A plugin for OpenRGB that allows you to organize and control your RGB devices in a three-dimensional grid layout with advanced spatial lighting effects.

## Features

### 3D Grid Layout System
- **Customizable Grid Dimensions**: Configure width, height, and depth up to 50 units each
- **Device Positioning**: Place RGB devices at specific coordinates in 3D space
- **Interactive 3D Visualization**: Rotate and zoom the grid view with mouse controls
- **Real-time Device Updates**: See device positions and colors update live

### Spatial Lighting Effects

The plugin includes 9 advanced spatial effects that calculate colors based on device position in 3D space:

1. **Wave X/Y/Z**: Directional wave patterns along each axis
2. **Radial Wave**: Expanding waves from a central origin point
3. **Rain**: Simulated rainfall effect cascading down the Y-axis
4. **Fire**: Dynamic fire simulation with flickering
5. **Plasma**: Smooth plasma-like color transitions
6. **Ripple**: Multi-layered ripple effects from origin
7. **Spiral**: Rotating spiral patterns in 3D space

### Effect Controls
- **Speed Control**: Adjust animation speed (1-100)
- **Brightness Control**: Set effect brightness (0-100%)
- **Color Gradients**: Define start and end colors for gradient effects
- **Real-time Adjustment**: Modify effect parameters while running

## Installation

### Prerequisites
- OpenRGB 0.9 or later
- Qt 5.15 or later

### Building from Source

1. Clone the OpenRGB repository as a submodule:
```bash
git submodule add https://gitlab.com/CalcProgrammer1/OpenRGB OpenRGB
git submodule update --init --recursive
```

2. Build the plugin:

**Windows:**
```bash
qmake OpenRGB3DSpatialPlugin.pro
make
```

**Linux:**
```bash
qmake OpenRGB3DSpatialPlugin.pro PREFIX=/usr
make
sudo make install
```

3. Copy the compiled plugin to your OpenRGB plugins directory:
   - Windows: `%APPDATA%/OpenRGB/plugins/`
   - Linux: `/usr/lib/openrgb/plugins/`
   - macOS: `~/Library/Application Support/OpenRGB/plugins/`

## Usage

1. Launch OpenRGB
2. Navigate to the "3D Spatial" tab
3. Configure your 3D grid dimensions
4. Devices will be automatically placed in the grid
5. Drag to rotate the 3D view, scroll to zoom
6. Select an effect from the dropdown
7. Adjust speed, brightness, and colors
8. Click "Start Effect" to begin

## Network API

The plugin supports network control via OpenRGB SDK:

- `NET_PACKET_ID_3D_SPATIAL_GET_GRID`: Get grid dimensions
- `NET_PACKET_ID_3D_SPATIAL_SET_DEVICE_POS`: Set device position
- `NET_PACKET_ID_3D_SPATIAL_GET_EFFECTS`: List available effects
- `NET_PACKET_ID_3D_SPATIAL_START_EFFECT`: Start effect with parameters
- `NET_PACKET_ID_3D_SPATIAL_STOP_EFFECT`: Stop current effect

## Development

### Project Structure
```
OpenRGB3DSpatialPlugin/
├── OpenRGB/                    # OpenRGB SDK (submodule)
├── ui/                         # Qt UI components
│   ├── OpenRGB3DSpatialTab.*   # Main plugin tab
│   └── Grid3DWidget.*          # 3D visualization widget
├── SpatialGrid3D.*             # 3D grid layout system
├── SpatialEffects.*            # Spatial effects engine
└── OpenRGB3DSpatialPlugin.*    # Plugin interface
```

### Adding New Effects

To add a new spatial effect:

1. Add effect type to `SpatialEffectType` enum in `SpatialEffects.h`
2. Implement calculation function in `SpatialEffects.cpp`
3. Add case to `UpdateDeviceColors()` switch statement
4. Add effect name to UI combo box in `OpenRGB3DSpatialTab.cpp`

## Contributing

Contributions are welcome! Please follow the OpenRGB contributing guidelines:
- Use C-style code conventions
- Follow the existing code style
- Test thoroughly before submitting
- Keep commits focused and descriptive

## License

This plugin is licensed under GPL-2.0-only, consistent with the OpenRGB project.

## Credits

Created for the OpenRGB community
Based on the OpenRGB Plugin SDK