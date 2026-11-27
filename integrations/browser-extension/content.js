// Movie Sync, in streaming pages and their player frames: finds the main video and reports what
// is playing and where to the background script. Reads timing and titles only, never the picture.
(() => {
  if (window.__orgbMovieSync) return;
  window.__orgbMovieSync = true;

  const api = typeof browser !== "undefined" ? browser : chrome;
  const host = location.hostname;
  const isTop = window === window.top;
  const onHost = (name) => host === name || host.endsWith("." + name);
  const isNetflix = onHost("netflix.com");
  const isPrime = onHost("primevideo.com") || /(^|\.)amazon\.[a-z.]+$/.test(host);
  const clean = (t) => (t || "").replace(/\s+/g, " ").trim();
  const safe = (fn, d) => {
    try {
      const v = fn();
      return v === undefined ? d : v;
    } catch (e) {
      return d;
    }
  };
  /* After the extension is reloaded or updated, this old copy can no longer talk to it. */
  const alive = () => safe(() => !!(api.runtime && api.runtime.id), false);
  const post = (msg) => {
    if (!alive()) return;
    safe(() => {
      const p = api.runtime.sendMessage(msg);
      if (p && p.catch) p.catch(() => {});
    });
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

  /* The feature: the longest video over a minute, then preferring one that is not muted and is
     shown (home-page trailers and previews are short, muted or tiny). */
  const mainVideo = (vids) => {
    let best = null;
    let bestScore = -1;
    for (const v of vids) {
      if (!isFinite(v.duration) || v.duration < 60) continue;
      const shown = v.clientWidth * v.clientHeight > 80000;
      const score = v.duration * 4 + (v.muted ? 0 : 1) * v.duration + (shown ? 1 : 0) * v.duration;
      if (score > bestScore) {
        bestScore = score;
        best = v;
      }
    }
    return best;
  };

  /* Titles that only show with the player controls: remembered once seen, forgotten when the
     title changes (address, Netflix ID, video source or length). */
  let sticky = { title: "", episode: "", key: "" };
  const siteTitle = () => {
    const text = (sel) => {
      const e = document.querySelector(sel);
      return e ? clean(e.textContent) : "";
    };
    if (isNetflix) {
      const block = document.querySelector('[data-uia="video-title"]');
      if (block) {
        /* <h4>Show</h4><span>E6</span><span>Episode 6</span> */
        const show = clean((block.querySelector("h4") || {}).textContent || "");
        const parts = Array.from(block.querySelectorAll("span")).map((s) => clean(s.textContent)).filter(Boolean);
        return { title: show || clean(block.textContent), episode: parts.join(" ") };
      }
      return null;
    }
    if (isPrime) {
      const t = text(".atvwebplayersdk-title-text");
      if (t) return { title: t, episode: text(".atvwebplayersdk-subtitle-text") };
      return null;
    }
    return null;
  };

  /* Netflix's own player (page-netflix.js): exact ID and position. Only believed on Netflix,
     with the right types (any page could post a look-alike message). */
  let netflixPlayer = null;
  if (isNetflix) {
    window.addEventListener("message", (e) => {
      const d = e.data;
      if (e.source !== window || !d || d.__orgbMovieSync !== "netflix") return;
      if (typeof d.id !== "string" || !isFinite(d.positionMs) || !isFinite(d.durationMs)) return;
      netflixPlayer = { id: d.id, positionMs: +d.positionMs, durationMs: +d.durationMs, at: +d.sampledAt || Date.now() };
    });
  }

  /* Prime's permanent title ID, from the playback request it made for this title (watched as
     it happens: the resource timing buffer fills up and stops recording on long sessions). */
  let primeId = "";
  if (isPrime && typeof PerformanceObserver !== "undefined") {
    const takePrime = (name) => {
      if (!/GetVodPlaybackResources|\/playback\//i.test(name)) return;
      const m = name.match(/titleId=(amzn1\.dv\.gti\.[0-9a-f-]+)/i);
      if (m) primeId = m[1];
    };
    safe(() => performance.getEntriesByType("resource").forEach((r) => takePrime(r.name)));
    safe(() => {
      new PerformanceObserver((list) => list.getEntries().forEach((r) => takePrime(r.name))).observe({ type: "resource", buffered: true });
    });
  }

  /* Ads playing inside the feature's own player, where a site marks them. */
  const siteAd = () =>
    (onHost("youtube.com") && !!document.querySelector("#movie_player.ad-showing")) ||
    (isPrime && !!document.querySelector(".atvwebplayersdk-ad-timer, .atvwebplayersdk-adtimeindicator-text"));

  let lastSent = 0;
  let lastKey = "";
  let hadVideo = false;
  let lastPageUrl = "";
  let idleUntil = 0;
  const watched = new WeakSet();

  const tick = (force) => {
    if (!alive()) {
      clearInterval(timer);
      return;
    }
    /* The top page reports its address (a player frame inside it cannot see it). */
    if (isTop && location.href !== lastPageUrl) {
      lastPageUrl = location.href;
      post({ type: "movie-sync-page", url: location.href });
    }
    const now = Date.now();
    if (!force && now < idleUntil) return;

    const vids = allVideos(); /* one walk per tick */
    vids.forEach((v) => {
      if (watched.has(v)) return;
      watched.add(v);
      ["play", "pause", "seeked", "ratechange", "waiting", "playing", "emptied", "ended", "durationchange"].forEach((ev) =>
        v.addEventListener(ev, () => tick(true))
      );
    });
    const v = mainVideo(vids);
    if (!v) {
      if (hadVideo) {
        hadVideo = false;
        post({ type: "movie-sync-gone" });
      }
      idleUntil = now + 2000; /* nothing here: look again in 2 s */
      return;
    }
    hadVideo = true;

    const nf = isNetflix && location.pathname.startsWith("/watch/") && netflixPlayer && now - netflixPlayer.at < 1500
      ? netflixPlayer
      : null;
    const titleKey = [location.href, nf ? nf.id : "", v.currentSrc, Math.round(v.duration)].join("|");
    if (titleKey !== sticky.key) sticky = { title: "", episode: "", key: titleKey };
    const site = siteTitle();
    if (site && site.title) {
      sticky.title = site.title;
      sticky.episode = site.episode || "";
    }

    const md = navigator.mediaSession && navigator.mediaSession.metadata;
    /* Ads: a marked ad in the feature's player, or another video playing while it waits
       (Prime and others swap to a separate ad player). */
    const otherPlaying = vids.some((x) => x !== v && !x.paused && !x.ended && x.readyState >= 2 && x.currentTime > 0);
    const paused = v.paused || v.ended || v.readyState < 3;
    let positionMs = Math.round(v.currentTime * 1000);
    let durationMs = Math.round(v.duration * 1000);
    if (nf) {
      /* Netflix's player clock, brought up to now (it was read up to 200 ms ago). */
      positionMs = nf.positionMs + (paused ? 0 : (now - nf.at) * v.playbackRate);
      durationMs = nf.durationMs;
    }

    const msg = {
      type: "movie-sync-state",
      frameUrl: location.href,
      mediaTitle: md ? clean(md.title) : "",
      mediaArtist: md ? clean(md.artist) : "",
      pageTitle: document.title,
      siteTitle: sticky.title,
      siteEpisode: sticky.episode,
      netflixId: nf ? nf.id : "",
      primeTitleId: isPrime ? primeId : "",
      positionMs,
      durationMs,
      paused,
      muted: v.muted || v.volume === 0,
      rate: v.playbackRate,
      ad: siteAd() || (otherPlaying && v.paused),
      sentAt: now,
    };
    /* On change at once; otherwise 4 times a second while playing, once a second while paused. */
    const key = [msg.netflixId, msg.paused, msg.ad, msg.rate, msg.siteTitle, Math.round(msg.durationMs / 1000)].join("|");
    if (force || key !== lastKey || now - lastSent >= (paused ? 1000 : 250)) {
      lastKey = key;
      lastSent = now;
      post(msg);
    }
  };

  const timer = setInterval(() => tick(false), 250);
  window.addEventListener("pagehide", () => post({ type: "movie-sync-gone" }));
  /* A video starting anywhere wakes the 2 s idle wait at once. */
  document.addEventListener("play", () => tick(true), true);
})();
