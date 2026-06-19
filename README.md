# OpenRGB 3D Spatial LED Control

[![Pipeline](https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin/badges/main/pipeline.svg)](https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin/-/pipelines)
[![Latest release](https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin/-/badges/release.svg)](https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin/-/releases)

**Primary repository:** [GitLab — OpenRGBDevelopers/OpenRGB3DSpatialPlugin](https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin)  
**GitHub mirror:** [Wolfieeewolf/OpenRGB3DSpatialPlugin](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin) (read-only; issues and merge requests on GitLab)

Plugin for [OpenRGB](https://openrgb.org/) built around one idea: **drive lighting from a 3D layout**—where devices and surfaces sit in space—not only from zone order on a single strip. You place hardware in a **3D room**, run **spatial effects** across that model, and can add **screen capture** (ambilight-style onto geometry), **display planes**, and **game-linked** lighting. Waves, mirroring, sampling, and similar ideas use position in the grid so the rig matches how you arranged it.

This project started as a **personal build** and is still **experimental**: some areas are rough, incomplete, or tuned for one workflow. It is shared in case others want to try it, break it, or extend it—not as a polished retail product.

## Who it is for

- You already use OpenRGB and are OK reading a short **README** and poking at options.
- You want **3D-aware** effects and layout (not only “strip modes” on a single device).
- You can tolerate **sharp edges**: features still in motion, and setup that assumes you will experiment.

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
- Qt **5.15** for today’s OpenRGB releases, or **6.0+** when using a Qt 6 OpenRGB build (match the host’s Qt major and toolchain)
- Toolchain: **MSVC** (typical Windows build) or **GCC/Clang** (Linux)
- **OpenRGB submodule** (headers/SDK for the plugin):  
  `git submodule update --init`

## Build

Prefer an **out-of-tree** build (keeps generated files out of the source tree):

```bash
mkdir build && cd build
qmake ../OpenRGB3DSpatialPlugin.pro CONFIG+=release
# Windows MSVC: nmake   |   Linux: make -j$(nproc)
```

In-tree from the repo root also works (`qmake OpenRGB3DSpatialPlugin.pro CONFIG+=release` then `nmake` / `make`). Optional `PROJECT_VERSION` in the repo root sets the plugin version string for builds (calendar `YY.MM.DD.V`, e.g. `26.05.17.2`).

Install or copy the built plugin to the location OpenRGB expects for plugins (see OpenRGB’s documentation for plugin paths on your OS).

**Pre-built binaries:** [GitLab CI artifacts](https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin/-/jobs/artifacts/main/download?job=Linux%20amd64) (nightly-style builds from `main`) or [Releases](https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin/-/releases) for tagged versions. Pick the package that matches your OpenRGB Qt major (Qt5 vs Qt6).

## Quick start (minimum path)

1. Build the plugin and install it; restart OpenRGB if needed.
2. Open the **3D Spatial** tab.
3. Bring devices into the 3D scene and place them roughly where they sit in your room.
4. Pick an effect, add it to the stack (or use the flow your build exposes), and start it.
5. For **Screen Mirror**, set up a **display plane** and capture source in the UI, then enable preview if you want to see the capture on the plane.

If something does not light up, check OpenRGB’s device view (zones, permissions, and whether the plugin is loaded) before assuming the spatial stack is at fault.

## Contributing / development

**Plugin bugs and ideas** — open on **GitLab** (primary): [issues](https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin/-/issues) · [merge requests](https://gitlab.com/OpenRGBDevelopers/OpenRGB3DSpatialPlugin/-/merge_requests)

The [GitHub repo](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin) is a mirror only; new issues and PRs there are redirected to GitLab.

**Controller preset JSON** (other repo): [OpenRGB3DSpatialPresets issues](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets/issues/new?template=new-controller-preset.yml)

See [CONTRIBUTING.md](CONTRIBUTING.md) for code style and which repo to use.

**Not required to build the plugin DLL:** `MinecraftSenderMod/` (optional Fabric mod for local game-telemetry tests), `.github/` / `.gitlab-ci.yml` (CI only), `OpenRGB/` submodule (headers/SDK only—do not treat as plugin source to edit casually). Developer reference lives in [`docs/`](docs/README.md) (viewport, effects, UI, **[custom controller JSON](docs/controller-preset-format.md)**). Use `docs/_local/` for private scratch notes (gitignored).

## Config on disk

OpenRGB config root (e.g. `%AppData%\Roaming\OpenRGB` on Windows).

- **Session UI** — `OpenRGB.json` → `3DSpatialPlugin` (camera, grid, room, profile names, auto-load flags, audio tuning).
- **Plugin data** — `plugins/settings/OpenRGB3DSpatialPlugin/`:
  - `controllers/` — LED layout JSON (import from [OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets); format: [docs/controller-preset-format.md](docs/controller-preset-format.md))
  - `layouts/` — scene layout profiles
  - `spatial-shaders/` — user GLSL for Shader Field (`*.fs`)
  - `effect_stack.json`, `*.effectprofile.json`, `*.stack.json` in the plugin root

**Open folder:** Settings → Profiles → **Open config folder**.

## License

GPL-2.0-only — see [LICENSE](LICENSE).
