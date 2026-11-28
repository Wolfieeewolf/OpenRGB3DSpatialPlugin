# Changelog

## Unreleased

### Movie Sync with streaming services
- **Netflix, Prime Video, Crunchyroll, Disney+, Stan, Binge, YouTube, Paramount+, Max and
  Apple TV:** cue scripts play in time with the stream (pause, seek, speed and ads followed).
  A browser extension reports what is playing and where; only timing and IDs are used, the
  picture (DRM) is never touched. Cue files go in `movie-sync/streams/`.
- **Browsers:** Chrome, Edge, Brave, Opera, Vivaldi and Firefox 128+, on Windows, Linux and
  macOS; also the Netflix and Prime Video "apps" on Windows (they run Edge extensions).
- **Without the extension:** Windows media controls and Linux MPRIS are followed by title and
  length.
- **Now Playing panel:** artwork, show, season, episode, rating, genres, advisories and
  synopsis from the service; a cue timeline with the service's intro, recap and end-credits
  marks; next cue; an activity log of everything Movie Sync did; connection lights.
- **Intro, recap and end credits:** pick a look to hold during each (works without a cue
  file).
- **Photosensitive titles** are noted; an optional setting (off by default) blocks
  photosensitive looks while a marked title plays.
- **Lights delay** for TVs and Bluetooth audio that lag (or slow LED devices).

### Light
- One light engine for the Real light overlay and Emitter + relay: light is blocked by
  blockers (ray maps per light), falls off like real light, and **Room exposure** sets the
  overall brightness.
- **Lumens per device:** set how bright each device really is (custom controller editor and
  regular devices), with sensible defaults per device type.

### Fixes
- Sheared room label text in the viewport on some GPUs.
- Blockers on custom controllers (rays now stop inside the box and count the device's own
  housing).
- Linux install script: checks for cmake and gives per-distro package hints.
