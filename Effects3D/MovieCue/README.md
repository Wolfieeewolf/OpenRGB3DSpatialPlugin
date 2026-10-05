# Movie Cue

In-plugin engine: Script API + VLC sidecar cues.

## Content

All Movie Cue drop-ins live under the shared content library:

```text
library/movie-cue/
  catalog.json
  explosions|pulses|washes|wraps/*.oreffect.json
  vlc/                 ← Lua helper
  docs/                ← authoring notes
```

Repo stock: [`presets/library/movie-cue/`](../../presets/library/movie-cue/).

Cue timelines stay **beside the video** (`Movie.movie_cue.json`), never inside OpenRGB.

Authoring: [presets/library/movie-cue/docs/movie-cue-authoring.md](../../presets/library/movie-cue/docs/movie-cue-authoring.md).

Library map: [Documentation/content-library.md](../../Documentation/content-library.md).
