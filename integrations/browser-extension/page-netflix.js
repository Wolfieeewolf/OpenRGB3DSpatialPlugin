// Netflix only, inside the page: Netflix's own player knows the exact episode ID and position,
// and its player app knows the show, season, episode, synopsis, rating, artwork and the
// intro / end-credits times. Hands them to content.js. Reads only; nothing leaves the page.
(() => {
  if (window.__orgbMovieSyncNetflix) return;
  window.__orgbMovieSyncNetflix = true;
  const safe = (fn) => {
    try {
      return fn();
    } catch (e) {
      return undefined;
    }
  };
  const str = (v, n) => (typeof v === "string" ? v.slice(0, n) : typeof v === "number" ? String(v) : "");
  const num = (v) => (typeof v === "number" && isFinite(v) ? v : parseInt(v, 10) || 0);
  /* Netflix's image links are signed (a changed name is refused); OpenRGB decodes the WebP
     with Windows' own codecs. */
  const jpegFirst = (url) => (typeof url === "string" && url.startsWith("https://") ? [url] : []);

  const playerApp = () => safe(() => window.netflix.appContext.state.playerApp.getAPI());

  /* Everything about the title, read once per episode (tried again every second until the
     player app has it). */
  const readMeta = (api, player, rawId) =>
    safe(() => {
      const md = safe(() => api.getVideoMetadataByVideoId(rawId));
      const v = md && md._metadataObject && md._metadataObject.video;
      if (!v) return null;
      const meta = {
        show: str(v.title, 200),
        season: 0,
        episode: 0,
        episode_title: "",
        synopsis: str(v.synopsis, 700),
        rating: str(v.rating, 40),
        advisories: "",
        genres: [],
        artwork: [],
        markers: [],
      };
      let still = "";
      let epMarkers = null;
      for (const s of v.seasons || []) {
        for (const e of s.episodes || []) {
          if (String(e.id) !== String(rawId) && String(e.episodeId) !== String(rawId)) continue;
          meta.season = num(s.seq);
          meta.episode = num(e.seq);
          meta.episode_title = str(e.title, 200);
          if (e.synopsis) meta.synopsis = str(e.synopsis, 700);
          still = safe(() => (e.stills && e.stills[0] && e.stills[0].url) || "") || "";
          epMarkers = e.skipMarkers || null;
        }
      }
      const play = safe(() => api.getPlayableDataByVideoId(rawId).summary);
      if (play) {
        meta.season = meta.season || num(play.season);
        meta.episode = meta.episode || num(play.episode);
      }
      const adv = safe(() => api.getAdvisoriesByVideoId(rawId)) || [];
      for (const a of adv) {
        const d = a && a.data;
        if (!d) continue;
        if (d.rating && !meta.rating) meta.rating = str(d.rating, 40);
        if (d.i18nAdvisories) meta.advisories = str(d.i18nAdvisories, 300);
      }
      /* Artwork: the show's story art (a wide banner), its box art, this episode's still. */
      for (const list of [v.storyart, v.artwork]) {
        const u = safe(() => list && list[0] && list[0].url);
        meta.artwork.push(...jpegFirst(u));
      }
      meta.artwork.push(...jpegFirst(still));
      meta.artwork = meta.artwork.slice(0, 6);

      /* Times: the player's time codes (skip_credits = the "Skip intro" window, ending = the
         end credits), else the episode's skip markers. */
      const tcs = safe(() => player.getTimeCodes()) || [];
      for (const t of tcs) {
        const start = num(t && t.startOffsetMs);
        const end = num(t && t.endOffsetMs);
        if (t.type === "skip_credits" && end > start) meta.markers.push({ kind: "intro", start_ms: start, end_ms: end });
        else if (t.type === "recap" && end > start) meta.markers.push({ kind: "recap", start_ms: start, end_ms: end });
        else if (t.type === "ending" && start > 0) meta.markers.push({ kind: "credits", start_ms: start, end_ms: -1 });
      }
      if (epMarkers) {
        for (const [k, kind] of [["credit", "intro"], ["recap", "recap"]]) {
          const m = epMarkers[k];
          if (m && num(m.end) > num(m.start) && !meta.markers.some((x) => x.kind === kind))
            meta.markers.push({ kind, start_ms: num(m.start), end_ms: num(m.end) });
        }
      }
      /* End credits: the "ending" time code, else where Netflix puts its "Next episode" screen
         (the post-play data), else the credits offset of the title. */
      if (!meta.markers.some((x) => x.kind === "credits")) {
        const post = safe(() => api.getPostPlayExperienceByVideoId(rawId).currentEpisodeSeamlessData);
        const end = num(post && post.seamlessEndMillis) || num(play && play.creditsOffset);
        if (end > 0) meta.markers.push({ kind: "credits", start_ms: end, end_ms: -1 });
      }
      return meta;
    });

  let meta = null;
  let metaFor = "";
  let metaAt = 0;
  setInterval(() => {
    /* Only while watching (the home page's trailers have IDs too). */
    if (!location.pathname.startsWith("/watch/")) return;
    const player = safe(() => {
      const vp = window.netflix.appContext.state.playerApp.getAPI().videoPlayer;
      const ids = vp.getAllPlayerSessionIds() || [];
      let fallback = null;
      let same = null;
      for (const id of ids) {
        const p = vp.getVideoPlayerBySessionId(id);
        if (!p || !p.getMovieId()) continue;
        /* The session that is playing wins (after auto-next the old episode can linger). */
        if (safe(() => p.isPlaying())) return p;
        /* Nothing playing (the "Next episode" screen, pause): stay with the episode we were
           following, not the next one Netflix has already loaded behind it. */
        if (String(p.getMovieId()) === metaFor) same = p;
        fallback = fallback || p;
      }
      return same || fallback;
    });
    if (!player) return;
    const rawId = safe(() => player.getMovieId());
    const id = String(rawId || "");
    if (id !== metaFor) {
      metaFor = id;
      meta = null;
      metaAt = 0;
    }
    /* Read every second until the end credits are known (time codes can arrive after the
       title starts), then every 15 s in case Netflix adds more. */
    const now = Date.now();
    const complete = meta && meta.markers.some((x) => x.kind === "credits");
    if (now - metaAt > (complete ? 15000 : 1000)) {
      metaAt = now;
      meta = readMeta(playerApp(), player, rawId) || meta;
    }
    window.postMessage(
      {
        __orgbMovieSync: "netflix",
        id,
        meta,
        positionMs: safe(() => player.getCurrentTime()),
        durationMs: safe(() => player.getDuration()),
        sampledAt: now,
      },
      "*"
    );
  }, 200);
})();
