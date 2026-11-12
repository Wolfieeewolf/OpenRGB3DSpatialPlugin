# Corsair iCUE SDK (stub)

**Status:** not started. Razer Chroma is the live vendor lane (`../razer/`).

When this lane is built, keep everything iCUE-specific here (shim / proxy, install scripts, game discovery). Emit the shared `GW01` UDP format via [`../common/`](../common/) so the plugin capture effect stays vendor-agnostic.

Do not put iCUE code under `razer/` or `common/`.
