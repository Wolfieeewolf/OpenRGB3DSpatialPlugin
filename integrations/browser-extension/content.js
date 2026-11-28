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
      netflixPlayer = {
        id: d.id,
        positionMs: +d.positionMs,
        durationMs: +d.durationMs,
        meta: d.meta && typeof d.meta === "object" ? d.meta : null,
        at: +d.sampledAt || Date.now(),
      };
    });
  }

  /* Prime's permanent title ID, from the playback request it made for this title (watched as
     it happens: the resource timing buffer fills up and stops recording on long sessions). */
  /* Prime also fetches playback info for the next episode (to start it seamlessly) and for
     the muted trailer behind the player, so the last request is often not what is playing.
     Every request is remembered with its time; the ID is chosen when the main video starts a
     new source: the newest request made before that moment. */
  let primeId = "";
  const primeReqs = [];
  if (isPrime && typeof PerformanceObserver !== "undefined") {
    const takePrime = (r) => {
      if (!/GetVodPlaybackResources|\/playback\//i.test(r.name)) return;
      const m = r.name.match(/titleId=(amzn1\.dv\.gti\.[0-9a-f-]+)/i);
      if (!m) return;
      primeReqs.push({ id: m[1], at: (performance.timeOrigin || 0) + r.startTime });
      if (primeReqs.length > 40) primeReqs.shift();
    };
    safe(() => performance.getEntriesByType("resource").forEach(takePrime));
    safe(() => {
      new PerformanceObserver((list) => list.getEntries().forEach(takePrime)).observe({ type: "resource", buffered: true });
    });
  }
  let primeSrcKey = "";
  let primeSrcAt = 0;
  const pickPrimeId = (v, now) => {
    const key = (v.currentSrc || "") + "|" + Math.round(v.duration || 0);
    if (key !== primeSrcKey) {
      primeSrcKey = key;
      primeSrcAt = now;
      primeId = "";
    }
    if (primeId) return;
    /* The newest request before the source started (noticed up to a tick late). */
    let best = null;
    for (const r of primeReqs) if (r.at <= primeSrcAt + 300 && (!best || r.at > best.at)) best = r;
    primeId = best ? best.id : "";
  };

  /* Artwork for the Now Playing card: what the site gives media controls (largest), else the
     page's share image, else its structured data. Only https image links; looked for again
     every 2 s until one turns up. */
  const httpsUrl = (u) => {
    const s = typeof u === "string" ? u : u && (u.url || u.contentUrl || u.src);
    if (typeof s !== "string" || s.length > 2048) return "";
    const abs = safe(() => new URL(s, location.href).href, "");
    return abs.startsWith("https://") ? abs : "";
  };
  const findArtwork = () => {
    const md = navigator.mediaSession && navigator.mediaSession.metadata;
    const art = safe(() => Array.from((md && md.artwork) || []), []);
    if (art.length) {
      const px = (a) => Math.max(0, ...String(a.sizes || "").split(/\s+/).map((s) => parseInt(s, 10) || 0));
      art.sort((a, b) => px(b) - px(a));
      for (const a of art) {
        const u = httpsUrl(a.src);
        if (u) return u;
      }
    }
    for (const sel of ['meta[property="og:image"]', 'meta[name="og:image"]', 'meta[name="twitter:image"]']) {
      const e = document.querySelector(sel);
      const u = e && httpsUrl(e.getAttribute("content"));
      if (u) return u;
    }
    let found = "";
    safe(() =>
      document.querySelectorAll('script[type="application/ld+json"]').forEach((s) => {
        if (found || s.textContent.length > 200000) return;
        const walk = (o, depth) => {
          if (found || !o || typeof o !== "object" || depth > 4) return;
          for (const k of ["image", "thumbnailUrl"]) {
            const v = Array.isArray(o[k]) ? o[k][0] : o[k];
            const u = httpsUrl(v);
            if (u) {
              found = u;
              return;
            }
          }
          Object.values(o).forEach((x) => walk(x, depth + 1));
        };
        walk(safe(() => JSON.parse(s.textContent), null), 0);
      })
    );
    return found;
  };
  let artwork = { url: "", key: "", at: 0 };

  /* ---------- What the title is: show, season, episode, synopsis, rating, artwork, and the
     intro / recap / end-credits times. Each service keeps these somewhere else. ---------- */

  /* Skip times the players fetched (page-hooks.js on Prime and Crunchyroll), by title ID. */
  const markersById = new Map();
  window.addEventListener("message", (e) => {
    const d = e.data;
    if (e.source !== window || !d || d.__orgbMovieSync !== "markers" || typeof d.id !== "string" || !Array.isArray(d.markers)) return;
    markersById.set(d.id, d.markers);
    if (markersById.size > 30) markersById.delete(markersById.keys().next().value);
  });

  const KINDS = ["intro", "recap", "credits", "preview"];
  const cleanMarkers = (list) =>
    (Array.isArray(list) ? list : [])
      .filter((m) => m && KINDS.includes(m.kind) && isFinite(m.start_ms) && m.start_ms >= 0)
      .slice(0, 12)
      .map((m) => ({ kind: m.kind, start_ms: Math.round(m.start_ms), end_ms: isFinite(m.end_ms) ? Math.round(m.end_ms) : -1 }));
  const s = (v, n) => (typeof v === "string" ? clean(v).slice(0, n) : "");
  const cleanMeta = (m) => {
    if (!m || typeof m !== "object") return null;
    return {
      show: s(m.show, 200),
      season: parseInt(m.season, 10) || 0,
      episode: parseInt(m.episode, 10) || 0,
      episode_title: s(m.episode_title, 200),
      synopsis: s(m.synopsis, 700),
      rating: s(m.rating, 40),
      advisories: s(m.advisories, 300),
      genres: (Array.isArray(m.genres) ? m.genres : []).map((g) => s(g, 40)).filter(Boolean).slice(0, 8),
      photosensitive: !!m.photosensitive,
      artwork: (Array.isArray(m.artwork) ? m.artwork : []).filter((u) => httpsUrl(u)).slice(0, 6),
      markers: cleanMarkers(m.markers),
    };
  };
  const q = (sel) => {
    const e = safe(() => document.querySelector(sel), null);
    return e ? clean(e.textContent) : "";
  };
  const qAll = (sel) => safe(() => Array.from(document.querySelectorAll(sel)).map((e) => clean(e.textContent)).filter(Boolean), []);

  /* Amazon serves AVIF / WebP behind ".jpg" names; a plain size code gives a real JPEG. */
  const amazonJpeg = (u) => {
    if (!httpsUrl(u)) return [];
    const j = u.replace(/\._[^/]+_\.(jpe?g|png|webp|avif)(\?.*)?$/i, "._SX1280_.jpg");
    return j !== u ? [j, u] : [u];
  };
  const primeMeta = () => {
    const meta = { show: sticky.title, synopsis: "", rating: "", genres: [], artwork: [] };
    if (primeId) meta.synopsis = q('[data-testid="synopsis-' + primeId + '"]');
    meta.synopsis = meta.synopsis || q('[data-testid="dp-atf-synopsis"]');
    meta.rating = q('[data-testid="rating-badge"]') || q('[data-testid="maturity-rating"]');
    meta.genres = qAll('[data-testid="genre-texts"]').concat(qAll('[data-testid="mood-texts"]'));
    meta.photosensitive = !!safe(() => document.querySelector('[data-testid="pse-badge"]'), null);
    /* "Season 1, Ep. 3 Ho, Ho, Folks" / "S1 E3 Ho, Ho, Folks" */
    const ep = sticky.episode || "";
    const m = ep.match(/(?:Season|S)\s*(\d+)\D{0,6}?(?:Episode|Ep\.?|E)\s*(\d+)\s*[:.\-–]?\s*(.*)$/i);
    if (m) {
      meta.season = +m[1];
      meta.episode = +m[2];
      meta.episode_title = m[3];
    } else {
      meta.episode_title = ep;
    }
    /* The page's hero picture is labelled with the title. */
    const want = (sticky.title || "").toLowerCase();
    const imgs = safe(() => Array.from(document.querySelectorAll("img")), []).filter((i) => want && (i.alt || "").toLowerCase() === want);
    imgs.sort((a, b) => b.naturalWidth * b.naturalHeight - a.naturalWidth * a.naturalHeight);
    if (imgs[0]) meta.artwork = amazonJpeg(imgs[0].currentSrc || imgs[0].src);
    meta.markers = markersById.get(primeId) || [];
    return meta;
  };

  /* Crunchyroll's player names its episode in the artwork it gives media controls; the page
     address moves to the next episode before the player does. */
  const crunchyPlayerId = () =>
    safe(() => {
      const md = navigator.mediaSession && navigator.mediaSession.metadata;
      const src = (md && md.artwork && md.artwork[0] && md.artwork[0].src) || "";
      const m = src.match(/\/content\/[a-z]\/([A-Z0-9]{8,})/i);
      return m ? m[1] : "";
    }, "");

  const crunchyMeta = () => {
    const md = navigator.mediaSession && navigator.mediaSession.metadata;
    if (!md) return null;
    const meta = { show: s(md.artist, 200), artwork: [], genres: [] };
    const m = (md.title || "").match(/^E(\d+)\s*[-–:]\s*(.*)$/i);
    if (m) {
      meta.episode = +m[1];
      meta.episode_title = m[2];
    } else {
      meta.episode_title = md.title || "";
    }
    const art = safe(() => Array.from(md.artwork || []), []);
    art.forEach((a) => httpsUrl(a.src) && meta.artwork.push(a.src));
    /* The player's episode ID (in its artwork link) and the page's: after auto-next the page
       moves on first, so the page's details only count while both agree. */
    const playerId = ((art[0] && art[0].src) || "").match(/\/content\/[a-z]\/([A-Z0-9]+)/i);
    const urlId = location.pathname.match(/\/watch\/([A-Z0-9]+)/i);
    const pid = playerId ? playerId[1] : "";
    const uid = urlId ? urlId[1] : "";
    if (!pid || pid === uid) {
      const og = safe(() => document.querySelector('meta[property="og:description"]').getAttribute("content"), "");
      meta.synopsis = og;
      safe(() =>
        document.querySelectorAll('script[type="application/ld+json"]').forEach((sc) => {
          const j = safe(() => JSON.parse(sc.textContent), null);
          if (!j || j["@type"] !== "TVEpisode") return;
          meta.season = parseInt(j.partOfSeason && j.partOfSeason.seasonNumber, 10) || 0;
          const img = httpsUrl(j.image);
          if (img) meta.artwork.push(img.replace(/format=auto/, "format=jpeg"));
        })
      );
      meta.rating = q('[data-t$="maturity-rating"]');
    }
    const id = pid || uid;
    meta.markers = markersById.get(id) || markersById.get(id.replace(/[A-Z]{4}$/, "")) || [];
    return meta;
  };

  let metaCache = { key: "", at: 0, meta: null };
  const titleMeta = (titleKey, nf) => {
    if (nf) return cleanMeta(nf.meta);
    const now = Date.now();
    if (metaCache.key !== titleKey) metaCache = { key: titleKey, at: 0, meta: null };
    /* Page details every 2 s (they fill in as the page loads); skip times as they arrive. */
    if (now - metaCache.at > 2000) {
      metaCache.at = now;
      metaCache.meta = isPrime ? safe(primeMeta, null) : onHost("crunchyroll.com") ? safe(crunchyMeta, null) : null;
    } else if (metaCache.meta && isPrime) {
      metaCache.meta.markers = markersById.get(primeId) || metaCache.meta.markers;
    }
    return cleanMeta(metaCache.meta);
  };

  /* Ads playing inside the feature's own player, where a site marks them. */
  const siteAd = () =>
    (onHost("youtube.com") && !!document.querySelector("#movie_player.ad-showing")) ||
    (isPrime && !!document.querySelector(".atvwebplayersdk-ad-timer, .atvwebplayersdk-adtimeindicator-text"));

  let lastSent = 0;
  let lastKey = "";
  let hadVideo = false;
  let lastPageUrl = "";
  let pageArt = { url: "", tries: 0, at: 0 };
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
      pageArt = { url: "", tries: 0, at: 0 };
    }
    /* The top page's artwork, for a player frame inside it that has none of its own
       (Crunchyroll's player is a frame; the show's page has the images). */
    if (isTop && !pageArt.url && pageArt.tries < 10 && Date.now() - pageArt.at > 2000) {
      pageArt.tries++;
      pageArt.at = Date.now();
      pageArt.url = findArtwork();
      if (pageArt.url) post({ type: "movie-sync-page-art", url: pageArt.url });
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
    if (isPrime) pickPrimeId(v, now);

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
    if (artwork.key !== titleKey) artwork = { url: "", key: titleKey, at: 0 };
    if (!artwork.url && now - artwork.at > 2000) {
      artwork.at = now;
      artwork.url = findArtwork();
    }
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
      crunchyId: onHost("crunchyroll.com") ? crunchyPlayerId() : "",
      artwork: artwork.url,
      meta: titleMeta(titleKey, nf),
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
