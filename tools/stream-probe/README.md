# Stream probe (developer tool)

A test-only browser extension that records everything a streaming page exposes: titles, IDs,
the player's own API (Netflix), data caches, API answers the player fetches (Prime skip times,
X-Ray, Crunchyroll CMS), media-session info, artwork, labelled UI text and loaded resources.
Used to find out what a service can give before adding it to the Movie Sync extension
(`integrations/browser-extension`). It sends nothing anywhere.

**Use:** load this folder unpacked (Edge / Chrome: Developer mode → Load unpacked), play
something, press **Alt+Shift+D**: the full report is copied to the clipboard (or saved to
Downloads if the clipboard is blocked). The corner label shows a summary while a video plays.

Windows media controls have their own probe: `scripts/probe-media-session.ps1`.
