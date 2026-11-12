# Logitech G Hub / LED Illumination (stub)

**Status:** not started. Razer Chroma is the live vendor lane (`../razer/`).

When this lane is built, keep everything Logitech-specific here (shim DLL name(s), install scripts, game discovery). Emit the shared `GW01` UDP format via [`../common/`](../common/) so the plugin capture effect stays vendor-agnostic.

Do not put G Hub code under `razer/` or `common/`.
