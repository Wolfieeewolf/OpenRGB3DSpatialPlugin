// Movie Sync background: works out which service and title each tab is playing and sends the
// one being watched to OpenRGB (Movie Sync effect, http://127.0.0.1:27180/v1/stream).
// Nothing goes anywhere but this computer.
const api = typeof browser !== "undefined" ? browser : chrome;
const PORT = 27180;
const STALE_MS = 4000;
const players = new Map(); // "tabId:frameId" -> { tabId, state, at, audible, muted }
const pages = new Map(); // tabId -> top page address (frames inside cannot see it)
let active = null;

/* ---------- which service, title ID and name ---------- */

const onHost = (host, name) => host === name || host.endsWith("." + name);

function identify(pageUrl, s) {
  let url;
  try {
    url = new URL(pageUrl || s.frameUrl);
  } catch (e) {
    url = new URL(s.frameUrl);
  }
  const host = url.hostname;
  const path = url.pathname;
  const out = { service: host.replace(/^www\./, ""), id: "", title: "", episode: "" };
  const match = (re) => (path.match(re) || [])[1] || "";

  if (onHost(host, "netflix.com")) {
    out.service = "netflix";
    out.id = s.netflixId || match(/\/watch\/(\d+)/);
  } else if (onHost(host, "primevideo.com") || /(^|\.)amazon\.[a-z.]+$/.test(host)) {
    out.service = "prime";
    out.id = s.primeTitleId || match(/\/detail\/([A-Z0-9]{8,})/i) || match(/\/dp\/([A-Z0-9]{8,})/i);
  } else if (onHost(host, "crunchyroll.com")) {
    out.service = "crunchyroll";
    out.id = s.crunchyId || match(/\/watch\/([A-Z0-9]+)/i);
  } else if (onHost(host, "disneyplus.com")) {
    out.service = "disney";
    out.id = match(/\/(?:video|play)\/([0-9a-f-]{8,})/i);
  } else if (onHost(host, "stan.com.au")) {
    out.service = "stan";
    out.id = match(/\/watch\/[^/]*?(\d{5,})/) || match(/\/(\d{5,})/);
  } else if (onHost(host, "binge.com.au")) {
    out.service = "binge";
    out.id = match(/\/(?:play|watch)\/([^/?#]+)/);
  } else if (onHost(host, "youtube.com")) {
    out.service = "youtube";
    out.id = url.searchParams.get("v") || "";
  } else if (onHost(host, "paramountplus.com")) {
    out.service = "paramount";
    out.id = match(/\/video\/([^/?#]+)/);
  } else if (onHost(host, "max.com")) {
    out.service = "max";
    out.id = match(/\/video\/watch\/([^?#]+)/);
  } else if (onHost(host, "tv.apple.com")) {
    out.service = "appletv";
    out.id = match(/\/(umc\.[^/?#]+)/);
  }

  /* Name: the site's own title block, else what the site reports to media controls, else the
     page title with the service's branding taken off. */
  if (s.siteTitle) {
    out.title = s.siteTitle;
    out.episode = s.siteEpisode || "";
  } else if (s.mediaTitle && !/^netflix$/i.test(s.mediaTitle)) {
    if (s.mediaArtist) {
      out.title = s.mediaArtist;
      out.episode = s.mediaTitle;
    } else {
      out.title = s.mediaTitle;
    }
  } else {
    out.title = (s.pageTitle || "")
      .replace(/^Prime Video:\s*/i, "")
      .replace(/^Watch\s+/i, "")
      .replace(/\s*[-|–]\s*(Watch on )?(Netflix|Crunchyroll|Disney\+|Stan|BINGE|YouTube|Paramount\+|Max|Apple TV\+?)\s*$/i, "")
      .trim();
    if (/^netflix$/i.test(out.title)) out.title = "";
  }
  return out;
}

/* ---------- toolbar badge ---------- */

/* Green ▶ = OpenRGB is getting the stream, red ! = OpenRGB not reachable (not running, or the
   Movie Sync effect / Script API is off), none = nothing playing. */
let badgeState = null; /* null: not set since start-up, so the first call always applies */
function badge(state, title) {
  /* Site access not granted yet (Firefox asks separately): that is the only thing to show. */
  if (needsAccess) {
    state = "access";
    title = "click here to allow access to the streaming sites and to OpenRGB";
  }
  if (state === badgeState) return;
  badgeState = state;
  const a = api.action || api.browserAction;
  if (!a) return;
  const text = state === "ok" ? "▶" : state === "error" || state === "access" ? "!" : "";
  Promise.resolve(a.setBadgeText({ text })).catch(() => {});
  if (a.setBadgeBackgroundColor) {
    const color = state === "error" ? "#c0392b" : state === "access" ? "#d68910" : "#2e9e4f";
    Promise.resolve(a.setBadgeBackgroundColor({ color })).catch(() => {});
  }
  if (a.setTitle) Promise.resolve(a.setTitle({ title: "OpenRGB Movie Sync: " + title })).catch(() => {});
}

/* ---------- site access ----------
   Chrome grants the sites in the manifest at install. Firefox (Manifest V3) may not: then
   nothing runs on the streaming sites and OpenRGB cannot be reached until the user allows it.
   The toolbar button asks (the browser only shows the question after a click). */
const NEEDED = (() => {
  const m = api.runtime.getManifest();
  const origins = new Set(m.host_permissions || []);
  for (const cs of m.content_scripts || []) for (const o of cs.matches || []) origins.add(o);
  return { origins: Array.from(origins) };
})();
let needsAccess = false;
function checkAccess() {
  if (!api.permissions || !api.permissions.contains) return;
  Promise.resolve(api.permissions.contains(NEEDED))
    .then((ok) => {
      needsAccess = !ok;
      badgeState = null;
      badge("", "nothing playing");
    })
    .catch(() => {});
}
const actionApi = api.action || api.browserAction;
if (actionApi && actionApi.onClicked) {
  actionApi.onClicked.addListener(() => {
    /* Called straight from the click (no await before it), as the browser requires. */
    if (needsAccess && api.permissions && api.permissions.request) {
      Promise.resolve(api.permissions.request(NEEDED)).then(checkAccess, checkAccess);
    }
  });
}
if (api.permissions && api.permissions.onAdded) api.permissions.onAdded.addListener(checkAccess);
if (api.permissions && api.permissions.onRemoved) api.permissions.onRemoved.addListener(checkAccess);
checkAccess();

/* ---------- sending: one request at a time, newest state wins, back off when OpenRGB is away ---------- */

let inFlight = false;
let pending = null;
let retryAt = 0;
let failures = 0;

async function pump() {
  if (inFlight || !pending || Date.now() < retryAt) return;
  const body = pending;
  pending = null;
  inFlight = true;
  const ctrl = typeof AbortController !== "undefined" ? new AbortController() : null;
  const timeout = setTimeout(() => ctrl && ctrl.abort(), 1000);
  try {
    const r = await fetch(`http://127.0.0.1:${PORT}/v1/stream`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
      signal: ctrl ? ctrl.signal : undefined,
    });
    failures = 0;
    if (body.stopped) badge("", "nothing playing");
    else if (r.ok) badge("ok", "sending " + (body.service || "") + " " + (body.title || body.id || ""));
    else badge("error", "OpenRGB refused (" + r.status + ")");
  } catch (e) {
    failures++;
    retryAt = Date.now() + (failures > 3 ? 5000 : 1000);
    badge("error", "OpenRGB not reachable on port " + PORT + " (is the Movie Sync effect running?)");
    if (!pending) pending = body; /* try the latest again after the back-off */
  } finally {
    clearTimeout(timeout);
    inFlight = false;
    if (pending) setTimeout(pump, Math.max(0, retryAt - Date.now()));
  }
}

function send(body) {
  pending = body;
  pump();
}

/* ---------- which player is being watched ---------- */

function prune() {
  const now = Date.now();
  for (const [key, p] of players) {
    if (now - p.at > STALE_MS) players.delete(key);
  }
}

function pickActive() {
  prune();
  /* Stay with the current player while it is playing: two tabs must not take turns. */
  const cur = active && players.get(active);
  if (cur && !cur.state.paused) return active;
  let best = null;
  for (const [key, p] of players) {
    const score =
      (p.state.paused ? 0 : 8) + (p.audible ? 4 : 0) + (p.muted ? 0 : 2) + (key === active ? 1 : 0);
    if (!best || score > best.score || (score === best.score && p.at > best.at)) best = { key, score, at: p.at };
  }
  return best ? best.key : null;
}

function reselect() {
  const prev = active;
  active = pickActive();
  if (prev && !active) {
    send({ source: "extension", stopped: true });
  }
}

function forget(match) {
  for (const key of Array.from(players.keys())) {
    if (match(key)) players.delete(key);
  }
  reselect();
}

/* Artwork link for the Now Playing card: https only, sane length. */
const pageArt = new Map();
function artUrl(u) {
  return typeof u === "string" && u.length <= 2048 && u.startsWith("https://") ? u : "";
}

api.runtime.onMessage.addListener((msg, sender) => {
  const tabId = sender && sender.tab ? sender.tab.id : -1;
  if (!msg || tabId < 0) return;
  if (msg.type === "movie-sync-page") {
    pages.set(tabId, msg.url);
    pageArt.delete(tabId);
    return;
  }
  if (msg.type === "movie-sync-page-art") {
    if (!sender.frameId && artUrl(msg.url)) pageArt.set(tabId, msg.url);
    return;
  }
  const key = tabId + ":" + (sender.frameId || 0);
  if (msg.type === "movie-sync-gone") {
    forget((k) => k === key);
    return;
  }
  if (msg.type !== "movie-sync-state") return;
  /* The tab's address names the title; a player frame inside it (Crunchyroll) only knows its
     own address, so use the top page's (reported by its own frame). */
  const who = identify(sender.tab.url || pages.get(tabId), msg);
  const state = {
    source: "extension",
    service: who.service,
    id: who.id,
    title: who.title,
    episode: who.episode,
    position_ms: Math.max(0, Math.round(msg.positionMs || 0)),
    duration_ms: Math.max(0, Math.round(msg.durationMs || 0)),
    paused: !!msg.paused,
    rate: msg.rate || 1,
    ad: !!msg.ad,
    artwork_url: artUrl(msg.artwork) || artUrl(pageArt.get(tabId)) || "",
  };
  /* Title details (built and checked by content.js); kept small. */
  if (msg.meta && typeof msg.meta === "object") {
    const text = JSON.stringify(msg.meta);
    if (text.length < 16000) state.meta = msg.meta;
  }
  players.set(key, { tabId, state, at: Date.now(), audible: !!(sender.tab && sender.tab.audible), muted: !!msg.muted });
  reselect();
  if (active === key) send(state);
});

api.tabs &&
  api.tabs.onRemoved &&
  api.tabs.onRemoved.addListener((tabId) => {
    pages.delete(tabId);
    pageArt.delete(tabId);
    forget((k) => k.startsWith(tabId + ":"));
  });

/* Nothing reported for a while (tab frozen, crashed, navigated away): stop, clear the badge.
   Also makes the badge right after the service worker restarts. */
setInterval(() => {
  if (active || players.size) reselect();
  else badge("", "nothing playing");
}, 2000);
