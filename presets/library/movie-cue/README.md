# Movie Cue library

Shared catalog packs for the **Movie Cue** effect (**Source → Movie Cue effects**).

```text
library/movie-cue/
  catalog.json
  _template.oreffect.json
  _example_sidecar.movie_cue.json
  explosions|pulses|washes|wraps/
  vlc/
  docs/
```

## Layout template

Every `.oreffect.json` look uses the same section order — copy `_template.oreffect.json` and keep the comments:

1. **Pack identity** — `format`, `version`, `id`, `name`
2. **Playback** — `duration_ms`, `loop`, `priority`
3. **Tracks** — each track has `name`, `target`, `blocks`
4. Inside each block: type → timing → brightness → colour → motion

## Same rules as Packs

| Rule | Value |
|------|--------|
| Format | `openrgb3d.effect_pack` |
| Version | `4` |
| Extension | `.oreffect.json` |
| Template | `_template.oreffect.json` (same layout as `library/packs/_template.oreffect.json`) |

Full schema: [Documentation/effect-event-maker.md](../../../Documentation/effect-event-maker.md).

Cue timelines (when to fire) are **not** here — they are sidecars next to the video. See [movie-cue-authoring.md](docs/movie-cue-authoring.md).

## Add a look

1. Copy `_template.oreffect.json` into the right category folder.
2. Keep section comments; change values.
3. Update `catalog.json`.
4. Movie Cue → Reload → Test fire.
