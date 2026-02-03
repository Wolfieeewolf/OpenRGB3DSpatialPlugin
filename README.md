# OpenRGB 3D Spatial LED Control

Plugin for [OpenRGB](https://openrgb.org/) that arranges your RGB devices in a 3D room and runs spatial effects across them. Place controllers in the grid, add reference points and display planes, then run effects (Wave, Plasma, Screen Mirror, and others) that respect 3D position.

## Requirements

- OpenRGB (with plugin support)
- Qt 5.15+ or Qt 6
- MSVC (Windows) or GCC/Clang (Linux)
- OpenRGB submodule: `git submodule update --init`

## Build

From the repo root:

- **Windows (x64):** `qmake OpenRGB3DSpatialPlugin.pro CONFIG+=release` then `nmake`
- **Linux:** `qmake OpenRGB3DSpatialPlugin.pro PREFIX=/usr` then `make -j$(nproc)`

Copy or install the resulting plugin so OpenRGB can load it (see OpenRGB docs for plugin location).

## Usage

In OpenRGB, open the **3D Spatial** tab. Add your detected devices to the 3D view, position them in the grid, then pick an effect and start it. Use **Object Creator** to add custom controller grids, reference points, or display planes (e.g. for Screen Mirror). Layout and effect profiles can be saved and loaded.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for style, practices, and release process.

## License

GPL-2.0-only — see [LICENSE](LICENSE).
