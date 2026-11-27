// Stream probe (test): shows, in a small corner label, what this streaming page exposes that a
// Movie Sync extension could use: title / episode from several places, and the video player's
// position. Read-only; nothing leaves the page.
(() => {
  if (window.__orgbStreamProbe) return;
  window.__orgbStreamProbe = true;

  const label = document.createElement("div");
  label.style.cssText = [
    "position:fixed", "top:8px", "left:8px", "z-index:2147483647",
    "background:rgba(0,0,0,0.78)", "color:#7CFC9A", "font:12px/1.35 Consolas,monospace",
    "padding:6px 8px", "border-radius:6px", "pointer-events:none", "white-space:pre",
    "max-width:60vw", "overflow:hidden"
  ].join(";");

  const fmt = (s) => {
    if (!isFinite(s)) return "-";
    const h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), sec = (s % 60).toFixed(1);
    return (h ? h + ":" + String(m).padStart(2, "0") : m) + ":" + String(sec).padStart(4, "0");
  };

  const text = (sel) => {
    const el = document.querySelector(sel);
    return el ? el.textContent.trim().replace(/\s+/g, " ") : "";
  };

  // The main video: the longest one that has a duration.
  const mainVideo = () => {
    let best = null;
    document.querySelectorAll("video").forEach((v) => {
      if (isFinite(v.duration) && (!best || v.duration > best.duration)) best = v;
    });
    return best;
  };

  /* Remember the last non-empty value of each line (some appear only for a moment). */
  const seen = {};
  const sticky = (key, value) => {
    const now = Date.now();
    if (value && value !== "-" && value.trim()) {
      seen[key] = { value, at: now };
      return value;
    }
    const s = seen[key];
    return s ? s.value + "   (last seen " + Math.round((now - s.at) / 1000) + "s ago)" : "-";
  };

  const tick = () => {
    const v = mainVideo();
    if (!v) {
      label.remove();
      return;
    }
    if (!label.isConnected) (document.body || document.documentElement).appendChild(label);

    const url = location.href;
    const netflixId = (url.match(/netflix\.com\/watch\/(\d+)/) || [])[1] || "";
    const md = navigator.mediaSession && navigator.mediaSession.metadata;
    const lines = [
      "OpenRGB stream probe (test)",
      "site:        " + location.hostname,
      "url id:      " + (netflixId || url.replace(/^https?:\/\/[^/]+/, "").slice(0, 70)),
      "page title:  " + sticky("page", document.title),
      "media title: " + sticky("media", md ? (md.title || "") + (md.artist ? "  |  artist: " + md.artist : "") +
        (md.album ? "  |  album: " + md.album : "") : ""),
      // Netflix player title block (show + episode), present while the player UI exists.
      "netflix ui:  " + sticky("netflix", text('[data-uia="video-title"]')),
      // Prime / Crunchyroll title elements (best effort).
      "prime ui:    " + sticky("prime", (text(".atvwebplayersdk-title-text") + " " + text(".atvwebplayersdk-subtitle-text")).trim()),
      "crunchy ui:  " + sticky("crunchy", (text('[data-t="show-title-link"]') + " " + text("h1.title")).trim()),
      "position:    " + fmt(v.currentTime) + " / " + fmt(v.duration) + (v.paused ? "  (paused)" : "  (playing)") +
        "  rate " + v.playbackRate
    ];
    label.textContent = lines.join("\n");
  };

  setInterval(tick, 500);
})();
