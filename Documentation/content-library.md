# Content folders map

The plugin is the **engine** (C++ under `engines/` / `shared/`). Plugin data root and the [presets repo](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets) use the **same folder names** (`effects/volume/`, …) — copy-paste install into AppData. **No looks ship in the DLL**; those paths are not source-tree folders.

```text
%AppData%\OpenRGB\plugins\settings\OpenRGB3DSpatialPlugin/
```

Each feature owns its tree.

## Where does X go?

| I want… | Put it in | UI |
|---------|-----------|-----|
| Device layout JSON | `controllers/<category>/<brand>/` | Object Creator → Custom Controller |
| Soft 3D field `.fs` | `effects/volume/` | Effect library → Volume |
| FFT / beat `.fs` | `effects/audio/` | Audio |
| Image / GIF `.fs` | `effects/media/` | Media |
| Shadertoy-like `.fs` | `effects/shader-field/` | Shader Field |
| Strip `.kernel` | `effects/patterns/` | Patterns |
| Action Editor Tools | `action-editor/toolbox/` | Action Editor → Tools |
| Editor swatches / curves | `action-editor/{gradients,colors,curves}.json` | Action Editor (create-on-save) |
| Trigger actions | `triggers/effects/` or `triggers/effects/<category>/` | Object Creator → Actions (Category filter) |
| When → play | `triggers/triggers.json` | Object Creator → Triggers |
| Film looks | `movie-sync/effects/<category>/` | Movie Sync |
| Movie Sync catalog / VLC | `movie-sync/catalog.json`, `movie-sync/vlc/` | Movie Sync |
| Scene meshes | `meshes/` | Scene props |

## Tree

```text
…/OpenRGB3DSpatialPlugin/
  controllers/
  effects/
    volume|audio|media|shader-field|patterns/
  action-editor/
    toolbox/
    gradients.json
    colors.json
    curves.json
  triggers/
    effects/                  # root packs + optional <category>/ folders
    triggers.json
  movie-sync/
    catalog.json
    effects/
      explosions|pulses|washes|wraps/
    vlc/
  meshes/
```

## Dump-to-presets audit (2026-10-31)

Whole-plugin scan (**excluding `OpenRGB/`**): **nothing to move.** The C++ tree already has zero stock `.fs` / `.fx` / `.kernel` / action packs / controller JSONs / `effects|controllers|action-editor|triggers|movie-sync|meshes` folders.

| Candidate | Verdict |
|-----------|---------|
| Volume/Audio/Media/Shader `.fs`, patterns, toolbox, triggers, movie-sync looks | Already in [OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets) only |
| `resources/` font + plugin icon | **keep** — host UI chrome, not looks |
| Embedded GLSL in Action/volume/particle/ribbon engines | **keep** — engine runtime, not AppData content |
| ActionBuiltinGpu (`plasma`/`fire`/`snow`/`colorwash`) | **keep** — fail-closed builtins when toolbox missing; toolbox looks stay in presets |
| Minecraft `REGISTER_EFFECT_3D` packs | **keep** — programmed Game engines (not `.fs` content); optional later DLL extract ≠ presets dump |
| `integrations/minecraft` Fabric mod | **keep** with plugin |
| Authoring docs (`shader-conversion`, recipe map) | **keep** in plugin; presets has its own `docs/` catalog/format |

**Rule:** new looks → presets repo → copy into AppData. Never add stock content under this plugin checkout.

## Related

- Engines: [effects-engines.md](effects-engines.md)
- Actions / Triggers: [effect-event-maker.md](effect-event-maker.md)
