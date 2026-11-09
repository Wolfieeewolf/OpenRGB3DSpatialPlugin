# Game wrappers (v2)

Retail **SDK proxy DLLs** (Chroma, etc.) send lighting to the plugin over UDP **9877**. **Telemetry** stays on **9876** — see [`SpatialMeasurement.md`](SpatialMeasurement.md) and the Minecraft bridge.

## What exists in the plugin today

| Piece | Status |
|-------|--------|
| `GameLightingBridge` | Listens on `127.0.0.1:9877`, parses **GW01** (`engines/game/Game/GameLightingFormat.h`) |
| `GameTelemetryBridge` | Listens on `127.0.0.1:9876`, JSON + room-sample SHM |
| Capture spatial effect | **Game wrapper capture** — uniform RGB from GW01 v0 |

## GW01 v0 (shim contract)

- **53 bytes**, magic `GW01`, frame type `UniformRgb` (one RGB for full grid)
- **Source** string (e.g. `chroma_shim`) in frame bytes 17–40

Implement new shims under `tools/game_wrappers/` only.

## v2 rules (avoid v0 mistakes)

1. **One game, one API** before any universal installer.
2. **Correct DLL name only** — Colore: `RzChromaSDK64.dll`; Unity: `UnityNativeChromaSDK(.dll|3.dll)`; native: `RazerChromaSDK64.dll`.
3. Never replace retail `Corale.Colore.dll` in `Managed/`.
4. Success = game runs + stable GW01 during play + visible color changes on **Game wrapper capture**.

## Repo layout

```
tools/game_wrappers/                    ← shims and install scripts
engines/game/Game/GameLightingBridge.*
engines/game/Game/GameLightingFormat.h
engines/game/Game/WinsockLifetime.*     ← shared WSAStartup with telemetry
engines/game/GameWrappers/              ← capture effect
```
