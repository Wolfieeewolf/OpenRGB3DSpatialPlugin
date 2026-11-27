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
    const player = safe(() => {
      const vp = window.netflix.appContext.state.playerApp.getAPI().videoPlayer;
      const ids = vp.getAllPlayerSessionIds() || [];
      for (const id of ids) {
        const p = vp.getVideoPlayerBySessionId(id);
        if (p && p.getMovieId()) return p;
      }
      return null;
    });
    if (!player) return;
    window.postMessage(
      {
        __orgbMovieSync: "netflix",
        id: String(safe(() => player.getMovieId()) || ""),
        positionMs: safe(() => player.getCurrentTime()),
        durationMs: safe(() => player.getDuration()),
        paused: safe(() => player.isPaused()),
      },
      "*"
    );
  }, 250);
})();
