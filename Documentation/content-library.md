# Content library map

All drop-in content lives under **one** root. Folder name = library category.

## Where does X go?

| I want… | Put it in | UI category |
|---------|-----------|-------------|
| Soft 3D field `.fs` | `library/volume/` | Volume |
| FFT / beat `.fs` | `library/audio/` | Audio |
| Image / GIF / texture `.fs` | `library/media/` | Media |
| Shadertoy-like room `.fs` | `library/shader-field/` | Shader Field |
| Strip / kernel `.kernel` | `library/patterns/` | Patterns |
| Effect pack `.oreffect.json` + `.fx` blocks | `library/packs/` (+ `blocks/`) | Packs (Effect Pack maker / bindings) |
| Movie Cue catalog look | `library/movie-cue/<category>/` | Movie Cue |
| VLC Lua helper | `library/movie-cue/vlc/` | (Movie Cue tooling) |
| Screen Mirror notes/assets | `library/ambilight/` | Ambilight |
| Reactive notes/assets | `library/reactive/` | Reactive |
| Minecraft / game notes | `library/game/` | Game |

**Media** = textures/images/GIF into the room — not sound, not VLC movies.  
**Audio** = FFT-driven fields.  
**Movie Cue** = sidecar-driven movie looks.

## Tree

```text
…/OpenRGB3DSpatialPlugin/
  library/
    volume/
    audio/
    media/
    shader-field/
    patterns/
    packs/
      blocks/
    movie-cue/
      explosions|pulses|washes|wraps/
      vlc/
    ambilight/
    reactive/
    game/
  controllers/
  meshes/
  bindings/
```

Repo stock mirrors this under [`presets/library/`](../presets/library/).

## Code vs content

| | |
|--|--|
| **Code** (`Effects3D/`) | C++ engines inside the plugin (Screen Mirror, Movie Cue, …) |
| **Content** (`library/`) | Files you drop in — no rebuild |

## Migration

On plugin load, old folders are moved once if the new folder is empty:

- `effects/spatial` → `library/volume`
- `effects/audio|media|shader-field` → `library/…`
- `patterns` → `library/patterns`
- `timelines` → `library/packs`
- `movie-effects` → `library/movie-cue`

## Related

- Engine behaviour: [effects-engines.md](effects-engines.md)
- Possible future standalone engines: [extract-later.md](extract-later.md)
