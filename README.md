# OpenRGB 3D Spatial LED Control

Plugin for [OpenRGB](https://openrgb.org/) built around one idea: **drive lighting from a 3D layout**—where devices and surfaces sit in space—not only from zone order on a single strip. You place hardware in a **3D room**, run **spatial effects** across that model, and can add **screen capture** (ambilight-style onto geometry), **display planes**, and **game-linked** lighting.

This started as a **for-me** plugin and is still **alpha**: uneven, half-built in places, and not polished. Shared in case someone else wants to try it, break it, or extend it—not as a finished product.

## Player + content

The DLL is a **player**. Effect looks, strip kernels, Shader Field presets, and timeline blocks live as **files** under the plugin data folder (and in the stock [OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets) repo). An empty data folder loads **no** stock effects.

```text
<OpenRGB config>/plugins/settings/OpenRGB3DSpatialPlugin/
  controllers/     device layouts
  effects/
    spatial/       FolderVolume room effects (.fs)
    audio/         audio FolderVolume effects (.fs)
    media/         texture / shape media (.fs)
    shader-field/  2D Shader Field presets (.fs)
  patterns/        strip / colormap kernels (.kernel)
  timelines/       shows + blocks/
```

Install stock content by copying from the presets repo into those folders (see that repo’s README).

### Effects engines

Most looks are **thin content** on a small set of **engines** in the player:

| Engine | Content | Notes |
|--------|---------|--------|
| **Volume** | `effects/spatial/*.fs` | `volumeMain` + FolderVolume header (`class:`, `global:`, `param:`, `finish:`) |
| **Audio** | `effects/audio/*.fs` | Same host; shared FFT/onset service |
| **Media** | `effects/media/*.fs` | Volume + image/GIF browse |
| **Shader Field** | `effects/shader-field/*.fs` | `spatialMain` 2D fields projected into the room |
| **Kernel / Strip** | `patterns/*.kernel` | 1D strip / colormap patterns |

**Built-in engines** (not a content farm): **Reactive** (keyboard/mouse/gamepad pulses), **Screen Mirror** / ambilight (capture), **Minecraft** game channels. Contracts: [Documentation/effects-engines.md](Documentation/effects-engines.md).

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
- **Effect stack** — file-loaded spatial / audio / media looks, Shader Field presets, strip kernels, plus built-in **Reactive**, Screen Mirror, and game bridges. Quality varies a lot by effect.
- **Effect packs + Event Bindings** — timeline packs and Manual / OS / game-style triggers. Design notes: [Documentation/effect-event-maker.md](Documentation/effect-event-maker.md).
- **OpenRGB profiles** — layout + effects round-trip through the host profile payload (current schema only; no legacy dual loaders).
- **Minecraft bridge** — Room Ambilight over **sparse cubemap SHM** (mod ≥ 0.9.46) plus UDP vitals/damage. Fabric mod under [integrations/minecraft/](integrations/minecraft/). **Alpha:** works well enough on **vanilla**; not meaningfully tested against big modpacks.

## Documentation

Browsable guide: **[Wiki](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/wiki)** (user + developer pages).

Contracts that ship with the code live under **[Documentation/](Documentation/)**:

| Doc | When to read it |
| --- | --- |
| [effects-engines.md](Documentation/effects-engines.md) | Engines vs content; FolderVolume / Audio / Media / Shader Field / Kernel rules |
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
- **File-loaded effects** — Spatial, audio, and media looks are FolderVolume content; Shader Field and kernels are disk presets. Still alpha UX; missing files means missing effects.
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
