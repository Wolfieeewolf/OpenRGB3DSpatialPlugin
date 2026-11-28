# OpenRGB 3D Spatial - Movie Sync browser extension

Plays Movie Sync cue scripts in time with streaming services: Netflix, Prime Video,
Crunchyroll, Disney+, Stan, Binge, YouTube, Paramount+, Max and Apple TV, and shows what is
playing (artwork, show, season, episode, synopsis, rating) in the Movie Sync panel.

What it reads: which title is playing (the service's title / episode ID, name and details),
where (position, pause, seek, speed, ads) and the parts the service marks (intro, recap, end
credits). What it never reads: the picture (no DRM is touched). Where it sends it: only to
OpenRGB on this computer (`http://127.0.0.1:27180/v1/stream`). It only runs on the streaming
services above.

## Browsers and systems

| | Windows | Linux | macOS |
|---|---|---|---|
| Chrome, Edge, Brave, Opera, Vivaldi | ✅ | ✅ | ✅ |
| Firefox 128+ | ✅ | ✅ | ✅ |
| Netflix / Prime Video "apps" | ✅ (they are Edge web apps: Edge extensions run inside) | – | – |
| Safari | – | – | ❌ (needs an Xcode-built app; not planned) |
| Without the extension | Windows media controls | MPRIS (D-Bus) | – |

**Without the extension:** Windows media controls and Linux MPRIS report what most players
and browsers are playing (title, position, pause), so Movie Sync can follow them by title and
length with nothing installed. The extension adds exact IDs (Netflix's Windows app tells the
system nothing), artwork and details, and the intro / recap / credits marks.

## Build

```
powershell -ExecutionPolicy Bypass -File build.ps1     # Windows
./build.sh                                              # Linux / macOS (needs python3)
```

This makes `dist/chromium/` and `dist/firefox/` (folders to load) and
`dist/movie-sync-chromium-<version>.zip` / `dist/movie-sync-firefox-<version>.zip` (to share
or to upload to the browser stores). Firefox declares the background script differently,
so it gets its own build; everything else is the same.

## Install (developer mode)

- **Edge:** `edge://extensions` → Developer mode → Load unpacked → `dist/chromium` (or this
  folder).
- **Chrome / Brave / Vivaldi:** `chrome://extensions` (`brave://`, `vivaldi://`) → Developer
  mode → Load unpacked → `dist/chromium`.
- **Opera:** `opera://extensions` → Developer mode → Load unpacked → `dist/chromium`.
- **Firefox:** `about:debugging#/runtime/this-firefox` → Load Temporary Add-on →
  `dist/firefox/manifest.json`. Temporary add-ons go when Firefox closes; to keep it, have the
  zip signed at addons.mozilla.org ("On your own" listing: signed, not public), or use Firefox
  Developer Edition / Nightly with `xpinstall.signatures.required` set to false.
  Firefox may not grant the sites at install: if the toolbar button shows an orange **!**,
  click it and allow access (to the streaming sites and to OpenRGB on this computer).

After an update press **Reload** on the extension and reopen the streaming tab or app.

Then in OpenRGB add the **Movie Sync** effect to the stack, keep **Enable Script API** on and
start the stack. The toolbar badge shows green ▶ while OpenRGB is receiving, red ! when it
cannot reach OpenRGB (hover for why). The Movie Sync panel shows what is playing, the cue
timeline and an activity log.

## Intro, recap and end credits

Netflix, Prime Video and Crunchyroll mark the intro, recap and end credits of an episode
(the times behind their "Skip intro" buttons). Movie Sync shows them on the timeline and, in
**Settings**, can hold a look for each (**During the intro / recap / end credits**), with or
without a cue file. Netflix's "Next episode" countdown jumps on after a few seconds: press
**Watch credits** to keep the credits look going.

Titles the service marks photosensitive (Prime Video's badge, advisories that mention
flashing lights) are noted in the activity log; **Block them for titles the service marks
photosensitive** (Settings, off by default) also blocks photosensitive looks while such a
title plays.

## Cue files for streams

Same format as VLC sidecars, saved in `movie-sync/streams/` (any name ending
`.movie_sync.json`; press **Reload** in the Movie Sync panel after adding one), plus a
`streams` list naming the services and IDs it is for:

```json
{
  "format": "openrgb3d.movie_sync",
  "version": 1,
  "title": "Chainsmoker Cat E6 Episode 6",
  "streams": [{ "service": "netflix", "id": "82760637" }],
  "duration_ms": 1500081,
  "duration_tolerance_ms": 3000,
  "offset_ms": 0,
  "cues": [
    { "t_ms": 10000, "effect_id": "explosion", "edge": "pulse" },
    { "t_ms": 30000, "t_end_ms": 60000, "effect_id": "ocean_horizon", "edge": "level" }
  ]
}
```

- **Matching:** `service` + `id` first (with the length within `duration_tolerance_ms`), else
  `title` (show + episode) and length. IDs: Netflix episode ID, Prime `amzn1.dv.gti...` title
  ID, Crunchyroll / Disney+ / YouTube IDs from the address. Crunchyroll gives each audio
  version (sub / dub) its own ID (`GE00384331ENUS`, `GE00384331JAJP`, ...): one of them is
  enough, the others match too when the length fits.
  IDs may be numbers or strings.
- **Timing:** pulses fire as playback passes them (never after a seek, so skipping ahead does
  not set off a backlog); levels hold while playback is inside them (a level with no
  `t_end_ms` holds to the end); pausing the film freezes running looks; ads hold the cues.
- **Offsets:** `offset_ms` shifts this file (positive = earlier, as in VLC sidecars).
  **Lights delay** in the Movie Sync panel shifts every cue: positive = later (your TV or
  Bluetooth makes the picture or sound lag), negative = earlier (slow LED devices).
- **Services:** `netflix`, `prime`, `crunchyroll`, `disney`, `stan`, `binge`, `youtube`,
  `paramount`, `max`, `appletv`.
- Live streams (no fixed length) are not followed.

- **Artwork:** the service's own is shown; put `name.jpg` or `.png` next to
  `name.movie_sync.json` to use your own.

`examples/` has a cue file for testing.
