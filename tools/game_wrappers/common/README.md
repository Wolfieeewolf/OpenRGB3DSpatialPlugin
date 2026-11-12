# Shared game-wrapper wire

Vendor-agnostic helpers every shim links:

| File | Role |
|------|------|
| `gw_udp_emit.*` | `GW01` UDP to `127.0.0.1:9877` (must stay in sync with `GameLightingFormat.h`) |
| `gw_capture_log.*` | Optional session capture under `%TEMP%` |

Do **not** put Razer / Logitech / iCUE / amBX SDK types here — those belong in `../razer/`, `../logitech/`, etc.
