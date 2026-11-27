# OpenRGB 3D Spatial - Movie Sync browser extension

Plays Movie Sync cue scripts in time with streaming services: Netflix, Prime Video,
Crunchyroll, Disney+, Stan, Binge, YouTube and other sites with a normal web video player.
Works in **Chrome, Edge and Firefox** on Windows, Linux and macOS, including the Netflix and
Prime Video "apps" on Windows (they are Edge web apps).

What it reads: which title is playing (the service's title / episode ID and name) and where
(position, pause, seek, speed, ads). What it never reads: the picture (no DRM is touched).
Where it sends it: only to OpenRGB on this computer (`http://127.0.0.1:27180/v1/stream`).

On Windows, apps that report to the system media controls (Crunchyroll, Prime Video, ...) are
also followed with no extension at all; the extension adds exact IDs and Netflix titles.

## Install (developer mode)

- **Edge:** `edge://extensions` → Developer mode → Load unpacked → pick this folder.
- **Chrome:** `chrome://extensions` → Developer mode → Load unpacked → pick this folder.
- **Firefox (128+):** `about:debugging#/runtime/this-firefox` → Load Temporary Add-on → pick
  `manifest.json` (temporary until Firefox restarts; signed builds come later).

Then in OpenRGB add the **Movie Sync** effect to the stack, keep **Enable Script API** on and
start the stack. The Movie Sync panel's **Streaming** line shows what is playing and which cue
file matched.

## Cue files for streams

Same format as VLC sidecars, saved in `movie-sync/streams/` (any name ending
`.movie_sync.json`), plus a `streams` list naming the services and IDs it is for:

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

- **Matching:** `service` + `id` first (Netflix episode ID, Prime `amzn1.dv.gti...` title ID,
  Crunchyroll / Disney+ / YouTube IDs from the address), else `title` (show + episode) with the
  length within `duration_tolerance_ms`.
- **Timing:** pulses fire as playback passes them (never after a seek, so skipping ahead does
  not set off a backlog); levels hold while playback is inside them; pause, seek, speed and ads
  are followed. `offset_ms` shifts this file; **Lights delay** in the Movie Sync panel shifts all
  of them (TV / Bluetooth / device lag).
- **Services:** `netflix`, `prime`, `crunchyroll`, `disney`, `stan`, `binge`, `youtube`,
  `paramount`, `max`, `appletv`; other sites use their host name.

`examples/` has a cue file for testing.
