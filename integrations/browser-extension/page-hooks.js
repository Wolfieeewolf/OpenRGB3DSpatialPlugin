// Prime Video and Crunchyroll only, inside the page from the start: the players fetch their
// own skip-intro / recap / end-credits times; this keeps a copy of just those times (and the
// title they belong to) and hands them to content.js. Reads only; nothing leaves the page.
(() => {
  if (window.__orgbMovieSyncHooks) return;
  window.__orgbMovieSyncHooks = true;

  const safe = (fn) => {
    try {
      return fn();
    } catch (e) {
      return undefined;
    }
  };
  const post = (service, id, markers) => {
    if (!id || !markers || !markers.length) return;
    window.postMessage({ __orgbMovieSync: "markers", service, id: String(id), markers }, "*");
  };
  const ms = (v, scale) => (typeof v === "number" && isFinite(v) && v >= 0 ? Math.round(v * scale) : -1);

  /* Prime: GetVodPlaybackResources -> transitionTimecodes.result.events (milliseconds). */
  const prime = (url, json) => {
    const id = safe(() => new URL(url, location.href).searchParams.get("titleId")) || "";
    const events = safe(() => json.transitionTimecodes.result.events) || [];
    const kinds = { SKIP_INTRO: "intro", SKIP_RECAP: "recap", END_CREDITS: "credits" };
    const out = [];
    for (const e of events) {
      const kind = e && kinds[e.eventType];
      if (!kind) continue;
      const start = ms(e.startTimeMs, 1);
      if (start < 0) continue;
      out.push({ kind, start_ms: start, end_ms: ms(e.endTimeMs, 1) });
    }
    post("prime", id, out);
  };

  /* Crunchyroll: skip-events/<id>.json -> { intro: {start, end}, credits: ..., recap: ... }
     (seconds). */
  const crunchyroll = (url, json) => {
    const id = safe(() => new URL(url, location.href).pathname.split("/").pop().replace(/\.json$/i, "")) || "";
    const out = [];
    for (const kind of ["intro", "recap", "credits", "preview"]) {
      const e = json && json[kind];
      if (!e || typeof e !== "object") continue;
      const start = ms(e.start, 1000);
      if (start < 0) continue;
      out.push({ kind: kind === "preview" ? "preview" : kind, start_ms: start, end_ms: ms(e.end, 1000) });
    }
    post("crunchyroll", id, out);
  };

  const route = (url) => {
    if (/GetVodPlaybackResources/i.test(url)) return prime;
    if (/\/skip-events\//i.test(url)) return crunchyroll;
    return null;
  };
  const handle = (url, text) =>
    safe(() => {
      const fn = route(url);
      if (!fn || typeof text !== "string" || text.length > 4000000) return;
      fn(url, JSON.parse(text));
    });

  safe(() => {
    const origFetch = window.fetch;
    window.fetch = function (input) {
      const p = origFetch.apply(this, arguments);
      const url = safe(() => (typeof Request !== "undefined" && input instanceof Request ? input.url : String(input))) || "";
      if (route(url)) p.then((r) => safe(() => r.clone().text().then((t) => handle(url, t)).catch(() => {}))).catch(() => {});
      return p;
    };
  });
  safe(() => {
    const open = XMLHttpRequest.prototype.open;
    XMLHttpRequest.prototype.open = function (method, url) {
      this.__orgbMsUrl = String(url || "");
      return open.apply(this, arguments);
    };
    const send = XMLHttpRequest.prototype.send;
    XMLHttpRequest.prototype.send = function () {
      if (route(this.__orgbMsUrl) && !this.__orgbMsHooked) {
        this.__orgbMsHooked = true; /* one listener per request object, even when reused */
        this.addEventListener("load", () =>
          safe(() => {
            if (this.responseType === "" || this.responseType === "text") handle(this.__orgbMsUrl, this.responseText);
            else if (this.responseType === "json") handle(this.__orgbMsUrl, JSON.stringify(this.response));
          })
        );
      }
      return send.apply(this, arguments);
    };
  });
})();
