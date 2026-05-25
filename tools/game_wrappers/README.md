# Game wrappers (shims)

Proxy DLLs that games load instead of official middleware SDKs. Each shim forwards lighting to the plugin on **UDP `127.0.0.1:9877`** using **`GW01`** frames (`Game/GameLightingFormat.h`).

Telemetry mods use **9876** — see `docs/GAME.md`.

## v0 packet (uniform RGB)

All devices same color (typical Razer Chroma). C struct:

```c
#pragma pack(push, 1)
struct UniformRgbFrame {
    char magic[4];        /* "GW01" */
    uint32_t sequence;
    uint64_t timestamp_ms;
    uint8_t frame_type;   /* 0 = uniform */
    char source[24];      /* e.g. "chroma_shim" */
    float r, g, b;        /* 0..1 */
};
#pragma pack(pop)
```

## Install (per game)

1. Build the shim for the game's architecture (usually **32-bit** for older titles).
2. Copy the proxy DLL beside the game `.exe` with the name the game imports (e.g. `RazerChromaSDK64.dll` — verify with Dependencies / `dumpbin /imports`).
3. Run the game; the plugin must be loaded in OpenRGB (listener starts on plugin load).

## Layout

```
tools/game_wrappers/
  README.md
  chroma_shim/     (planned — first spike)
  lightfx_shim/    (planned)
```

Reference: [Artemis.Plugins.Wrappers](https://github.com/Artemis-RGB/Artemis.Plugins.Wrappers), `docs/GAME_WRAPPERS.md`.
