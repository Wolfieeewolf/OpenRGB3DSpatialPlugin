# Changelog

All notable changes to the OpenRGB 3D Spatial Plugin will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2025-09-24

### Added - Initial Production Release

#### Core Features
- **3D Spatial LED Control System**: Position RGB devices in 3D space with interactive viewport
- **25 Advanced Spatial Effects**: Wave, animated, geometric, room, transition, and LED-specific effects
- **Custom 3D Controllers**: Create virtual devices by mapping LEDs from multiple physical devices
- **Profile Management**: Save/load spatial layouts with auto-load on startup
- **Real-time Effect Control**: Adjust speed, brightness, and colors while effects are running

#### Spatial Effects
**Wave Effects:**
- Wave X/Y/Z - Directional waves along each axis
- Radial Wave - Expanding concentric waves
- Ripple - Multi-layered interference patterns

**Animated Effects:**
- Rain - Cascading droplets
- Fire - Dynamic flame simulation
- Plasma - Smooth color transitions
- Meteor - Shooting star trails
- DNA Helix - Double helix spiral

**Geometric Effects:**
- Spiral - Rotating spiral patterns
- Orbit - Planetary orbit simulation
- Sphere Pulse - Expanding/contracting spheres
- Cube Rotate - Rotating cube outline
- Breathing Sphere - Pulsing sphere with breathing

**Room Effects:**
- Room Sweep - Wall-to-wall scanning beam
- Corners - Sequential corner illumination
- Vertical Bars - Animated vertical bars
- Explosion - Expanding shockwave

**Transition Effects:**
- Wipe Top-Bottom - Vertical color wipe
- Wipe Left-Right - Horizontal color wipe
- Wipe Front-Back - Depth-based color wipe

**LED-Specific Effects:**
- LED Sparkle - Random LED twinkles
- LED Chase - Sequential LED runner
- LED Twinkle - Smooth breathing twinkle

#### User Interface
- **3D OpenGL Viewport**: Interactive device positioning with mouse controls
  - Middle Mouse: Rotate camera
  - Shift + Middle Mouse / Right Mouse: Pan camera
  - Scroll Wheel: Zoom in/out
  - Left Click: Select and move devices
- **Device List Panel**: Shows all connected RGB devices
- **Effect Controls**: Speed, brightness, and color gradient settings
- **Layout Management**: Save/load profiles with dropdown selector
- **Custom Controller Creator**: Dialog for creating virtual 3D grids

#### Data Management
- **Layout Persistence**: JSON-based layout storage in `%APPDATA%/OpenRGB/plugins/3DSpatial/layouts/`
- **Custom Controller Storage**: JSON files for virtual controller configurations
- **Auto-load Support**: Automatically restore preferred layout on startup
- **Import/Export**: Share custom controller configurations

#### Platform Support
- **Windows**: Full support (Qt 5.15 / Qt 6.8+, MSVC 2019/2022)
- **Linux**: Full support (Qt 5.15+, GCC/Clang)
- **macOS**: Full support (Qt 5.15+, Clang)

### Technical Improvements
- **OpenRGB Style Compliance**: Follows all OpenRGB coding guidelines
  - C-style code conventions (indexed loops, C arrays)
  - No `auto` keywords
  - Proper brace formatting
  - Aligned variable declarations
- **API Compliance**:
  - Proper `SetCustomMode()` calls before LED updates
  - Prevents unintended flash memory writes
- **Clean Codebase**:
  - Zero TODOs/FIXMEs in plugin code
  - Zero debug comments
  - Proper LogManager usage (no printf/cout)
  - ~4,788 lines of production code
- **Cross-Platform Build System**:
  - QMake-based build with platform-specific configurations
  - C++17 standard for all platforms
  - Proper library linkage (OpenGL, sockets, etc.)

### Documentation
- Comprehensive README with installation and usage instructions
- Troubleshooting guide for common issues
- Developer guide for adding new effects
- Code examples for contributors
- Complete feature documentation

### Known Limitations
- Requires OpenRGB 0.9 or later
- Devices must support direct color control
- Some devices may require "Direct" mode to be set manually
- Performance depends on number of devices and LEDs

---

## Release Notes

### Version 1.0.0 - Production Release

This is the first production-ready release of the OpenRGB 3D Spatial Plugin. The plugin has been thoroughly tested and cleaned up to meet OpenRGB project standards.

**What's New:**
- Complete spatial effect system with 25 unique effects
- Custom controller creation for virtual 3D LED grids
- Profile management with auto-load capability
- Production-ready code following OpenRGB guidelines
- Comprehensive documentation and troubleshooting guides

**Installation:**
Download the appropriate plugin file for your platform and place it in your OpenRGB plugins directory. See README.md for detailed instructions.

**Upgrade Notes:**
This is the initial release - no upgrade path needed.

**Contributors:**
- Plugin development and OpenRGB integration
- Effect algorithm implementation
- UI/UX design and 3D viewport
- Documentation and testing

Built for the OpenRGB community with ❤️