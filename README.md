# OpenRGB 3D Spatial Plugin

A spatial lighting plugin for [OpenRGB](https://openrgb.org) that lets you position devices in 3D space and run room-aware effects.

## Highlights
- 3D viewport with move/rotate gizmo and grid snap
- Manual or auto-detected room dimensions in millimetres
- Spatial effects (Wave, Wipe, Spiral, Plasma, Helix, Explosion, Spin, etc.)
- Zones, reference points, and effect stacking for complex scenes
- In-process SDK surface (`OpenRGB3DSpatialGridAPI`) for other plugins/tools

## Requirements
- OpenRGB 0.9+
- Qt 5.15 or Qt 6.5/6.6/6.8 (matching your OpenRGB build)
- CMake/ninja or qmake + compiler toolchain (MSVC, GCC, Clang)

## Install
```bash
# inside the OpenRGB source tree
cd plugins
git clone https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin.git
cd OpenRGB3DSpatialPlugin
qmake OpenRGB3DSpatialPlugin.pro
make -j$(nproc)
```
The resulting plugin library is produced under `release/` (Windows) or your build output directory (Linux/macOS). Copy it to the OpenRGB plugins folder and enable it from the Plugins tab.

## Usage
1. Launch OpenRGB and open the **3D Spatial** tab.
2. Add controllers from **Available Controllers** and set their positions.
3. Tune room dimensions in **Grid Settings**.
4. Trigger an effect from the **Effects** tab or combine multiple via **Effect Stack**.

A concise walkthrough lives in `docs/guides/QuickStart.md`. The grid SDK interface is documented in `docs/guides/GridSDK.md`.

## Development
- Coding style mirrors upstream OpenRGB: explicit types, indexed loops, braces on new lines.
- `docs/guides/ContributorGuide.md` covers build steps, effect architecture, and PR expectations.
- External references:
  - `https://gitlab.com/CalcProgrammer1/OpenRGB/-/blob/master/RGBControllerAPI.md` (device abstraction)
  - `https://gitlab.com/CalcProgrammer1/OpenRGB/-/blob/master/OpenRGBSDK.md` (network SDK protocol)

## Documentation
- `docs/guides/QuickStart.md`  user walkthrough
- `docs/guides/GridSDK.md`  spatial SDK reference
- `docs/guides/ContributorGuide.md`  development workflow

## License
GPL-2.0-only  same as OpenRGB.

## Support
- Report issues: <https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/issues>
- Community chat: <https://discord.gg/AQwjJPY>

Happy mapping!


