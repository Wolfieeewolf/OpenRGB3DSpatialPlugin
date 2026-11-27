// Movie Sync background: works out which service and title each tab is playing and sends the
// one that is playing to OpenRGB (Movie Sync effect, http://127.0.0.1:27180/v1/stream).
// Nothing goes anywhere but this computer.
const api = typeof browser !== "undefined" ? browser : chrome;
const DEFAULT_PORT = 27180;
const players = new Map(); // "tabId:frameId" -> { tabId, state, at }
let active = null;
let port = DEFAULT_PORT;

Promise.resolve(api.storage.local.get({ port: DEFAULT_PORT }))
  .then((v) => (port = (v && v.port) || DEFAULT_PORT))
  .catch(() => {});

/* Which service, title ID and name, from the tab's address plus what the page reported. */
function identify(tabUrl, s) {
  let url;
  try {
    url = new URL(tabUrl || s.frameUrl);
  } catch (e) {
    url = new URL(s.frameUrl);
  }
  const host = url.hostname.replace(/^www\./, "");
  const path = url.pathname;
  const out = { service: host, id: "", title: "", episode: "" };
  const match = (re) => (path.match(re) || [])[1] || "";

  if (host.endsWith("netflix.com")) {
    out.service = "netflix";
    out.id = s.netflixId || match(/\/watch\/(\d+)/);
  } else if (host.endsWith("primevideo.com") || /(^|\.)amazon\./.test(host)) {
    out.service = "prime";
    out.id = s.primeTitleId || match(/\/detail\/([A-Z0-9]{8,})/i) || match(/\/dp\/([A-Z0-9]{8,})/i);
  } else if (host.endsWith("crunchyroll.com")) {
    out.service = "crunchyroll";
    out.id = match(/\/watch\/([A-Z0-9]+)/i);
  } else if (host.endsWith("disneyplus.com")) {
    out.service = "disney";
    out.id = match(/\/(?:video|play)\/([0-9a-f-]{8,})/i);
  } else if (host.endsWith("stan.com.au")) {
    out.service = "stan";
    out.id = match(/\/watch\/[^/]*?(\d{5,})/) || match(/\/(\d{5,})/);
  } else if (host.endsWith("binge.com.au")) {
    out.service = "binge";
    out.id = match(/\/(?:play|watch)\/([^/?#]+)/);
  } else if (host.endsWith("youtube.com")) {
    out.service = "youtube";
    out.id = url.searchParams.get("v") || "";
  } else if (host.endsWith("paramountplus.com")) {
    out.service = "paramount";
    out.id = match(/\/video\/([^/?#]+)/);
  } else if (host.endsWith("max.com")) {
    out.service = "max";
    out.id = match(/\/video\/watch\/([^?#]+)/);
  } else if (host.endsWith("tv.apple.com")) {
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
      .replace(/\s*[-|–]\s*(Netflix|Crunchyroll|Disney\+|Stan|BINGE|YouTube|Paramount\+|Max|Apple TV\+?)\s*$/i, "")
      .trim();
    if (/^netflix$/i.test(out.title)) out.title = "";
  }
  return out;
}

/* Toolbar badge: green ▶ = OpenRGB is getting the stream, red ! = OpenRGB not reachable
   (not running, or the Movie Sync effect / Script API is off), none = no video found. */
let badgeState = "";
function badge(state, title) {
  if (state === badgeState) return;
  badgeState = state;
  const a = api.action || api.browserAction;
  if (!a) return;
  const text = state === "ok" ? "▶" : state === "error" ? "!" : "";
  Promise.resolve(a.setBadgeText({ text })).catch(() => {});
  if (a.setBadgeBackgroundColor) {
    Promise.resolve(a.setBadgeBackgroundColor({ color: state === "error" ? "#c0392b" : "#2e9e4f" })).catch(() => {});
  }
  if (a.setTitle) Promise.resolve(a.setTitle({ title: "OpenRGB Movie Sync: " + title })).catch(() => {});
}

async function send(body) {
  try {
    const r = await fetch(`http://127.0.0.1:${port}/v1/stream`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    if (body.stopped) badge("", "no video playing");
    else if (r.ok) badge("ok", "sending " + (body.service || "") + " " + (body.title || body.id || ""));
    else badge("error", "OpenRGB refused (" + r.status + ")");
  } catch (e) {
    /* OpenRGB not running / Movie Sync effect off: try again next report. */
    badge("error", "OpenRGB not reachable on port " + port + " (is the Movie Sync effect running?)");
  }
}

function pickActive() {
  /* The player that is playing (most recently reported) wins; else the last one reporting. */
  let best = null;
  for (const [key, p] of players) {
    if (Date.now() - p.at > 5000) continue;
    const score = (p.state.paused ? 0 : 1e13) + p.at;
    if (!best || score > best.score) best = { key, score };
  }
  return best ? best.key : null;
}

function forget(match) {
  for (const key of Array.from(players.keys())) {
    if (match(key)) players.delete(key);
  }
  if (active && !players.has(active)) {
    active = null;
    send({ source: "extension", stopped: true });
  }
}

api.runtime.onMessage.addListener((msg, sender) => {
  const tabId = sender && sender.tab ? sender.tab.id : -1;
  if (!msg || tabId < 0) return;
  const key = tabId + ":" + (sender.frameId || 0);
  if (msg.type === "movie-sync-gone") {
    forget((k) => k === key);
    return;
  }
  if (msg.type !== "movie-sync-state") return;
  /* The tab's address names the title; a player frame inside it (Crunchyroll) only knows its
     own address, so fall back to the page that embedded it. */
  const who = identify(sender.tab.url || msg.referrer, msg);
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
  };
  players.set(key, { tabId, state, at: Date.now() });
  active = pickActive();
  if (active === key) send(state);
});

api.tabs &&
  api.tabs.onRemoved &&
  api.tabs.onRemoved.addListener((tabId) => forget((k) => k.startsWith(tabId + ":")));
