# OpenRGB 3D Spatial LED Control

Plugin for [OpenRGB](https://openrgb.org/) built around one idea: **drive lighting from a 3D layout**—where devices and surfaces sit in space—not only from zone order on a single strip. You place hardware in a **3D room**, run **spatial effects** across that model, and can add **screen capture** (ambilight-style onto geometry), **display planes**, and **game-linked** lighting.

This started as a **for-me** plugin and is still **alpha**: uneven, half-built in places, and not polished. Shared in case someone else wants to try it, break it, or extend it—not as a finished product.

## Player + content

The DLL is a **player** of **engines**. Each engine plays its own list of looks — like a music player with different tracks.

| Engine (library category) | What it plays |
|---------------------------|---------------|
| **Volume** | `effects/spatial/*.fs` (`volumeMain` + FolderVolume header) |
| **Audio** | `effects/audio/*.fs` (same host; shared FFT/onset) |
| **Media** | `effects/media/*.fs` (Volume + image/GIF browse) |
| **Shader Field** | each `effects/shader-field/*.fs` (`spatialMain`) |
| **Kernel / Strip** | `patterns/*.kernel` (1D strip / colormap; not a stack category) |
| **Reactive** | HID pulses (player codec — one row) |
| **Ambilight** | Screen Mirror capture (player codec — one row) |
| **Game** | Minecraft channels (programmed pack) |

Stock files live under the plugin data folder (and in [OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets)). An empty data folder loads **no** file-based looks.

```text
<OpenRGB config>/plugins/settings/OpenRGB3DSpatialPlugin/
  controllers/     device layouts
  effects/
    spatial/       Volume engine content (.fs)
    audio/         Audio engine content (.fs)
    media/         Media engine content (.fs)
    shader-field/  Shader Field engine content (.fs)
  patterns/        strip / colormap kernels (.kernel)
  timelines/       shows + blocks/
```

Install stock content with **Effect Library → Install Stock Pack** (downloads from [OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets)), or **Install from Zip…** for an offline archive. You can also copy folders manually (see that repo’s README). Contracts: [Documentation/effects-engines.md](Documentation/effects-engines.md).

Library categories **are** the engine names. The folder `effects/spatial/` is Volume content on disk — not a separate “Spatial effects” product bucket.

## Who it is for

- You already use OpenRGB and are OK reading a short **README** and poking at options.
- You want **3D-aware** effects and layout (not only “strip modes” on a single device).
- You can tolerate **sharp edges**: features still in motion, UI that fights you, and setup that assumes you will experiment.

If you expect plug-and-play with zero reading, this will probably frustrate you—OpenRGB itself is already a lot for many users.

## Requirements

- **OpenRGB 1.0** (Plugin API **5**), **Qt 6** matching the host (pipeline builds use **6.8.3**). Check **Information → Software Info**.
- Room viewport needs **OpenGL 4.1 Core**. Qt5 hosts are not supported.
- Install the matching release artifact into OpenRGB’s `plugins` folder (or rebuild with `scripts/install-linux-user.sh` for distro OpenRGB). Flatpak OpenRGB needs the Flatpak extension — do not load a host `.so`.

More detail: [CONTRIBUTING.md](CONTRIBUTING.md).

## What exists (high level)

None of this is “done for everyone”—only a map of areas that have code:

- **3D viewport** — place/rotate devices, grid snap, room turntable, gizmo. OpenGL 4.1 Core (MeshBatch / GLSL 410). Expect DPR quirks and ongoing viewport churn.
- **Reference points, display planes, capture zones** — for Screen Mirror / ambilight-style mapping onto geometry.
- **Effect stack** — pick an **engine** in the library, then a look that engine plays (Volume / Audio / Media / Shader Field files; Reactive / Ambilight / Game codecs). Quality varies a lot by look.
- **Effect packs + Event Bindings** — timeline packs and Manual / OS / game-style triggers. Design notes: [Documentation/effect-event-maker.md](Documentation/effect-event-maker.md).
- **OpenRGB profiles** — layout + effects round-trip through the host profile payload (current schema only; no legacy dual loaders).
- **Minecraft bridge** — Room Ambilight over **sparse cubemap SHM** (mod ≥ 0.9.46) plus UDP vitals/damage. Fabric mod under [integrations/minecraft/](integrations/minecraft/). **Alpha:** works well enough on **vanilla**; not meaningfully tested against big modpacks.

## Documentation

Browsable guide: **[Wiki](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/wiki)** (user + developer pages).

Contracts that ship with the code live under **[Documentation/](Documentation/)**:

| Doc | When to read it |
| --- | --- |
| [effects-engines.md](Documentation/effects-engines.md) | Engines as players; library categories; FolderVolume / codecs |
| [shader-conversion.md](Documentation/shader-conversion.md) | Porting 1D / 2D / 3D shaders into those engines |
| [PluginSpatialMeasurement.md](Documentation/PluginSpatialMeasurement.md) | Layout math, mm ↔ grid, RoomGrid, spacing, viewport / effects contracts |
| [SpatialMeasurement.md](Documentation/SpatialMeasurement.md) | Minecraft / telemetry bridge (RoomGrid → game world, sparse cubemap SHM) |
| [effect-event-maker.md](Documentation/effect-event-maker.md) | Effect packs + Event Bindings design |
| [examples/](Documentation/examples/) | Sample `.oreffect.json` pack |

Stock layouts and effect files: **[OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets)**.

Upstream OpenRGB docs stay in the `OpenRGB/` submodule (`OpenRGB/Documentation/…`). Day-to-day contribution rules: [CONTRIBUTING.md](CONTRIBUTING.md).

## What to expect (honest status)

The whole plugin is **experimental**. Pieces land at different levels of polish:

- **Spatial layout / viewport** — Backbone of the project. Furthest along, still evolving, and still easy to confuse (pivots, wipe directions, gizmo feel, DPI).
- **File-loaded looks** — Volume / Audio / Media / Shader Field content on disk; missing files means missing rows in that engine’s list. Still alpha UX.
- **Packs / events** — Real path, not a mock—but authoring and bindings are early.
- **Screen mirror / ambilight** — **Works** for some setups, still **experimental**. Capture stays on the CPU; room fields use a GPU volume atlas.
- **Reactive** — Keyboard / mouse / gamepad pulses from layout positions (privacy: no key logging). Mapping depends on OpenRGB LED names.
- **Gaming** — Minecraft path above is **vanilla-shaped alpha**, not a supported-mod matrix. Other games are “bring your own telemetry story.”

Until you have tried a feature on **your** PC, treat it as **unproven** for you—not “done” for everyone.

## Contributing / issues

Source of truth and PRs: **GitHub** (see [CONTRIBUTING.md](CONTRIBUTING.md)). The GitLab copy is a mirror/backup. Bug reports need versions and steps; “it doesn’t work” without that may get closed.

Controller JSON and stock effect files: **[OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets)**.

## License

GPL-2.0-only — see [LICENSE](LICENSE).

## Support

This is still a for-me alpha. If you want to throw something in the tip jar anyway, I like **pizza** more than coffee:

<a href="https://buymeacoffee.com/wolfieee"><img src="https://img.buymeacoffee.com/button-api/?text=Buy%20me%20a%20pizza&emoji=%F0%9F%8D%95&slug=wolfieee&button_colour=FFDD00&font_colour=000000&font_family=Cookie&outline_colour=000000&coffee_colour=ffffff" alt="Buy me a pizza" /></a>

[buymeacoffee.com/wolfieee](https://buymeacoffee.com/wolfieee)
