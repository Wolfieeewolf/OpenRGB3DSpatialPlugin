# ShowEditor Free Laser Show Pack — format deep dive

Survey of `Laser Show - Showeditor Free Laser Show Pack` (~261 shows, ~15.8k files).
Goal: what is on disk, what we parse, what is worth multi-projector work.

## Inventory (pack-wide)

| Ext | Count | Role | Plugin use |
|-----|------:|------|------------|
| `.heb` | ~13.5k | Figure geometry + colour | **Core** — `HebReader` |
| `.seq` | ~284 | Timeline cues | **Core** — track-aware parser |
| `.ord` (`Key.ord`) | ~266 | ASCII key → `.heb` name | **Core** |
| `.shw` | ~278 | Show name / audio stub | Meta only |
| `.hep` | ~285 | HE-Scan colour palette text | Deferred (RGB444 in HEB is enough for now) |
| `.info` / `[INFO].txt` | ~283+ | Human + binary Showinfo notes | Notes string on load |
| `.mapping` | ~303 | DMX/MIDI→key tables | Mostly zeros — skip |
| `.dmx` | ~238 | Fog/fixture cues | Skip for LED room |
| `.wavdata` / `.bmp` / `.jpg` | many | Audio / preview | Skip |
| `.laser_ini` | few | Editor UI prefs | Skip |
| `.ild` | **0** | — | Not in this pack (standalone ILDA still supported) |

## `.seq` figure cues (decoded)

Two layouts for a lit-figure cue:

```
[f64 LE time_ms][u8  track_code][0x8F][u8 key][0x00][0x01]
[f64 LE time_ms][u16 LE track_code, hi=0][0x8F][u8 key][0x00][0x01]
```

**Spur / track code:** `track_code = 1 + track_index * 19`

| Code | Track |
|------|------:|
| `0x01` | 0 |
| `0x14` | 1 |
| `0x27` | 2 |
| `0x3A` | 3 |
| `0x4D` | 4 |
| … | … through ~11 (`0xD2`) |

This matches ShowEditor multi-output “Spur A/B/C/D” (and extra banks). INFO text that says
“channel A + satellite B” lines up with multi-track timelines (e.g. Womanizer uses tracks 0,1,2,3,4,5,9).

Hold model: each **(key, track)** pair holds until the next cue with the same key on the same track.

Other opcodes (e.g. `0x7F`) appear in large shows — not figure geometry; ignored for now.

## `.heb`

Already reverse-engineered (9-byte points, RGB444, blanking). Static (`frame_n=0`) and animated frames.
Effect trailer after points is ignored (no fog/slider data).

## `.shw`

Text meta. Lines after audio include placeholders “früher projScreenx/y” (unused zeros).
**No Fog / Transparency / Beam / Screen Simulation values** are stored here.

## `.hep`

Text header `Farbpalette fuer HE-Scan_*` plus numeric palette tables. Optional future colour remap;
HEB point colours already carry RGB444.

## Multi-projector room look (done)

Laser Vector maps each Spur/track (and ILDA scanner head) to its own projector origin:

- Stroke media packs `track` in the Z texel (`u_media` col 1, blue).
- With **Multi projector origins** on, active tracks spread left→right on the shared **Projector depth** face; **Projector spread** scales the width (0 = stacked, 1 = full).
- Beam wedges for that stroke throw from that origin to the shared gallery far UV; Graphic stays on the far plane.
- Solo Spur filter still uses that track’s origin. Off = every stroke shares the centre origin.

Supports Spur indices 0..11 (ShowEditor A… plus extra banks). Womanizer-style multi-track timelines read as separate heads instead of one stacked projector.

Loader hygiene: Key.ord HEB names must be folder leaves (no `..` / separators); text sidecars capped at 1 MiB; binary `.seq`/`.heb` stay at the 64 MiB ceiling.

## Worth next (optional)

1. Optional `.hep` palette apply if HEB colours look wrong vs ShowEditor.
2. Non-`0x8F` `.seq` opcodes only if they affect visible beams.
3. Per-head Y/Z offsets or arc layout presets (current default is a straight X line).

## Skip

Simulation fog sliders, DMX fog, MIDI mappings, wav/bmp previews, editor `.laser_ini`.
