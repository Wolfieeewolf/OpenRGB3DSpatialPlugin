// Movie Sync, in every page and frame: finds the main video and reports what is playing and
// where to the background script. Reads timing and titles only, never the picture.
(() => {
  if (window.__orgbMovieSync) return;
  window.__orgbMovieSync = true;

  const api = typeof browser !== "undefined" ? browser : chrome;
  const host = location.hostname;
  const clean = (t) => (t || "").replace(/\s+/g, " ").trim();
  const safe = (fn, d) => {
    try {
      const v = fn();
      return v === undefined ? d : v;
    } catch (e) {
      return d;
    }
  };

  /* All videos, including inside open shadow roots (Disney+ and others hide them there). */
  const allVideos = (root = document, out = []) => {
    safe(() => root.querySelectorAll("video").forEach((v) => out.push(v)));
    safe(() =>
      root.querySelectorAll("*").forEach((e) => {
        if (e.shadowRoot) allVideos(e.shadowRoot, out);
      })
    );
    return out;
  };

  /* The feature: the longest video over a minute, preferring one that is shown and not muted
     (home-page trailers and previews are short, muted or tiny). */
  const mainVideo = (vids) => {
    let best = null;
    let bestScore = -1;
    for (const v of vids) {
      if (!isFinite(v.duration) || v.duration < 60) continue;
      const shown = v.clientWidth * v.clientHeight;
      const score = v.duration + (v.muted ? 0 : 1e6) + (shown > 80000 ? 1e7 : 0);
      if (score > bestScore) {
        bestScore = score;
        best = v;
      }
    }
    return best;
  };

  /* Titles that only show with the player controls: remember them once seen. */
  let stickyTitle = "";
  let stickyEpisode = "";
  const siteTitle = () => {
    const text = (sel) => {
      const e = document.querySelector(sel);
      return e ? clean(e.textContent) : "";
    };
    if (/netflix\.com$/.test(host)) {
      const block = document.querySelector('[data-uia="video-title"]');
      if (block) {
        /* <h4>Show</h4><span>E6</span><span>Episode 6</span> */
        const show = clean((block.querySelector("h4") || {}).textContent || "");
        const parts = Array.from(block.querySelectorAll("span")).map((s) => clean(s.textContent)).filter(Boolean);
        return { title: show || clean(block.textContent), episode: parts.join(" ") };
      }
      return null;
    }
    if (/primevideo\.com$|amazon\./.test(host)) {
      const t = text(".atvwebplayersdk-title-text");
      if (t) return { title: t, episode: text(".atvwebplayersdk-subtitle-text") };
      return null;
    }
    return null;
  };

  /* Netflix's own player (page-netflix.js): exact ID and position. */
  let netflixPlayer = null;
  window.addEventListener("message", (e) => {
    if (e.source === window && e.data && e.data.__orgbMovieSync === "netflix") netflixPlayer = { ...e.data, at: Date.now() };
  });

  /* Prime's permanent title ID, from the playback request it made. */
  const primeTitleId = () =>
    safe(() => {
      const hits = performance
        .getEntriesByType("resource")
        .map((r) => r.name)
        .filter((n) => n.includes("titleId=amzn1.dv.gti."));
      const last = hits[hits.length - 1];
      const m = last && last.match(/titleId=(amzn1\.dv\.gti\.[0-9a-f-]+)/i);
      return m ? m[1] : "";
    }, "");

  let lastSent = 0;
  let lastKey = "";
  let hadVideo = false;

  const report = (force) => {
    const vids = allVideos();
    const v = mainVideo(vids);
    if (!v) {
      if (hadVideo) {
        hadVideo = false;
        safe(() => api.runtime.sendMessage({ type: "movie-sync-gone" }));
      }
      return;
    }
    hadVideo = true;

    const md = navigator.mediaSession && navigator.mediaSession.metadata;
    const site = siteTitle();
    if (site && site.title) {
      stickyTitle = site.title;
      stickyEpisode = site.episode || "";
    }
    /* Ads: another video playing while the feature waits (Prime and others swap players). */
    const otherPlaying = vids.some((x) => x !== v && !x.paused && !x.ended && x.readyState >= 2 && x.currentTime > 0);
    const nf = netflixPlayer && Date.now() - netflixPlayer.at < 1500 ? netflixPlayer : null;

    const msg = {
      type: "movie-sync-state",
      frameUrl: location.href,
      referrer: window === window.top ? "" : document.referrer,
      pageTitle: document.title,
      mediaTitle: md ? clean(md.title) : "",
      mediaArtist: md ? clean(md.artist) : "",
      mediaAlbum: md ? clean(md.album) : "",
      siteTitle: stickyTitle,
      siteEpisode: stickyEpisode,
      netflixId: nf ? nf.id : "",
      primeTitleId: /primevideo\.com$|amazon\./.test(host) ? primeTitleId() : "",
      positionMs: nf && isFinite(nf.positionMs) ? nf.positionMs : Math.round(v.currentTime * 1000),
      durationMs: nf && isFinite(nf.durationMs) ? nf.durationMs : Math.round(v.duration * 1000),
      paused: v.paused || v.ended || v.readyState < 3,
      rate: v.playbackRate,
      ad: otherPlaying && v.paused,
    };
    /* Send on change or every 250 ms while playing (cheap, local only). */
    const key = [msg.netflixId, msg.paused, msg.ad, msg.rate, Math.round(msg.positionMs / 1000)].join("|");
    const now = Date.now();
    if (force || key !== lastKey || now - lastSent >= 250) {
      lastKey = key;
      lastSent = now;
      safe(() => api.runtime.sendMessage(msg));
    }
  };

  /* React at once to play / pause / seek, and poll for everything else. */
  const watched = new WeakSet();
  const hook = () =>
    allVideos().forEach((v) => {
      if (watched.has(v)) return;
      watched.add(v);
      ["play", "pause", "seeked", "ratechange", "waiting", "playing", "emptied", "ended"].forEach((ev) =>
        v.addEventListener(ev, () => report(true))
      );
    });
  setInterval(() => {
    hook();
    report(false);
  }, 250);
})();
