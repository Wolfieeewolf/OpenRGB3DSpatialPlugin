# OpenRGB 3D Spatial LED Control

Plugin for [OpenRGB](https://openrgb.org/) built around one idea: **drive lighting from a 3D layout**—where devices and surfaces sit in space—not only from zone order on a single strip. You place hardware in a **3D room**, run **spatial effects** across that model, and can add **screen capture** (ambilight-style onto geometry), **display planes**, and **game-linked** lighting.

This started as a **for-me** plugin and is still **alpha**, but the core path is solid enough to use daily: room layout, effect stack, Screen Mirror, and the presets content pack. Shared so others can try it, break it, or extend it—not as a finished product for everyone.

## Engine + content

This repo is the **raw engine**: spatial layout, Effect Library loaders, Ambilight, Reactive, Minecraft codecs, Action Editor / Triggers / Movie Sync **players**. It ships **no looks**.

All drop-in content (`.fs`, `.kernel`, Action Tools `.fx`, trigger/Movie Sync actions, catalogs, VLC helper, controller layouts) comes from [OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets) — copy into AppData.

| Engine (in the DLL) | Content folder (presets pack) |
|---------------------|-------------------------------|
| **Volume** | `effects/volume/*.fs` |
| **Audio** | `effects/audio/*.fs` |
| **Media** | `effects/media/*.fs` (textures/GIF — not movies) |
| **Shader Field** | `effects/shader-field/*.fs` |
| **Patterns** | `effects/patterns/*.kernel` |
| **Actions** (Action Editor) | `action-editor/toolbox/` + timelines in `triggers/` / `movie-sync/` |
| **Movie Sync** | `movie-sync/` + video sidecars |
| **Triggers** | `triggers/triggers.json` + `triggers/effects/` |
| **Ambilight / Reactive / Game** | codecs in the DLL (Minecraft bridge mod under `integrations/minecraft/`) |

```text
<OpenRGB config>/plugins/settings/OpenRGB3DSpatialPlugin/
  controllers/                         (nested OK — same as presets pack)
  effects/volume|audio|media|shader-field|patterns/
  action-editor/
    toolbox/
    gradients.json | colors.json | curves.json   (create-on-save)
  triggers/
    effects/
    triggers.json                                (create-on-save)
  movie-sync/
    catalog.json
    effects/<category>/
    vlc/
  meshes/                                        (scene props; plugin-only)
```

Map: [Documentation/content-library.md](Documentation/content-library.md).

## Who it is for

- You already use OpenRGB and are OK reading a short **README** and poking at options.
- You want **3D-aware** effects and layout (not only “strip modes” on a single device).
- You can tolerate **sharp edges**: features still in motion, and setup that assumes you will experiment.

If you expect plug-and-play with zero reading, this will probably frustrate you—OpenRGB itself is already a lot for many users.

## Requirements

- **OpenRGB 1.0** (Plugin API **5**), **Qt 6** matching the host (pipeline builds use **6.8.3**). Check **Information → Software Info**.
- Room viewport needs **OpenGL 4.1 Core**. Qt5 hosts are not supported.
- **Windows:** prefer the release **MSI** (`OpenRGB_3D_Spatial_Plugin_Windows_64_msi.zip`). It installs the DLL into OpenRGB’s `plugins` folder and can install the [presets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets) content pack into AppData in the same run:
  - **Typical** — plugin + effect looks + Action tools + Triggers + Movie Sync (no controller layouts).
  - **Complete** — Typical plus every controller layout.
  - **Custom** — Advanced checkbox tree (pick effect families and only the controller brands you own).
  Or drop the bare `.dll` / zip yourself and copy the presets repo by hand. Flatpak OpenRGB needs the Flatpak extension — do not load a host `.so`. Linux/macOS: matching release artifact, or rebuild with `scripts/install-linux-user.sh` for distro OpenRGB.

### Building from source

Baseline toolchain matches upstream OpenRGB ([`OpenRGB/Documentation/Compiling.md`](OpenRGB/Documentation/Compiling.md)): Git, Qt, and a C++ compiler (MSVC on Windows).

**Extras for this plugin:**

1. Clone with submodules: `git clone --recurse-submodules …` (or `git submodule update --init`).
2. **CMake** (needed once so the first Build can compile Assimp for mesh import — OpenRGB itself does not require it):
   - **Windows:** Qt Maintenance Tool → *Build Tools* → **CMake**, **or** Visual Studio Installer → **C++ CMake tools for Windows** (either is enough). Ninja is optional. If you use **VS 2026**, prefer a CMake that lists the `Visual Studio 18 2026` generator (VS’s own CMake tools, or CMake 4.x) — Qt’s older *Build Tools* CMake may only know VS 2022.
   - **Linux:** install `cmake` from your distro (`apt` / `dnf` / …).
   - **macOS:** `brew install cmake`.
3. Open `OpenRGB3DSpatialPlugin.pro` in Qt Creator (or qmake) and Build. Assimp is built automatically into the build tree (`_deps/`) on first link. Normal **Clean** keeps `_deps` (so the next Build stays fast); deleting the whole build folder or `scripts/build-assimp.* -Clean` / `--clean` forces an Assimp rebuild. You do not need to run those scripts by hand for day-to-day work.

More detail: [CONTRIBUTING.md](CONTRIBUTING.md).

## What exists (high level)

A map of areas that have real code today:

- **3D viewport** — place/rotate devices, grid snap, room turntable, gizmo. OpenGL 4.1 Core (MeshBatch / GLSL 410). Pick/gizmo/camera use DPR-aware mapping (logical feel, framebuffer pick).
- **Reference points, display planes, capture zones** — for Screen Mirror / ambilight-style mapping onto geometry.
- **Effect stack** — pick an **engine** in the library, then a look that engine plays (Volume / Audio / Media / Shader Field files; Reactive / Ambilight / Game codecs).
- **Actions + Triggers** — Action Editor timelines; Manual / OS / game-style Triggers. Design notes: [Documentation/effect-event-maker.md](Documentation/effect-event-maker.md).
- **OpenRGB profiles** — layout + effects round-trip through the host profile payload (current schema only).
- **Minecraft bridge** — Room Ambilight over **sparse cubemap SHM** (mod ≥ 0.9.46) plus UDP vitals/damage. Fabric mod under [integrations/minecraft/](integrations/minecraft/). Works well enough on **vanilla**; not a modpack matrix.

## Documentation

Browsable guide: **[Wiki](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/wiki)** (user + developer pages).

Contracts that ship with the code live under **[Documentation/](Documentation/)**:

| Doc | When to read it |
| --- | --- |
| [effects-engines.md](Documentation/effects-engines.md) | Engines as players; library categories; FolderVolume / codecs |
| [shader-conversion.md](Documentation/shader-conversion.md) | Porting 1D / 2D / 3D shaders into those engines |
| [PluginSpatialMeasurement.md](Documentation/PluginSpatialMeasurement.md) | Layout math, mm ↔ grid, RoomGrid, spacing, viewport / effects contracts |
| [SpatialMeasurement.md](Documentation/SpatialMeasurement.md) | Minecraft / telemetry bridge (RoomGrid → game world, sparse cubemap SHM) |
| [effect-event-maker.md](Documentation/effect-event-maker.md) | Actions + Triggers design |
| [content-library.md](Documentation/content-library.md) | AppData folder map (content lives in the presets repo) |

Looks, layouts, tools: **[OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets)**.

Upstream OpenRGB docs stay in the `OpenRGB/` submodule (`OpenRGB/Documentation/…`). Day-to-day contribution rules: [CONTRIBUTING.md](CONTRIBUTING.md).

## What to expect (honest status)

Still **alpha**, with uneven polish by area:

- **Spatial layout / viewport** — Backbone. Furthest along; pivots, wipe directions, and DPI can still confuse.
- **File-loaded looks** — Volume / Audio / Media / Shader Field content on disk; missing files means missing rows. UX is early but usable.
- **Actions / Triggers** — Real path, not a mock — authoring UX is still early.
- **Screen Mirror / ambilight** — Daily-driver capable for plane + Reach setups. UV mapping is an **outside / convex** sphere from the plane (light out from the glass). Clips that dive *into* a tunnel will feel wrong by design. Capture stays on the CPU; room fields use a GPU volume atlas.
- **Reactive** — Keyboard / mouse / gamepad pulses from layout positions (privacy: no key logging). Mapping depends on OpenRGB LED names.
- **Gaming** — Minecraft path above is **vanilla-shaped alpha**. Other games are “bring your own telemetry story.”

Until you have tried a feature on **your** PC, treat it as **unproven** for you—not “done” for everyone.

## Contributing / issues

Source of truth and PRs: **GitHub** (see [CONTRIBUTING.md](CONTRIBUTING.md)). The GitLab copy is a mirror/backup. Bug reports need versions and steps; “it doesn’t work” without that may get closed.

Controller JSON and all effect/content files: **[OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets)**.

## License

GPL-2.0-only — see [LICENSE](LICENSE).

## Support

This is still a for-me alpha. If you want to throw something in the tip jar anyway, I like **pizza** more than coffee:

<a href="https://buymeacoffee.com/wolfieee"><img src="https://img.buymeacoffee.com/button-api/?text=Buy%20me%20a%20pizza&emoji=%F0%9F%8D%95&slug=wolfieee&button_colour=FFDD00&font_colour=000000&font_family=Cookie&outline_colour=000000&coffee_colour=ffffff" alt="Buy me a pizza" /></a>

[buymeacoffee.com/wolfieee](https://buymeacoffee.com/wolfieee)
