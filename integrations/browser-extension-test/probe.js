// Stream probe (test): records everything a streaming page exposes that a Movie Sync
// extension might use, shows a summary in a corner label, and copies the full report to the
// clipboard on Alt+Shift+D. Read-only; nothing leaves the page.
(() => {
  if (window.__orgbStreamProbe) return;
  window.__orgbStreamProbe = true;

  const isTop = window === window.top;
  const started = Date.now();
  const label = document.createElement("div");
  label.style.cssText = [
    "position:fixed", "top:8px", "left:8px", "z-index:2147483647",
    "background:rgba(0,0,0,0.80)", "color:#7CFC9A", "font:12px/1.35 Consolas,monospace",
    "padding:6px 8px", "border-radius:6px", "pointer-events:none", "white-space:pre",
    "max-width:70vw", "max-height:60vh", "overflow:hidden"
  ].join(";");

  const fmt = (s) => {
    if (!isFinite(s)) return String(s);
    const h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), sec = (s % 60).toFixed(1);
    return (h ? h + ":" + String(m).padStart(2, "0") : m) + ":" + String(sec).padStart(4, "0");
  };
  const clean = (t) => (t || "").replace(/\s+/g, " ").trim();
  const safe = (fn, d) => {
    try {
      const v = fn();
      return v === undefined ? d : v;
    } catch (e) {
      return d;
    }
  };

  /* Everything under the document, including open shadow roots (Disney+ etc.). */
  const deepAll = (selector, root = document, out = []) => {
    safe(() => root.querySelectorAll(selector).forEach((e) => out.push(e)));
    safe(() =>
      root.querySelectorAll("*").forEach((e) => {
        if (e.shadowRoot) deepAll(selector, e.shadowRoot, out);
      })
    );
    return out;
  };

  /* Remember short-lived values (titles that only show with the player controls). */
  const seen = {};
  const sticky = (key, value) => {
    const now = Date.now();
    if (value && clean(String(value)) && value !== "-") {
      seen[key] = { value: clean(String(value)), at: now };
      return seen[key].value;
    }
    const s = seen[key];
    return s ? s.value + "   (" + Math.round((now - s.at) / 1000) + "s ago)" : "-";
  };

  /* Player events with timestamps (ads tend to show up as source / length changes). */
  const events = [];
  const watched = new WeakSet();
  const EVENTS = ["play", "playing", "pause", "seeking", "seeked", "waiting", "stalled", "ratechange",
    "durationchange", "loadedmetadata", "emptied", "ended", "error", "enterpictureinpicture",
    "leavepictureinpicture", "resize"];
  const watch = (v, idx) => {
    if (watched.has(v)) return;
    watched.add(v);
    EVENTS.forEach((ev) =>
      v.addEventListener(ev, () => {
        events.push({
          t: ((Date.now() - started) / 1000).toFixed(1) + "s", video: idx, event: ev,
          pos: +v.currentTime.toFixed(2), dur: +(isFinite(v.duration) ? v.duration.toFixed(2) : -1),
          src: (v.currentSrc || "").slice(0, 60)
        });
        if (events.length > 300) events.shift();
      })
    );
  };

  /* What the page's own scripts expose (from page-probe.js). */
  let pageInfo = null;
  window.addEventListener("message", (e) => {
    if (e.source === window && e.data && e.data.__orgbProbe) pageInfo = e.data;
  });

  /* Stream manifests / segments the player loaded (they often carry content IDs). */
  const manifests = () =>
    safe(() =>
      performance.getEntriesByType("resource")
        .map((r) => r.name)
        .filter((n) => /\.(mpd|m3u8|ism|isml)(\?|$)|manifest|playlist|\/playback|licen[cs]e|\/ads?\b|doubleclick|imasdk|freewheel/i.test(n))
        .slice(-25), []);

  const jsonLd = () =>
    safe(() =>
      Array.from(document.querySelectorAll('script[type="application/ld+json"]'))
        .map((s) => safe(() => JSON.parse(s.textContent), null))
        .filter(Boolean)
        .map((j) => {
          const pick = (o) => o && ({ type: o["@type"], name: o.name, partOfSeries: o.partOfSeries && o.partOfSeries.name,
            season: o.partOfSeason && (o.partOfSeason.seasonNumber || o.partOfSeason.name), episode: o.episodeNumber,
            duration: o.duration, id: o["@id"] || o.url });
          return Array.isArray(j) ? j.map(pick) : pick(j);
        }), []);

  const meta = () => {
    const m = {};
    document.querySelectorAll("meta[property], meta[name]").forEach((e) => {
      const k = e.getAttribute("property") || e.getAttribute("name");
      if (/^(og:|twitter:|title|description|video|al:)/i.test(k)) m[k] = clean(e.getAttribute("content")).slice(0, 160);
    });
    return m;
  };

  const videoInfo = (v, i) => ({
    index: i,
    pos: +v.currentTime.toFixed(2),
    dur: isFinite(v.duration) ? +v.duration.toFixed(2) : String(v.duration),
    paused: v.paused, ended: v.ended, muted: v.muted, volume: +v.volume.toFixed(2),
    rate: v.playbackRate, readyState: v.readyState, networkState: v.networkState,
    buffered: safe(() => Array.from({ length: v.buffered.length }, (_, k) => [+v.buffered.start(k).toFixed(1), +v.buffered.end(k).toFixed(1)]), []),
    size: v.videoWidth + "x" + v.videoHeight, shown: Math.round(v.clientWidth) + "x" + Math.round(v.clientHeight),
    src: (v.currentSrc || v.src || "").slice(0, 80),
    textTracks: safe(() => Array.from(v.textTracks).map((t) => t.kind + ":" + t.language + ":" + t.label + ":" + t.mode), []),
    audioTracks: safe(() => Array.from(v.audioTracks || []).map((t) => t.language + ":" + t.label + (t.enabled ? "*" : "")), []),
    quality: safe(() => {
      const q = v.getVideoPlaybackQuality();
      return { frames: q.totalVideoFrames, dropped: q.droppedVideoFrames };
    }, null),
    inShadow: v.getRootNode() !== document,
    aria: clean(v.getAttribute("aria-label") || v.getAttribute("title") || "").slice(0, 80)
  });

  /* Elements that look like ad UI (labels / classes / data attributes with "ad"). */
  const adHints = () =>
    deepAll('[class*="ad-" i], [class*="-ad" i], [class*="advert" i], [id*="advert" i], [data-testid*="ad" i], [aria-label*="advert" i], [aria-label*="ad " i], [class*="ima-" i]')
      .filter((e) => e.offsetParent !== null)
      .slice(0, 12)
      .map((e) => (e.tagName.toLowerCase() + "." + clean(String(e.className)).slice(0, 50) + " '" + clean(e.textContent).slice(0, 40) + "'"));

  /* Per-site title text (best effort; the real extension keeps these small). */
  const siteTitles = () => {
    const t = (sel) => clean(deepAll(sel).map((e) => e.textContent).join(" | ")).slice(0, 160);
    return {
      netflix: t('[data-uia="video-title"]'),
      prime: t(".atvwebplayersdk-title-text, .atvwebplayersdk-subtitle-text"),
      crunchyroll: t('[data-t="show-title-link"], h1.title, .erc-current-media-info h1'),
      disney: t('[data-testid="title-field"], .title-field, .subtitle-field'),
      stan: t('[class*="VideoTitle"], [class*="video-title"], [data-testid*="title"]'),
      binge: t('[class*="Title"] h1, [data-testid*="title"]'),
      youtube: t("h1.ytd-watch-metadata, #title h1")
    };
  };

  const report = () => {
    const vids = deepAll("video");
    vids.forEach(watch);
    const md = navigator.mediaSession && navigator.mediaSession.metadata;
    return {
      frame: isTop ? "top" : "iframe",
      url: location.href,
      path: location.pathname, query: location.search, hash: location.hash,
      ids: (location.href.match(/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}|\d{6,}/gi) || []).slice(0, 6),
      pageTitle: document.title,
      mediaSession: md ? { title: md.title, artist: md.artist, album: md.album,
        artwork: (md.artwork || []).map((a) => a.src).slice(0, 2) } : null,
      mediaPlaybackState: navigator.mediaSession ? navigator.mediaSession.playbackState : null,
      meta: meta(),
      jsonLd: jsonLd(),
      siteTitles: siteTitles(),
      videos: vids.map(videoInfo),
      adHints: adHints(),
      manifests: manifests(),
      page: pageInfo,
      visibility: document.visibilityState, focused: document.hasFocus(),
      fullscreen: !!document.fullscreenElement, pip: !!document.pictureInPictureElement,
      iframes: Array.from(document.querySelectorAll("iframe")).map((f) => (f.src || "").slice(0, 80)).slice(0, 8),
      events: events.slice(-60)
    };
  };

  /* The main video: the longest unmuted one with a length. */
  const mainVideo = (vids) => {
    let best = null;
    vids.forEach((v) => {
      if (isFinite(v.duration) && v.duration > 30 && (!best || v.duration > best.duration)) best = v;
    });
    return best;
  };

  const tick = () => {
    const vids = deepAll("video");
    vids.forEach(watch);
    const v = mainVideo(vids);
    if (!v) {
      label.remove();
      return;
    }
    if (!label.isConnected) (document.body || document.documentElement).appendChild(label);
    const md = navigator.mediaSession && navigator.mediaSession.metadata;
    const titles = siteTitles();
    const siteTitle = Object.values(titles).find((x) => x) || "";
    const nf = pageInfo && pageInfo.netflix && pageInfo.netflix[0];
    const lastEv = events.slice(-3).map((e) => e.t + " " + e.event).join(", ");
    label.textContent = [
      "OpenRGB stream probe v0.2 (test)   Alt+Shift+D = copy full report",
      "site:        " + location.hostname + (isTop ? "" : "  (iframe)"),
      "ids in url:  " + ((location.href.match(/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}|\d{6,}/gi) || []).slice(0, 3).join(", ") || "-"),
      "page title:  " + sticky("page", document.title),
      "media title: " + sticky("media", md ? [md.title, md.artist, md.album].filter(Boolean).join("  |  ") : ""),
      "site title:  " + sticky("site", siteTitle),
      "netflix api: " + (nf ? "movie " + nf.movieId + "  " + fmt((nf.currentTimeMs || 0) / 1000) + " / " + fmt((nf.durationMs || 0) / 1000) : "-"),
      "position:    " + fmt(v.currentTime) + " / " + fmt(v.duration) + (v.paused ? "  (paused)" : "  (playing)") +
        "  rate " + v.playbackRate + (v.readyState < 3 ? "  BUFFERING" : ""),
      "videos:      " + vids.length + (vids.some((x) => x.getRootNode() !== document) ? "  (some in shadow DOM)" : ""),
      "ad hints:    " + (adHints().slice(0, 2).join("; ") || "-"),
      "manifests:   " + manifests().length + " seen",
      "last events: " + (lastEv || "-")
    ].join("\n");
  };

  document.addEventListener("keydown", (e) => {
    if (e.altKey && e.shiftKey && (e.key === "D" || e.key === "d")) {
      const text = JSON.stringify(report(), null, 2);
      navigator.clipboard.writeText(text).then(
        () => { label.textContent = "Full report copied to the clipboard (" + text.length + " characters)."; },
        () => { console.log("[OpenRGB stream probe]", text); label.textContent = "Clipboard blocked: report printed to the console (F12)."; }
      );
    }
  }, true);

  setInterval(tick, 500);
})();
