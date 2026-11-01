# OpenRGB 3D Spatial LED Control

Plugin for [OpenRGB](https://openrgb.org/) built around one idea: **drive lighting from a 3D layout**â€”where devices and surfaces sit in spaceâ€”not only from zone order on a single strip. You place hardware in a **3D room**, run **spatial effects** across that model, and can add **screen capture** (ambilight-style onto geometry), **display planes**, and **game-linked** lighting.

This started as a **for-me** plugin and is still **alpha**: uneven, half-built in places, and not polished. Shared in case someone else wants to try it, break it, or extend itâ€”not as a finished product.

## Engine + content

This repo is the **raw engine**: spatial layout, Effect Library loaders, Ambilight, Reactive, Minecraft codecs, Action Editor / Triggers / Movie Sync **players**. It ships **no looks**.

All drop-in content (`.fs`, `.kernel`, Action Tools `.fx`, trigger/Movie Sync actions, catalogs, VLC helper, controller layouts) comes from [OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets) — copy-paste into AppData.

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
- You want **3D-aware** effects and layout (not only â€œstrip modesâ€ on a single device).
- You can tolerate **sharp edges**: features still in motion, UI that fights you, and setup that assumes you will experiment.

If you expect plug-and-play with zero reading, this will probably frustrate youâ€”OpenRGB itself is already a lot for many users.

## Requirements

- **OpenRGB 1.0** (Plugin API **5**), **Qt 6** matching the host (pipeline builds use **6.8.3**). Check **Information â†’ Software Info**.
- Room viewport needs **OpenGL 4.1 Core**. Qt5 hosts are not supported.
- Install the matching release artifact into OpenRGBâ€™s `plugins` folder (or rebuild with `scripts/install-linux-user.sh` for distro OpenRGB). Flatpak OpenRGB needs the Flatpak extension â€” do not load a host `.so`.

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

None of this is â€œdone for everyoneâ€â€”only a map of areas that have code:

- **3D viewport** â€” place/rotate devices, grid snap, room turntable, gizmo. OpenGL 4.1 Core (MeshBatch / GLSL 410). Expect DPR quirks and ongoing viewport churn.
- **Reference points, display planes, capture zones** â€” for Screen Mirror / ambilight-style mapping onto geometry.
- **Effect stack** â€” pick an **engine** in the library, then a look that engine plays (Volume / Audio / Media / Shader Field files; Reactive / Ambilight / Game codecs). Quality varies a lot by look.
- **Actions + Triggers** — Action Editor timelines; Manual / OS / game-style Triggers. Design notes: [Documentation/effect-event-maker.md](Documentation/effect-event-maker.md).

- **OpenRGB profiles** â€” layout + effects round-trip through the host profile payload (current schema only; no legacy dual loaders).
- **Minecraft bridge** â€” Room Ambilight over **sparse cubemap SHM** (mod â‰¥ 0.9.46) plus UDP vitals/damage. Fabric mod under [integrations/minecraft/](integrations/minecraft/). **Alpha:** works well enough on **vanilla**; not meaningfully tested against big modpacks.

## Documentation

Browsable guide: **[Wiki](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/wiki)** (user + developer pages).

Contracts that ship with the code live under **[Documentation/](Documentation/)**:

| Doc | When to read it |
| --- | --- |
| [effects-engines.md](Documentation/effects-engines.md) | Engines as players; library categories; FolderVolume / codecs |
| [shader-conversion.md](Documentation/shader-conversion.md) | Porting 1D / 2D / 3D shaders into those engines |
| [PluginSpatialMeasurement.md](Documentation/PluginSpatialMeasurement.md) | Layout math, mm â†” grid, RoomGrid, spacing, viewport / effects contracts |
| [SpatialMeasurement.md](Documentation/SpatialMeasurement.md) | Minecraft / telemetry bridge (RoomGrid â†’ game world, sparse cubemap SHM) |
| [effect-event-maker.md](Documentation/effect-event-maker.md) | Actions + Triggers design |
| [content-library.md](Documentation/content-library.md) | AppData folder map (content lives in the presets repo) |

Looks, layouts, tools: **[OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets)**.

Upstream OpenRGB docs stay in the `OpenRGB/` submodule (`OpenRGB/Documentation/â€¦`). Day-to-day contribution rules: [CONTRIBUTING.md](CONTRIBUTING.md).

## What to expect (honest status)

The whole plugin is **experimental**. Pieces land at different levels of polish:

- **Spatial layout / viewport** â€” Backbone of the project. Furthest along, still evolving, and still easy to confuse (pivots, wipe directions, gizmo feel, DPI).
- **File-loaded looks** â€” Volume / Audio / Media / Shader Field content on disk; missing files means missing rows in that engineâ€™s list. Still alpha UX.
- **Actions / Triggers** — Real path, not a mock — but authoring UX is still early.

- **Screen mirror / ambilight** â€” **Works** for some setups, still **experimental**. Capture stays on the CPU; room fields use a GPU volume atlas.
- **Reactive** â€” Keyboard / mouse / gamepad pulses from layout positions (privacy: no key logging). Mapping depends on OpenRGB LED names.
- **Gaming** â€” Minecraft path above is **vanilla-shaped alpha**, not a supported-mod matrix. Other games are â€œbring your own telemetry story.â€

Until you have tried a feature on **your** PC, treat it as **unproven** for youâ€”not â€œdoneâ€ for everyone.

## Contributing / issues

Source of truth and PRs: **GitHub** (see [CONTRIBUTING.md](CONTRIBUTING.md)). The GitLab copy is a mirror/backup. Bug reports need versions and steps; â€œit doesnâ€™t workâ€ without that may get closed.

Controller JSON and all effect/content files: **[OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets)**.

## License

GPL-2.0-only â€” see [LICENSE](LICENSE).

## Support

This is still a for-me alpha. If you want to throw something in the tip jar anyway, I like **pizza** more than coffee:

<a href="https://buymeacoffee.com/wolfieee"><img src="https://img.buymeacoffee.com/button-api/?text=Buy%20me%20a%20pizza&emoji=%F0%9F%8D%95&slug=wolfieee&button_colour=FFDD00&font_colour=000000&font_family=Cookie&outline_colour=000000&coffee_colour=ffffff" alt="Buy me a pizza" /></a>

[buymeacoffee.com/wolfieee](https://buymeacoffee.com/wolfieee)
