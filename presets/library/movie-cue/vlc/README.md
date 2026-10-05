# VLC Movie Cue helper

Sidecar-only workflow (no cue library inside OpenRGB).

## Everyday use

1. Add **Movie Cue** to the effect stack in OpenRGB and start it (Script API on).
2. Keep cues next to the video:
   - `Spooderman2.mp4`
   - `Spooderman2.movie_cue.json`
3. Play the video in VLC (Lua interface installed once — below).

OpenRGB discovers the sidecar through the VLC helper and fires catalog `effect_id`s.

## Install Lua once (Windows)

| File | Copy to |
|------|---------|
| `openrgb_movie_cue_intf.lua` | `%APPDATA%\vlc\lua\intf\openrgb_movie_cue.lua` (**must rename** — VLC loads the stem from `lua-intf`) |

In `%APPDATA%\vlc\vlcrc`:

```text
extraintf=luaintf
lua-intf=openrgb_movie_cue
```

Fully quit and restart VLC.

## Cue file

See `../` for catalog packs (`explosions/`, `pulses/`, `washes/`, `wraps/`).
Cue scripts reference those `effect_id`s by timestamp — they stay beside the movie, never inside OpenRGB.
