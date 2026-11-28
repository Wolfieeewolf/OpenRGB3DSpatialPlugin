# Local API (for integrations)

How companion apps talk to the OpenRGB 3D Spatial plugin on the same computer. Everything here
listens on **127.0.0.1 only**; nothing is reachable from the network.

| Channel | Address | Used by | Plugin side |
|---|---|---|---|
| Movie Sync Script API (HTTP + JSON) | `127.0.0.1:27180` | VLC cue script, Movie Sync browser extension | `engines/movie-sync/MovieSyncListener.cpp` |
| Game telemetry (UDP + JSON) | `127.0.0.1:9876` | Minecraft Fabric mod | `engines/game/Game/GameTelemetryBridge.cpp` |
| Room Ambilight samples (shared memory) | named shared memory | Minecraft Fabric mod | see [SpatialMeasurement.md §5](SpatialMeasurement.md) |

Change a message here and in its sender in the same commit (they live in this repository for
that reason: `integrations/browser-extension`, `integrations/minecraft`).

---

## 1. Movie Sync Script API — `http://127.0.0.1:27180`

Runs while a **Movie Sync** effect is in the stack with **Enable Script API** on (port saved as
`movie_sync_port`, default 27180). One request per connection; answers are JSON.

### Who may call it

Web pages must not be able to drive the lights, so a request is refused when:

- it carries an `Origin` header that is not a browser extension (`chrome-extension://`,
  `moz-extension://`, `extension://`, `safari-web-extension://`): **403**. Local tools send no
  `Origin`;
- it is a `POST` without `Content-Type: application/json`: **415** (a web page cannot send
  that cross-origin without a CORS preflight, which this API never answers);
- its `Host` is not `127.0.0.1`, `localhost` or `[::1]` (DNS rebinding): **403**.

Limits: headers 8 KB, body 64 KB.

### `GET /v1/health`

```json
{ "ok": true, "service": "openrgb3d.movie_sync" }
```

### `GET /v1/catalog`

The looks a cue can name (from `movie-sync/` in the plugin's settings folder):

```json
{ "effects": [ { "id": "explosion", "name": "Explosion", "category": "fire",
                 "source": "movie_sync", "photosensitive": false } ] }
```

### `POST /v1/effect` — one cue edge (VLC)

```json
{ "effect_id": "explosion", "edge": "pulse", "active": true }
```

| Field | | |
|---|---|---|
| `effect_id` | required | a catalog `id` |
| `edge` | `"pulse"` (default) | fire once |
| | `"level"` | hold while `active` is true, release when a later call sends `active: false` |
| `active` | default `true` | |

Answer `{ "ok": true, "effect_id": "..." }` (always, once parsed: problems show in the Movie
Sync panel); `400` when `effect_id` is missing or the body is not JSON.

### `POST /v1/stream` — what a streaming player is doing (browser extension)

Sent about 4 times a second while playing, once a second while paused, and at once on any
change. The plugin follows the newest report; reports older than 5 s count as gone.

```json
{
  "source": "extension",
  "service": "netflix",
  "id": "82760641",
  "title": "Chainsmoker Cat",
  "episode": "E10 Episode 10",
  "position_ms": 52300,
  "duration_ms": 1494117,
  "paused": false,
  "rate": 1,
  "ad": false,
  "artwork_url": "https://...",
  "meta": {
    "show": "Chainsmoker Cat",
    "season": 1, "episode": 10, "episode_title": "Episode 10",
    "synopsis": "...", "rating": "MA 15+", "advisories": "Strong sex scenes, and nudity",
    "genres": ["Comedy"], "photosensitive": false,
    "artwork": ["https://.../story.webp", "https://.../box.webp"],
    "markers": [
      { "kind": "intro",   "start_ms": 52052,   "end_ms": 139973 },
      { "kind": "credits", "start_ms": 1329494, "end_ms": -1 }
    ]
  }
}
```

| Field | Meaning |
|---|---|
| `source` | `"extension"` |
| `service` | `netflix`, `prime`, `crunchyroll`, `disney`, `stan`, `binge`, `youtube`, `paramount`, `max`, `appletv` (else the site's host) |
| `id` | the service's title / episode ID (Netflix episode ID, Prime `amzn1.dv.gti...`, Crunchyroll `GE...ENUS`, ...); `""` if unknown |
| `title`, `episode` | names as the site shows them (used to match a cue file when there is no ID) |
| `position_ms`, `duration_ms` | playback position and length (clamped to 0..48 h) |
| `paused`, `rate`, `ad` | pause, speed (clamped 0.05..16), an ad playing (cues hold) |
| `artwork_url` | an https picture of the title (the page's), optional |
| `stopped` | `true`: nothing plays any more (the extension switched away) |
| `meta` | optional details from the service (all fields optional): |
| `meta.show`, `season`, `episode`, `episode_title` | shown on the Now Playing card |
| `meta.synopsis`, `rating`, `advisories`, `genres` | card and activity log; text is capped (synopsis 700, advisories 300, 8 genres) |
| `meta.photosensitive` | the service marks the title photosensitive (optional auto safety) |
| `meta.artwork` | up to 6 https pictures, best first (tried in order) |
| `meta.markers` | up to 12 marked parts: `kind` = `intro`, `recap`, `credits`, `preview`; `start_ms`; `end_ms` (`-1` = to the end); in stream time |

Answer `{ "ok": true }`. Bad or missing fields are ignored, not refused.

Cue files for streams and how they are matched: [browser extension README](../integrations/browser-extension/README.md#cue-files-for-streams).

---

## 2. Game telemetry — UDP `127.0.0.1:9876`

One JSON object per datagram. Every message has:

```json
{ "version": 1, "type": "player_pose", "timestamp_ms": 1795789115857, "source": "minecraft" }
```

Messages with another `version`, or missing one of those fields, are ignored.

| `type` | Extra fields |
|---|---|
| `player_pose` | `x`, `y`, `z` (position), `fx`, `fy`, `fz` (forward), `ux`, `uy`, `uz` (up), optional `blocks_per_m` |
| `health_state` | `health`, `health_max`, `hp_per_heart`, optional `hearts`, `hearts_max`; `hunger`, `hunger_max`; `air`, `air_max`; `item_durability_valid`, `item_durability`, `item_durability_max` |
| `damage_event` | `amount`, `dir_x`, `dir_y`, `dir_z` (where the hit came from) |
| `room_sample_shm_notify` | a new Room Ambilight frame is in shared memory |

Units, coordinate spaces and the Room Ambilight shared-memory layout:
[SpatialMeasurement.md](SpatialMeasurement.md).

---

## 3. System media (no API)

Without the browser extension, Movie Sync also reads what the system says is playing:
Windows media controls (SMTC) and Linux MPRIS (D-Bus). Nothing to send; see
`engines/movie-sync/MovieSyncSystemMedia.cpp`.

## Tools

`tools/stream-probe/` is a test-only browser extension that records everything a streaming
page exposes (titles, IDs, player APIs, artwork, skip times), to find out what a new service
can give before adding it to the real extension.
