# OpenRGB 3D Spatial LED Control

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
- Save and load the complete spatial layout and effect stack through **OpenRGB profiles**.
- Reuse global custom-controller definitions, stack presets, and shader assets across profiles.

## What to expect (honest status)

The whole plugin is **experimental**. Pieces land at different levels of polish:

- **Spatial layout and stacking** — This is the backbone: 3D grid, overlays, and driving LEDs from that model. It is the most “together” part of the project, and still evolving.
- **Effects** — A **mix**. Some effects feel great and behave well in 3D; others are **lackluster** or need more tuning, presets, or UX. Treat the library as a grab bag until you find favorites that match your setup.
- **Screen mirror / ambilight** — **Works**, but it is **experimental**, especially the idea of **mapping live capture into 3D space** (planes, zones, room grid). Expect to tune capture, geometry, and quality settings; behavior can depend on GPU, HDR, and Windows compositor quirks.
- **Gaming integration** — **Minecraft** room VR lighting uses **room-sample shared memory** (plus UDP telemetry for health/world tint and similar). Treat it as functional best-effort; report issues you hit.

Until you have tried a feature on **your** PC, treat it as **unproven** for you—not “done” for everyone.

## License

GPL-2.0-only — see [LICENSE](LICENSE).
