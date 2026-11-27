# OpenRGB 3D Spatial - Movie Sync browser extension

Plays Movie Sync cue scripts in time with streaming services: Netflix, Prime Video,
Crunchyroll, Disney+, Stan, Binge, YouTube, Paramount+, Max and Apple TV.
Works in **Chrome, Edge and Firefox** on Windows, Linux and macOS, including the Netflix
"app" on Windows (it is an Edge web app, so Edge extensions run inside it).

What it reads: which title is playing (the service's title / episode ID and name) and where
(position, pause, seek, speed, ads). What it never reads: the picture (no DRM is touched).
Where it sends it: only to OpenRGB on this computer (`http://127.0.0.1:27180/v1/stream`).
It only runs on the streaming services above.

On Windows, apps that report to the system media controls (Crunchyroll, Prime Video, ...) are
also followed with no extension at all; the extension adds exact IDs and Netflix titles (the
Netflix app tells Windows nothing about what is playing).

## Install (developer mode)

- **Edge:** `edge://extensions` → Developer mode → Load unpacked → pick this folder.
- **Chrome:** `chrome://extensions` → Developer mode → Load unpacked → pick this folder.
- **Firefox (128+):** run `make-firefox.ps1` (Windows) or `make-firefox.sh` (Linux / macOS,
  needs `python3`), then `about:debugging#/runtime/this-firefox` → Load Temporary Add-on →
  pick `dist/firefox/manifest.json` (temporary until Firefox restarts; signed builds come
  later). Firefox declares the background script differently, so it gets its own small build.

Then in OpenRGB add the **Movie Sync** effect to the stack, keep **Enable Script API** on and
start the stack. The toolbar badge shows green ▶ while OpenRGB is receiving, red ! when it
cannot reach OpenRGB (hover for why). The Movie Sync panel's **Streaming** line shows what is
playing and which cue file matched.

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
  version (sub / dub) its own ID (`GE00384331ENUS`, ...): list every one you want covered.
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

`examples/` has a cue file for testing.
