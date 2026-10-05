# Authoring movie cue sidecars

Cue timelines live **next to the video**, never inside OpenRGB.

## Files

| File | Role |
|------|------|
| `Spooderman2.mp4` | Your video |
| `Spooderman2.movie_cue.json` | Cue timeline (also accepts `.movie_cues.json`) |

Same folder. Same base name.

## Minimal script

```json
{
  "format": "openrgb3d.movie_cues",
  "version": 1,
  "title": "My Movie",
  "duration_ms": 120000,
  "duration_tolerance_ms": 5000,
  "port": 27180,
  "cues": [
    { "t_ms": 2000, "effect_id": "title_flash", "edge": "pulse" },
    { "t_ms": 15000, "t_end_ms": 45000, "effect_id": "room_wash", "edge": "level" },
    { "t_ms": 60000, "effect_id": "explosion", "edge": "pulse" }
  ]
}
```

## Fields

| Field | Meaning |
|-------|---------|
| `duration_ms` | Length you authored against (ms). VLC warns if the file differs by more than `duration_tolerance_ms`. |
| `port` | OpenRGB Script API port (default `27180`). |
| `cues[].t_ms` | Fire time from start of playback. |
| `cues[].t_end_ms` | Optional end for `level` holds. |
| `cues[].effect_id` | Must exist in the Movie Cue catalog (`library/movie-cue/`). |
| `cues[].edge` | `pulse` (one-shot) or `level` (hold until `t_end_ms` / active false). |

## Effect ids

In **Movie Cue**, use the two filters:

1. **Source** — `Movie Cue effects` (`library/movie-cue/…`) or `Effect packs` (`library/packs/`)
2. **Category** — type folder for Movie Cue effects (`washes`, `explosions`, …)

```text
library/movie-cue/
  explosions/
  pulses/
  washes/
  wraps/
```

Cue JSON `effect_id` can be a Movie Cue catalog id **or** an Effect Pack id. Photosensitive Movie Cue ids (e.g. `title_flash`) need **Allow photosensitive / strobe** checked.

## Effect pack template (looks)

Movie Cue looks and Effect Pack maker packs share one file format (`openrgb3d.effect_pack` v4):

- `presets/library/movie-cue/_template.oreffect.json`
- `presets/library/packs/_template.oreffect.json`

Same section layout (identity → playback → tracks → block type / timing / brightness / colour / motion). Keep the template comments when hand-editing so every pack reads the same way. Comments load in OpenRGB; in-app save drops comments but keeps field order. Schema details: [effect-event-maker.md](../../../../Documentation/effect-event-maker.md).

## Workflow

1. OpenRGB: add **Movie Cue**, Script API on.
2. Author / copy `Movie.movie_cue.json` beside the video.
3. VLC (Lua intf installed): play the video.
4. Use **Test fire** in the catalog to preview a look without VLC.

Template: [`../_example_sidecar.movie_cue.json`](../_example_sidecar.movie_cue.json)
