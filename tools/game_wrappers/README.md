# Game wrappers

Per-vendor SDK proxies that divert game lighting into OpenRGB over **UDP :9877** (`GW01` DeviceGrid). The plugin effect **Game → Game Wrappers → Game wrapper capture** remaps channels to OpenRGB destinations.

**Not for** mod-loader games (Stardew/SMAPI, Minecraft/Fabric) — those use **game integrations**, not these DLLs.

## Layout

| Folder | Vendor / SDK | Status |
|--------|--------------|--------|
| [`common/`](common/) | Shared `GW01` UDP emit + capture log | Live — used by every vendor shim |
| [`razer/`](razer/) | Razer Chroma (`RzChromaSDK*`) | **Live** |
| [`logitech/`](logitech/) | Logitech G Hub / LED Illumination | Stub — not started |
| [`icue/`](icue/) | Corsair iCUE SDK | Stub — not started |
| [`ambx/`](ambx/) | Philips amBX | Stub — not started |

Keep each vendor’s shim, install scripts, and catalogs **inside its own folder**. Do not share vendor-specific code across lanes — only `common/` (wire format) and the plugin capture effect are shared.

## Plugin (shared)

- Listener: `engines/game/Game/GameLightingBridge.cpp` — `127.0.0.1:9877`
- Format: `engines/game/Game/GameLightingFormat.h` (keep `common/gw_udp_emit.*` in sync)
- Effect: **Game wrapper capture** — multi-destination remap + per-game profiles

See [`Documentation/GAME_WRAPPERS.md`](../../Documentation/GAME_WRAPPERS.md).

## Razer (start here)

```powershell
cmake -S tools/game_wrappers/razer/chroma_shim -B tools/game_wrappers/razer/chroma_shim/build -A x64
cmake --build tools/game_wrappers/razer/chroma_shim/build --config Release
.\tools\game_wrappers\razer\install_chroma_shim.ps1 -GameExePath "D:\Steam\steamapps\common\DubWars\DubWars.exe"
```

Full Razer docs: [`razer/README.md`](razer/README.md).
