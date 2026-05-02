# OpenRGB 3D Spatial LED Control

Plugin for [OpenRGB](https://openrgb.org/) built around one idea: **drive lighting from a 3D layout**—where devices and surfaces sit in space—not only from zone order on a single strip. You place hardware in a **3D room**, run **spatial effects** across that model, and can add **screen capture** (ambilight-style onto geometry), **display planes**, and **game-linked** lighting. Waves, mirroring, sampling, and similar ideas use position in the grid so the rig matches how you arranged it.

This project started as a **personal build** and is still **experimental**: some areas are rough, incomplete, or tuned for one workflow. It is shared in case others want to try it, break it, or extend it—not as a polished retail product.

## Who it is for

- You already use OpenRGB and are OK reading a short **README** and poking at options.
- You want **3D-aware** effects and layout (not only “strip modes” on a single device).
- You can tolerate **sharp edges**: missing docs in places, features still in motion, and setup that assumes you will experiment.

If you expect plug-and-play with zero reading, this will probably frustrate you—OpenRGB itself is already a lot for many users.

## What you can do (high level)

- Add detected devices to a **3D viewport**, move and rotate them, snap to grid.
- Use **reference points**, **display planes**, and **capture zones** (e.g. for **Screen Mirror**).
- Stack **spatial effects** (wave, plasma, textures, audio-reactive bands, game bridges, etc.).
- Save and load layout / effect **profiles** where supported.

## What to expect (honest status)

The whole plugin is **experimental**. Pieces land at different levels of polish:

- **Spatial layout and stacking** — This is the backbone: 3D grid, overlays, and driving LEDs from that model. It is the most “together” part of the project, and still evolving.
- **Effects** — A **mix**. Some effects feel great and behave well in 3D; others are **lackluster** or need more tuning, presets, or UX. Treat the library as a grab bag until you find favorites that match your setup.
- **Screen mirror / ambilight** — **Works**, but it is **experimental**, especially the idea of **mapping live capture into 3D space** (planes, zones, room grid). Expect to tune capture, geometry, and quality settings; behavior can depend on GPU, HDR, and Windows compositor quirks.
- **Gaming integration** — The **plumbing** for game-linked lighting is **new** and **rough**. **Minecraft** in particular **needs a lot of work** (telemetry, UX, and reliability); other paths may be in better shape. Assume game features are **best-effort** and report or patch what you need.

Until you have tried a feature on **your** PC, treat it as **unproven** for you—not “done” for everyone.

## Requirements

- OpenRGB **with plugin support** enabled
- Qt **5.15+** or **6.x**
- Toolchain: **MSVC** (typical Windows build) or **GCC/Clang** (Linux)
- **OpenRGB submodule** (headers/SDK for the plugin):  
  `git submodule update --init`

## Build

From the repo root:

- **Windows (x64):**  
  `qmake OpenRGB3DSpatialPlugin.pro CONFIG+=release` then `nmake` (or your kit’s equivalent).
- **Linux:**  
  `qmake OpenRGB3DSpatialPlugin.pro PREFIX=/usr` then `make -j$(nproc)`

Install or copy the built plugin to the location OpenRGB expects for plugins (see OpenRGB’s documentation for plugin paths on your OS).

## Quick start (minimum path)

1. Build the plugin and install it; restart OpenRGB if needed.
2. Open the **3D Spatial** tab.
3. Bring devices into the 3D scene and place them roughly where they sit in your room.
4. Pick an effect, add it to the stack (or use the flow your build exposes), and start it.
5. For **Screen Mirror**, set up a **display plane** and capture source in the UI, then enable preview if you want to see the capture on the plane.

If something does not light up, check OpenRGB’s device view (zones, permissions, and whether the plugin is loaded) before assuming the spatial stack is at fault.

## Contributing / development

See [CONTRIBUTING.md](CONTRIBUTING.md) for code style, scope (plugin-owned paths vs `OpenRGB/` subtree), RGB safety notes, and release/version hints.

## License

GPL-2.0-only — see [LICENSE](LICENSE).
