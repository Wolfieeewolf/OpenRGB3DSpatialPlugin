# Content folders map

The plugin is the **engine**. Plugin data root and the [presets repo](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets) use the **same folder names** — copy-paste install. **No looks ship in the DLL or this repo’s `presets/`**; download everything from the presets repo.

```text
%AppData%\OpenRGB\plugins\settings\OpenRGB3DSpatialPlugin/
```

Each feature owns its tree.

## Where does X go?

| I want… | Put it in | UI |
|---------|-----------|-----|
| Device layout JSON | `controllers/` | Object Creator → Add from preset |
| Soft 3D field `.fs` | `effects/volume/` | Effect library → Volume |
| FFT / beat `.fs` | `effects/audio/` | Audio |
| Image / GIF `.fs` | `effects/media/` | Media |
| Shadertoy-like `.fs` | `effects/shader-field/` | Shader Field |
| Strip `.kernel` | `effects/patterns/` | Patterns |
| Action Editor Tools | `action-editor/toolbox/` (+ gradients/colors/curves JSON) | Action Editor → Tools |
| Trigger actions | `triggers/effects/*.action.json` | Object Creator → Actions |
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
    effects/
    triggers.json
  movie-sync/
    catalog.json
    effects/
      explosions|pulses|washes|wraps/
    vlc/
  meshes/
```

## Related

- Engines: [effects-engines.md](effects-engines.md)
- Actions / Triggers: [effect-event-maker.md](effect-event-maker.md)
