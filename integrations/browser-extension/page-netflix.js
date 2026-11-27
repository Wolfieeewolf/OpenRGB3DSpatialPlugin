// Netflix only, inside the page: Netflix's own player knows the exact episode ID and position.
// Hands them to content.js. Reads only; nothing leaves the page from here.
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
  setInterval(() => {
    /* Only while watching (the home page's trailers have IDs too). */
    if (!location.pathname.startsWith("/watch/")) return;
    const player = safe(() => {
      const vp = window.netflix.appContext.state.playerApp.getAPI().videoPlayer;
      const ids = vp.getAllPlayerSessionIds() || [];
      let fallback = null;
      for (const id of ids) {
        const p = vp.getVideoPlayerBySessionId(id);
        if (!p || !p.getMovieId()) continue;
        /* The session that is playing wins (after auto-next the old episode can linger). */
        if (safe(() => p.isPlaying())) return p;
        fallback = fallback || p;
      }
      return fallback;
    });
    if (!player) return;
    window.postMessage(
      {
        __orgbMovieSync: "netflix",
        id: String(safe(() => player.getMovieId()) || ""),
        positionMs: safe(() => player.getCurrentTime()),
        durationMs: safe(() => player.getDuration()),
        sampledAt: Date.now(),
      },
      "*"
    );
  }, 200);
})();
