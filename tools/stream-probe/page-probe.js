// Runs inside the page itself (MAIN world): reads what only the site's own scripts can see
// (its player objects, its data caches) and hands it to probe.js. Read-only; nothing leaves
// the page.
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
  const cut = (s, n) => (typeof s === "string" && s.length > n ? s.slice(0, n) + "...(" + s.length + ")" : s);
  /* JSON of anything, without cycles, functions or huge strings, and bounded in size. */
  const dump = (o, max = 40000, depthMax = 8) => {
    const seen = new WeakSet();
    const walk = (v, d) => {
      if (v === null || typeof v === "number" || typeof v === "boolean") return v;
      if (typeof v === "string") return cut(v, 600);
      if (typeof v === "function") return "[fn " + (v.name || "") + "/" + v.length + "]";
      if (typeof v !== "object") return String(v);
      if (seen.has(v)) return "[cycle]";
      if (d > depthMax) return "[deep]";
      seen.add(v);
      if (Array.isArray(v)) return v.slice(0, 40).map((x) => walk(x, d + 1));
      const out = {};
      for (const k of Object.keys(v).slice(0, 120)) out[k] = safe(() => walk(v[k], d + 1));
      return out;
    };
    return cut(safe(() => JSON.stringify(walk(o, 0))) || "", max);
  };

  /* Every image link anywhere inside an object (data caches hold box art, posters, stills). */
  const imageUrls = (o, limit = 150) => {
    const out = [];
    const seen = new WeakSet();
    const walk = (v, path, d) => {
      if (out.length >= limit || d > 10 || v === null || v === undefined) return;
      if (typeof v === "string") {
        if (/^https?:\/\/\S+\.(jpe?g|png|webp|avif|gif)(\?|$)/i.test(v) || /^https?:\/\/\S*(image|img|art|poster|thumb)\S*$/i.test(v))
          out.push({ path, url: cut(v, 400) });
        return;
      }
      if (typeof v !== "object" || seen.has(v)) return;
      seen.add(v);
      const keys = Array.isArray(v) ? v.slice(0, 60).map((_, i) => i) : Object.keys(v).slice(0, 200);
      for (const k of keys) safe(() => walk(v[k], path + "/" + k, d + 1));
    };
    walk(o, "", 0);
    return out;
  };

  /* All zero-argument getters of an object (getX, isX, hasX), called and recorded. */
  const getters = (obj) => {
    const out = {};
    if (!obj) return out;
    const names = new Set();
    let p = obj;
    for (let i = 0; p && i < 4; i++, p = Object.getPrototypeOf(p))
      safe(() => Object.getOwnPropertyNames(p).forEach((n) => names.add(n)));
    for (const n of names) {
      if (!/^(get|is|has)[A-Z]/.test(n)) continue;
      const f = safe(() => obj[n]);
      if (typeof f !== "function" || f.length !== 0) continue;
      out[n] = safe(() => {
        const v = f.call(obj);
        return v && typeof v.then === "function" ? "[promise]" : JSON.parse(dump(v, 2000, 3) || "null");
      });
    }
    return out;
  };

  const netflix = () =>
    safe(() => {
      const nf = window.netflix;
      if (!nf) return null;
      const api = safe(() => nf.appContext.state.playerApp.getAPI());
      const vp = api && api.videoPlayer;
      const ids = (vp && safe(() => vp.getAllPlayerSessionIds())) || [];
      const players = ids.map((id) => {
        const p = safe(() => vp.getVideoPlayerBySessionId(id));
        return { session: id, getters: getters(p) };
      });
      const movieId = (players.find((p) => p.getters.isPlaying) || players.find((p) => p.getters.getMovieId) || { getters: {} }).getters.getMovieId;
      const caches = {
        "netflix.falcorCache": safe(() => nf.falcorCache),
        "reactContext.models.falcorCache": safe(() => nf.reactContext.models.falcorCache),
        "appContext.state.model.models.falcorCache": safe(() => nf.appContext.state.model.models.falcorCache),
      };
      const cacheInfo = {};
      for (const [name, c] of Object.entries(caches)) {
        if (!c) continue;
        const videos = c.videos || {};
        const entry = movieId ? videos[movieId] : null;
        cacheInfo[name] = {
          topKeys: Object.keys(c).slice(0, 60),
          videoCount: Object.keys(videos).length,
          entryForMovie: entry ? dump(entry, 30000) : null,
          imagesForMovie: entry ? imageUrls(entry) : [],
        };
      }
      /* The player app's own API: every read-only call, for the title and for each session
         (metadata, artwork, skip-intro / credits timecodes, advisories, runtime ...). */
      const sessions = ids;
      const apiCalls = {};
      if (api) {
        for (const n of Object.keys(api)) {
          if (!/^(get|is|has|can)[A-Z]/.test(n)) continue;
          const f = api[n];
          if (typeof f !== "function") continue;
          const record = (label, args) => {
            apiCalls[label] = safe(() => {
              const v = f.apply(api, args);
              return v && typeof v.then === "function" ? "[promise]" : JSON.parse(dump(v, 8000, 6) || "null");
            });
          };
          if (/VideoId$/.test(n)) {
            if (movieId) record(n + "(" + movieId + ")", [movieId]);
          } else if (/SessionId$/.test(n)) {
            sessions.slice(0, 3).forEach((s) => record(n + "(" + s + ")", [s]));
          } else if (f.length === 0) {
            record(n + "()", []);
          } else if (movieId && /^getTimecode|^getCreditsOffset|^getRuntime|^getBookmark|^getContentMarker/.test(n)) {
            record(n + "(" + movieId + ")", [movieId]);
          }
        }
      }

      /* Other data caches: the path evaluator's and GraphQL's (Apollo), entries for this title. */
      const forMovie = (obj, label) =>
        safe(() => {
          if (!obj || !movieId) return null;
          const hits = {};
          let n = 0;
          for (const k of Object.keys(obj)) {
            if (n >= 25) break;
            if (String(k).includes(String(movieId))) {
              hits[k] = { images: imageUrls(obj[k], 60), json: dump(obj[k], 12000, 6) };
              n++;
            }
          }
          return { label, keyCount: Object.keys(obj).length, hits };
        });
      const st = safe(() => nf.appContext.state) || {};
      const pec = safe(() => st.pathEvaluatorCache);
      const apollo = safe(() => st.graphqlClient.cache.extract()) || safe(() => nf.reactContext.models.graphql.data);
      const otherCaches = {
        pathEvaluatorCache: pec ? { topKeys: Object.keys(pec).slice(0, 60), videos: forMovie(pec.videos, "videos") } : null,
        graphql: apollo ? forMovie(apollo, "apollo") : null,
        graphqlImagesForMovie: apollo && movieId
          ? imageUrls(Object.fromEntries(Object.entries(apollo).filter(([k]) => k.includes(String(movieId)))), 100)
          : [],
      };

      return {
        players,
        movieId,
        sessions,
        caches: cacheInfo,
        otherCaches,
        apiCalls,
        reactContextKeys: safe(() => Object.keys(nf.reactContext.models)),
        appContextKeys: safe(() => Object.keys(nf.appContext.state)),
        playerApiKeys: safe(() => Object.keys(api)),
      };
    });

  /* API answers the player and page fetch for themselves (Prime's playback resources carry
     skip-intro / end-credits times, X-Ray carries scenes and cast, GraphQL carries metadata).
     Copies of JSON answers from metadata-looking addresses, kept in the page, newest 20. */
  const captured = [];
  const WANT = /GetVodPlaybackResources|xray|playerChromeResources|getDetail|detail|metadata|graphql|pathEvaluator|cms|content|episode|series|season|objects|skip|marker|intro|credits|chapters/i;
  const SKIP = /\.(mpd|m3u8|mp4|m4s|ts|vtt|ttml|dfxp|jpg|jpeg|png|webp|avif|js|css|woff2?)(\?|$)|licen[cs]e|drm|telemetry|metric|log|event|clickstream|impression|beacon/i;
  const keep = (url, text) => {
    if (typeof text !== "string" || !text || text.length > 3000000) return;
    const t = text.trim();
    if (t[0] !== "{" && t[0] !== "[") return;
    captured.push({ url: cut(url, 300), at: Date.now(), size: t.length, images: safe(() => imageUrls(JSON.parse(t), 40)) || [], body: cut(t, 60000) });
    if (captured.length > 20) captured.shift();
  };
  const wanted = (url) => typeof url === "string" && WANT.test(url) && !SKIP.test(url);
  safe(() => {
    const origFetch = window.fetch;
    window.fetch = function (input, init) {
      const p = origFetch.apply(this, arguments);
      const url = safe(() => (typeof input === "string" ? input : input.url)) || "";
      if (wanted(url)) p.then((r) => safe(() => r.clone().text().then((t) => keep(url, t)).catch(() => {}))).catch(() => {});
      return p;
    };
  });
  safe(() => {
    const open = XMLHttpRequest.prototype.open;
    XMLHttpRequest.prototype.open = function (method, url) {
      this.__orgbUrl = String(url || "");
      return open.apply(this, arguments);
    };
    const sendX = XMLHttpRequest.prototype.send;
    XMLHttpRequest.prototype.send = function () {
      if (wanted(this.__orgbUrl)) {
        this.addEventListener("load", () => safe(() => {
          if (this.responseType === "" || this.responseType === "text") keep(this.__orgbUrl, this.responseText);
          else if (this.responseType === "json") keep(this.__orgbUrl, JSON.stringify(this.response));
        }));
      }
      return sendX.apply(this, arguments);
    };
  });

  /* Prime / other players: what their media-session helpers and SDK objects hold. */
  const otherPlayers = () => {
    const out = {};
    for (const n of ["__pvMediaSession", "ATVWebPlayerSDK", "WebPlayerSDK", "XpPlayerSDKMsDesktop", "jwplayer", "videojs", "bitmovin", "shaka", "Hls"]) {
      const v = safe(() => window[n]);
      if (v === undefined) continue;
      out[n] = { type: typeof v, keys: safe(() => Object.keys(v).slice(0, 80)), getters: typeof v === "object" ? getters(v) : null, images: imageUrls(v, 30), json: dump(v, 8000, 4) };
    }
    return out;
  };

  /* Page state blobs many sites ship (Next.js, Redux, Apollo, Nuxt ...). */
  const STATE_NAMES = ["__NEXT_DATA__", "__INITIAL_STATE__", "__PRELOADED_STATE__", "__APOLLO_STATE__", "__NUXT__",
    "__REDUX_STATE__", "__INITIAL_DATA__", "__STATE__", "__data", "ytInitialPlayerResponse", "ytInitialData",
    "__staticRouterHydrationData", "__remixContext", "__CR_DATA__", "__APP_STATE__"];
  const states = () => {
    const out = {};
    for (const n of STATE_NAMES) {
      const v = safe(() => window[n]);
      if (v === undefined) continue;
      out[n] = { images: imageUrls(v, 80), json: dump(v, 25000, 6) };
    }
    return out;
  };

  /* Page globals that are not standard browser ones (names only), from a clean frame. */
  let baseline = null;
  const customGlobals = () =>
    safe(() => {
      if (!baseline) {
        const f = document.createElement("iframe");
        f.style.display = "none";
        document.documentElement.appendChild(f);
        baseline = new Set(Object.getOwnPropertyNames(f.contentWindow));
        f.remove();
      }
      return Object.getOwnPropertyNames(window).filter((k) => !baseline.has(k)).slice(0, 250);
    }) || [];

  /* Full (heavy) data only when asked for (Alt+Shift+D); the light part every second. */
  const send = (full) => {
    window.postMessage(
      {
        __orgbProbe: true,
        full,
        netflix: full ? netflix() : safe(() => {
          const vp = window.netflix.appContext.state.playerApp.getAPI().videoPlayer;
          return vp.getAllPlayerSessionIds().map((id) => {
            const p = vp.getVideoPlayerBySessionId(id);
            return { movieId: safe(() => p.getMovieId()), currentTimeMs: safe(() => p.getCurrentTime()), durationMs: safe(() => p.getDuration()) };
          });
        }),
        states: full ? states() : null,
        captured: full ? captured.slice() : null,
        otherPlayers: full ? otherPlayers() : null,
        globals: full ? customGlobals() : null,
        at: Date.now(),
      },
      "*"
    );
  };
  window.addEventListener("message", (e) => {
    if (e.source === window && e.data && e.data.__orgbProbeWantFull) send(true);
  });
  /* The heavy dump every 4 s too, so the copy key can copy at once (a copy that waits for
     it loses the key press and the browser refuses the clipboard). */
  setInterval(() => send(false), 1000);
  setInterval(() => send(true), 8000);
  send(false);
  setTimeout(() => send(true), 1500);
})();
