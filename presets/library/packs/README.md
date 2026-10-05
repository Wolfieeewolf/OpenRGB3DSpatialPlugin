# Packs (Effect Packs)

User / stock **effect packs** for the Effect Pack maker, Event Bindings, and Movie Cue (**Source → Effect packs**).

```text
library/packs/
  _template.oreffect.json
  rainbow_wash.oreffect.json
  sequence_wipe.oreffect.json
  blocks/                    ← .fx block scripts
```

## Layout template

Hand-authored packs should follow `_template.oreffect.json` section order:

1. Pack identity → 2. Playback → 3. Tracks → block subsections (type, timing, brightness, colour, motion).

Comments (`//`, `/* */`) load fine. In-app save rewrites without comments but keeps field order.

Movie Cue catalog looks use the same layout under `library/movie-cue/`.

## Same file format everywhere

- Extension: `.oreffect.json`
- Format: `openrgb3d.effect_pack`
- Version: **4**

Full field reference: [Documentation/effect-event-maker.md](../../../Documentation/effect-event-maker.md).

## Quick start

1. Copy `_template.oreffect.json` → `my_cool_look.oreffect.json`
2. Set unique `id` (Script API / cue `effect_id`)
3. Set `name`, `duration_ms`, `loop` (`once` | `forever` | `while_active`)
4. Edit block `type` / `effect` (must match a `library/packs/blocks/**/*.fx` stem)
5. Reload in OpenRGB

Library map: [Documentation/content-library.md](../../../Documentation/content-library.md).
