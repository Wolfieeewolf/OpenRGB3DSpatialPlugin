# OpenRGB 3D Spatial LED Control

Plugin for [OpenRGB](https://openrgb.org/) built around one idea: **drive lighting from a 3D layout**—where devices and surfaces sit in space—not only from zone order on a single strip. You place hardware in a **3D room**, run **spatial effects** across that model, and can add **screen capture** (ambilight-style onto geometry), **display planes**, and **game-linked** lighting. Waves, mirroring, sampling, and similar ideas use position in the grid so the rig matches how you arranged it.

This started as a **for-me** plugin and is still **alpha**: uneven, half-built in places, and not polished. Some areas work okay for the workflows I use; others are stubs, test benches, or mid-rebuild. Shared in case someone else wants to try it, break it, or extend it—not as a finished product.

## Who it is for

- You already use OpenRGB and are OK reading a short **README** and poking at options.
- You want **3D-aware** effects and layout (not only “strip modes” on a single device).
- You can tolerate **sharp edges**: features still in motion, UI that fights you, and setup that assumes you will experiment.

If you expect plug-and-play with zero reading, this will probably frustrate you—OpenRGB itself is already a lot for many users.

## Requirements (practical)

- Recent **OpenRGB** with plugins enabled (see this repo’s `OpenRGB/` submodule / CI for the tree we build against).
- **Qt 6.8** and a GPU/driver that can run an **OpenGL 4.1 Core** context for the room viewport. Older Qt / no Core GL is not a supported dual path.

Build and contribution detail lives in [CONTRIBUTING.md](CONTRIBUTING.md).

## What exists (high level)

None of this is “done for everyone”—only a map of areas that have code:

- **3D viewport** — place/rotate devices, grid snap, room turntable, gizmo. Now on OpenGL 4.1 Core (MeshBatch / GLSL 410). Expect DPR quirks, thin lines on some Core drivers, and ongoing viewport churn.
- **Reference points, display planes, capture zones** — for Screen Mirror / ambilight-style mapping onto geometry.
- **Effect stack** — spatial effects (wave, plasma, textures, audio bands, game bridges, …). Quality varies a lot by effect.
- **Effect packs + Event Bindings** — timeline packs and Manual / OS / game-style triggers. Usable enough to author and fire things; still alpha (catalogs, UX, and edge cases in flux). Design notes: [Documentation/effect-event-maker.md](Documentation/effect-event-maker.md).
- **OpenRGB profiles** — layout + effects round-trip through the host profile payload (current schema only; no legacy dual loaders).
- **Minecraft bridge** — Room Ambilight over room-sample SHM plus UDP vitals/damage. Fabric mod under `[integrations/minecraft/](integrations/minecraft/)`. Best-effort.



## What to expect (honest status)

The whole plugin is **experimental**. Pieces land at different levels of polish:

- **Spatial layout / viewport** — Backbone of the project. Furthest along, still evolving, and still easy to confuse (pivots, wipe directions, gizmo feel, DPI).
- **Effects** — A **mix**. Some feel great in 3D; others are lackluster, half-ported, or need more tuning. Grab-bag until you find what matches your rig.
- **Packs / events** — Real path, not a mock—but authoring and bindings are early. Expect rough UI and “why did that not fire?” moments.
- **Screen mirror / ambilight** — **Works** for some setups, still **experimental** when mapping live capture into 3D (planes, zones, room grid). GPU / HDR / compositor quirks apply.
- **Gaming** — Minecraft path above; other games are “bring your own telemetry story.” Don’t assume a title is supported because someone asked for it.

Until you have tried a feature on **your** PC, treat it as **unproven** for you—not “done” for everyone.

## Contributing / issues

Source of truth and PRs: **GitHub** (see [CONTRIBUTING.md](CONTRIBUTING.md)). The GitLab copy is a mirror/backup. Bug reports need versions and steps; “it doesn’t work” without that may get closed.

## License

GPL-2.0-only — see [LICENSE](LICENSE).

## Support

This is still a for-me alpha. If you want to throw something in the tip jar anyway, I like **pizza** more than coffee:

<a href="https://buymeacoffee.com/wolfieee"><img src="https://img.buymeacoffee.com/button-api/?text=Buy%20me%20a%20pizza&emoji=%F0%9F%8D%95&slug=wolfieee&button_colour=FFDD00&font_colour=000000&font_family=Cookie&outline_colour=000000&coffee_colour=ffffff" alt="Buy me a pizza" /></a>

[buymeacoffee.com/wolfieee](https://buymeacoffee.com/wolfieee)
