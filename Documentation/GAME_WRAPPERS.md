# Game wrappers (universal Chroma middleman)

Titles keep calling their vendor lighting SDK; our per-vendor shim sends full grids into OpenRGB. **Razer first**; Logitech / iCUE / amBX are stub lanes.

Vendor code is isolated under `tools/game_wrappers/<vendor>/` so changing one SDK cannot break another. Only the shared wire (`common/`) and the plugin capture effect are cross-vendor.

**Scope (live):** titles that load a local Razer Chroma SDK DLL (`RzChromaSDK64.dll` / `RzChromaSDK.dll`). Drop in the Razer shim → UDP **9877**.

Games without that hook (Stardew, Minecraft, …) stay on **game integrations** (SMAPI / Fabric mods → telemetry **9876** or a dedicated emitter), not the DLL mapper.

## Architecture

```
Game  →  vendor shim (tools/game_wrappers/razer|logitech|icue|ambx/…)
      →  UDP GW01 DeviceGrid @ 127.0.0.1:9877   ← common/
      →  GameLightingBridge
      →  Game wrapper capture effect
            └─ remap each channel → one or more OpenRGB destinations
```

- **Shim is per-vendor, not per-game** — one Razer DLL (`RzChromaSDK64.dll` / `RzChromaSDK.dll`) installed next to each Chroma title.
- **Plugin owns remapping** — game says “keyboard”; you can send that channel to several OpenRGB targets at once (e.g. headset **and** ceiling).
- Install script ensures registry `HKLM\SOFTWARE\WOW6432Node\Razer Chroma SDK` `Enable=1` (needed by Colore; harmless otherwise).

## Finding more DLL games

Look in a game folder for any of:

| Marker | Meaning |
|--------|---------|
| `Corale.Colore.dll` (often `*_Data\Managed\`) | Unity + Colore → needs `RzChromaSDK64.dll` (or x86 `RzChromaSDK.dll`) beside the exe |
| `CChromaEditorLibrary.dll` / `UnityNativeChromaSDK*` | Native/editor bridge → loads `RzChromaSDK*.dll` |
| Shipping `RzChromaSDK*.dll` | Already expects a local SDK DLL — replace with our shim (keep a `.bak`) |

Skip titles that only light via Synapse cloud / no local SDK load, and skip mod-loader games (use integrations instead).

## Wire format (GW01)

| `frame_type` | Size | Meaning |
|--------------|------|---------|
| `2` DeviceGrid | 45 B + `rows*cols*3` | Full Chroma matrix (RGB888, row-major) for one device |

Device slots (fixed, universal): Keyboard, Mouse, Mousepad, Headset, Keypad, ChromaLink.

Grids are sent **verbatim**. The capture effect samples real cells on the **matched OpenRGB controller** (local UV), or room XYZ bands for spatial targets — not a single room-wide average unless you map to Whole room.

## Capture effect — multi-destination remap + per-game profiles

Each of the six Chroma slots has:

1. **Enable** — use this game channel at all.
2. **→ play on** — **multi-check** menu of OpenRGB destinations (peripheral types and/or room regions). One stream can light several targets; overlapping LEDs use **max-blend** (keep the brighter colour) when that option is on.

| Target | What lights |
|--------|-------------|
| Keyboard / Mouse / Mousepad / Headset / Keypad | Matching OpenRGB device types only |
| Room lights (all strips) | Any OpenRGB controller in the map |
| Whole room / Desk / Floor / Ceiling / Left / Right | LEDs in that room XYZ band |

**Game profile** (effect UI):

- Auto-switches with the live shim source (`DubWars.exe`, `Cat_Quest_II.exe`, …).
- **Save** (button, context menu, **Ctrl+S**), **Copy** from another saved game, **Reset**, **Template** (Keyboard→whole room / all→whole room / peripheral identity).
- Idle channels dim when the live snapshot is not painting them.
- Default for a new game: enable only channels seen active (fallback Keyboard→whole room) so ChromaLink washes do not bury keyboard fire.

Maps are stored **per game** inside the effect settings JSON. Saving the effect (or Ctrl+S on the profile panel) keeps every game’s map so the next session restores it automatically.

## Capture logs

While a shimmed game runs, the DLL writes `%TEMP%\openrgb_gw_capture_<exe>_<pid>.log` (pointer: `openrgb_gw_capture_latest.log`). `UnInit` and process detach both flush the session summary. Env `OPENRGB_GW_CAPTURE`: `full` (default) / `sampled` / `0`.

## Build / install

- Index: [`tools/game_wrappers/README.md`](../tools/game_wrappers/README.md)
- Razer: [`tools/game_wrappers/razer/README.md`](../tools/game_wrappers/razer/README.md)
