// Runs inside the page itself (MAIN world): reads what only the site's own scripts can see
// (its player objects) and hands it to probe.js. Read-only; nothing leaves the page.
(() => {
  if (window.__orgbPageProbe) return;
  window.__orgbPageProbe = true;

  const safe = (fn) => {
    try {
      return fn();
    } catch (e) {
      return undefined;
    }
  };

  const netflix = () =>
    safe(() => {
      const api = window.netflix && window.netflix.appContext && window.netflix.appContext.state &&
        window.netflix.appContext.state.playerApp && window.netflix.appContext.state.playerApp.getAPI();
      if (!api || !api.videoPlayer) return null;
      const vp = api.videoPlayer;
      const ids = safe(() => vp.getAllPlayerSessionIds()) || [];
      return ids.map((id) => {
        const p = safe(() => vp.getVideoPlayerBySessionId(id));
        if (!p) return { session: id };
        return {
          session: id,
          movieId: safe(() => p.getMovieId()),
          currentTimeMs: safe(() => p.getCurrentTime()),
          durationMs: safe(() => p.getDuration()),
          paused: safe(() => p.isPaused()),
          playing: safe(() => p.isPlaying()),
          ended: safe(() => p.isEnded()),
          volume: safe(() => p.getVolume()),
          audioTrack: safe(() => JSON.stringify(p.getAudioTrack())),
          textTrack: safe(() => JSON.stringify(p.getTimedTextTrack())),
          segment: safe(() => p.getCurrentSegment && p.getCurrentSegment()),
        };
      });
    });

  // Names of page globals that look like players / media / ads (names only).
  const playerGlobals = () =>
    safe(() =>
      Object.keys(window)
        .filter((k) => /player|video|media|stream|playback|ads?$|^ad|ima|shaka|hls|dash|bitmovin|jw|brightcove|conviva|nielsen|comscore/i.test(k))
        .slice(0, 80)
    ) || [];

  const send = () => {
    window.postMessage(
      { __orgbProbe: true, netflix: netflix(), globals: playerGlobals(), at: Date.now() },
      "*"
    );
  };
  setInterval(send, 1000);
  send();
})();
