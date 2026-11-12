# Razer Chroma game wrapper

Games keep calling Razer’s SDK; our `RzChromaSDK*.dll` shim sends full grids to the plugin (`UDP :9877`). Map each of the six Chroma channels to **one or more** OpenRGB destinations (e.g. Headset → headset **and** ceiling), saved **per game exe**.

This folder is **Razer-only**. Logitech / iCUE / amBX live in sibling folders under `tools/game_wrappers/`.

Razer’s marketing catalog (not a DLL guarantee): [Chroma Workshop → Games](https://www.razer.com/chroma-workshop#--games) feeds from  
`https://razer-assets2.s3.us-west-1.amazonaws.com/json/chroma-workshop/games.json` (~300 titles).  
Name dump: `chroma_workshop_games.txt`. Still verify each install for `RzChromaSDK*`, Colore, or `CChromaEditor*` before shim install.

## Build shim (Windows)

```powershell
cmake -S tools/game_wrappers/razer/chroma_shim -B tools/game_wrappers/razer/chroma_shim/build -A x64
cmake --build tools/game_wrappers/razer/chroma_shim/build --config Release
```

Output: **`RzChromaSDK64.dll`**. Use `-A Win32` for 32-bit → `RzChromaSDK.dll` (build dir `build32`).

Shared wire helpers are linked from [`../common/`](../common/) (`gw_udp_emit`, `gw_capture_log`).

## Install next to a game

```powershell
.\tools\game_wrappers\razer\install_chroma_shim.ps1 -GameExePath "D:\Steam\steamapps\common\DubWars\DubWars.exe"
```

Always installs as `RzChromaSDK*.dll` (the name games `LoadLibrary`). Replaces any leftover `RazerChromaSDK*` beside the exe. Elevates to write `Enable=1` under `HKLM\SOFTWARE\WOW6432Node\Razer Chroma SDK` when needed.

Batch install into every installed Chroma / workshop-list game on a Steam library:

```powershell
.\tools\game_wrappers\razer\install_chroma_shim_all.ps1 -SteamRoots "D:\Steam"
# preview first:
.\tools\game_wrappers\razer\install_chroma_shim_all.ps1 -SteamRoots "D:\Steam" -WhatIf
```

Verified on `D:\Steam`: **DOOM** (x64), **DubWars** (x64), **Cat Quest II** (x86), **Cat Quest** (x86).

## Capture log

While a shimmed game runs, the DLL writes a session log:

- `%TEMP%\openrgb_gw_capture_<GameExe>_<pid>.log`
- Pointer: `%TEMP%\openrgb_gw_capture_latest.log`

Env `OPENRGB_GW_CAPTURE`:

- unset / `full` — **default: complete dump** (every grid cell as `grid_raw`, every Create/Set/Delete/Query, param hex, session counters)
- `sampled` — ~10 Hz per device + averages only (if logs get huge)
- `0` — off

`UnInit` and process detach both flush the session summary (CQ2-style UnInit-without-exit is covered).

```powershell
.\tools\game_wrappers\razer\summarize_capture.ps1
.\tools\game_wrappers\razer\summarize_capture.ps1 -LogPath "$env:TEMP\openrgb_gw_capture_DOOMx64.exe_1234.log"
```

## Design

Shim sends **full device grids** (`DeviceGrid`, not colour averages) so animations stay intact.

Chroma create/set (not game-specific):

- `Create*(…, null)` → apply now
- `Create*(…, &id)` → allocate/reuse id, **store + apply** (titles that never `SetEffect` still light up)
- `SetEffect(id)` → re-apply stored frame (animation). UDP is coalesced ~60 Hz so storms do not drop on localhost.

Hardware effects (WAVE / SPECTRUM / …) are logged with param hex dumps and painted as a best-effort colour so the channel stays visible. CUSTOM grids are the real animation path for most games.

**Profiles + multi-destination remap** live in the shared plugin effect UI (not in this folder):

- Six fixed Chroma classes (Keyboard / Mouse / Mousepad / Headset / Keypad / ChromaLink)
- Each channel’s **→ play on** menu is multi-check
- **Game profile** auto-switches with the live exe; Save / Copy / Reset / Template / Ctrl+S
- Default template: **Keyboard → whole room** only (avoids ChromaLink wash burying DOOM fire)

See [`Documentation/GAME_WRAPPERS.md`](../../../Documentation/GAME_WRAPPERS.md).
