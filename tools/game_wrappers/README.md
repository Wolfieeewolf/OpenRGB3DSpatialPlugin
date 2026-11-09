# Game wrappers

Retail **SDK proxy DLLs** (Chroma, …) send lighting to the plugin over UDP **9877**. Telemetry stays on **9876**.

## Plugin listener

- `engines/game/Game/GameLightingBridge.cpp` — `127.0.0.1:9877`
- Wire format: `engines/game/Game/GameLightingFormat.h` (GW01, 53-byte uniform RGB)
- Spatial effect: **Game → Game Wrappers → Game wrapper capture**

## Build Chroma shim (Windows)

```powershell
cmake -S tools/game_wrappers/chroma_shim -B tools/game_wrappers/chroma_shim/build -A x64
cmake --build tools/game_wrappers/chroma_shim/build --config Release
```

Use `-A Win32` when the game is 32-bit. Output DLL name is `RazerChromaSDK64.dll` / `RazerChromaSDK.dll`.

## Install next to a game

```powershell
.\tools\game_wrappers\install_chroma_shim.ps1 -GameExePath "D:\Games\SomeGame\game.exe"
```

Then start OpenRGB with the 3D Spatial plugin, add **Game wrapper capture**, and launch the game.

## Design notes

See [`Documentation/GAME_WRAPPERS.md`](../../Documentation/GAME_WRAPPERS.md).
