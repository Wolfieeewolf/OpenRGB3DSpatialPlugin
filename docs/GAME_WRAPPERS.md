# Game wrappers (vendor capture)

Unified path for **retail games that talk to RGB middleware** (Razer Chroma, Corsair iCUE, Logitech Lightsync, LightFX / AlienFX, etc.). Same idea as the earlier `ambxrt` shim work, but **vendor-neutral** inside the plugin.

See also: [`GAME.md`](GAME.md) (mod / telemetry on UDP **9876**).

## Banner: one pipeline

Everything under **Game wrappers** shares one listener and one spatial effect:

```text
  Game.exe
      │  loads official SDK DLL name (or proxy)
      ▼
  tools/<vendor>_shim/   (wrapper DLL, per middleware)
      │  localhost UDP (or named pipe later)
      ▼
  GameLightingBridge       (single listener, one active source)
      ▼
  Game capture effect      (full-grid default → optional zones / compass / voxel)
      ▼
  3D LED grid
```

**Assumption:** only **one game** runs at a time. The bridge keeps **one** active source; no multi-game merge. If a new frame arrives with a different `source` id, replace the snapshot (optional stale timeout).

## Wrappers vs telemetry

| Path | Input | Typical data |
|------|--------|----------------|
| **Game wrappers** | Proxy SDK DLL beside game | Per-device or per-zone RGB, sometimes one color for all devices |
| **Game telemetry** | Mod / bridge (`GAME.md`) | Pose, damage vector, world lights, voxel frames |

Both can feed the **same grid effect family**; telemetry is better for true 3D (voxel room, directional damage). Wrappers are better for “whatever the game already sent to Chroma.”

## Listener (plugin)

- One module (e.g. `Game/GameLightingBridge`) — **not** amBX-specific naming in the API.
- Frame shape (minimal): `source`, `timestamp_ms`, `sequence`, list of `{ zone_id, r, g, b }` or a single **uniform** color.
- Default port **9877** (telemetry stays **9876** to avoid clashes). Wire format: `Game/GameLightingFormat.h` (`GW01` uniform RGB v0).
- UI: connection status, last source name, optional zone count.

## Effect behaviour (what we do with incoming data)

Depends on payload — start simple, enhance when data allows:

| Received | Default effect | Optional enhancement |
|----------|----------------|----------------------|
| One RGB for all devices (common on Chroma) | **Full grid** solid / pulse | Brightness from plugin stack |
| Per-zone / per-LED colors | Map zones → grid regions | User zone picker later |
| Directional hint (future / telemetry) | — | **Compass** sampler, discrete sectors |
| Voxel / pose (telemetry) | — | **Voxel room** mapping (`AttachRoomMappingPanel`) |

Razer Chroma often drives **all linked devices the same** → a **full-grid capture effect** is the right v0. Compass / voxel are refinements when the stream carries direction or 3D structure.

## Wrapper tools (repo layout)

```
tools/
  game_wrappers/
    README.md           # install: copy DLL next to game exe
    chroma_shim/        # first spike (Artemis/Aurora-style)
    lightfx_shim/       # later
    ...
```

Reference implementations (read, don’t embed C# in the plugin):

- [Artemis.Plugins.Wrappers](https://github.com/Artemis-RGB/Artemis.Plugins.Wrappers) — Chroma, Lightsync, LightFX proxy layout
- [Aurora](https://github.com/Aurora-RGB/Aurora) — end-to-end game + wrapper history (Colore, LightFX extender, LogiLed2Corsair)

## Implementation order

1. `GameLightingBridge` + **Game capture** effect (uniform color from UDP).
2. **Chroma** shim → log frames → match plugin format.
3. Zone mapping in effect UI if shim exposes regions.
4. Wire **compass discrete** / room mapping when input has direction or 3D telemetry.

## Out of scope (separate banner)

- Philips **amBX** engine / `.ambx` binaries (deprecated for this branch).
- Running multiple wrapper sources simultaneously.
