# OpenRGB API compliance (next branch)

This doc records checks made so the plugin stays compliant with the OpenRGB submodule API and profile behavior.

**Official OpenRGB docs (in submodule):** `OpenRGB/Documentation/`  
- **OpenRGBSDK.md** – SDK protocol versions, plugin protocol (v4 = plugin interface; v5 = zone flags, segments, ClearSegments/AddSegments).  
- **RGBControllerAPI.md** – **Source of truth** for RGBController: device types, zones, segments, zone flags, and the **Functions** section (GetName, GetLocation, GetZoneName, GetLEDName, etc.). Prefer these getters over direct member access.  
- **Common-Modes.md** – Standardized mode names (Direct, Static, Breathing, etc.) for reference.

## Plugin API version

- **OPENRGB_PLUGIN_API_VERSION** in `OpenRGB/OpenRGBPluginInterface.h` is **4** (OpenRGB 1.0: resizable effects-only zones, zone flags).
- The plugin returns this via `GetPluginAPIVersion()` and does not use APIs removed in v4.

## ResourceManagerInterface usage

The plugin only uses:

- `WaitForDeviceDetection()`, `GetRGBControllers()`, `GetDetectionPercent()`, `GetConfigurationDirectory()`
- `RegisterDeviceListChangeCallback` / `UnregisterDeviceListChangeCallback`
- `RegisterDetectionProgressCallback` / `UnregisterDetectionProgressCallback`

All are present in the current interface. No `RunZoneChecks` / `run_zone_checks` (that is OpenRGB Qt UI settings, not plugin API).

## RGBController: use getters, not direct members

The RGBController API doc (OpenRGB/Documentation/RGBControllerAPI.md) defines the public interface via Functions: GetName(), GetLocation(), GetZoneName(int zone), GetLEDName(int led), etc. OpenRGB also prefers getter functions for controller string fields (see commit “Use getter functions when accessing string fields in RGBControllers”). The plugin was updated to use:

| Instead of           | Use                    |
|----------------------|------------------------|
| `controller->name`   | `controller->GetName()` |
| `controller->location` | `controller->GetLocation()` |
| `controller->zones[i].name` | `controller->GetZoneName((unsigned int)i)` |
| `controller->leds[i].name`  | `controller->GetLEDName(i)` |

**Files updated:**

- `ui/OpenRGB3DSpatialTab_FreqRanges.cpp`: `t->controller->name` → `t->controller->GetName()`
- `ui/CustomControllerDialog.cpp`: `mapping.controller->name` → `GetName()`, zone/led names → `GetZoneName()` / `GetLEDName()`
- `VirtualController3D.cpp`: `c->name` / `c->location` → `c->GetName()` / `c->GetLocation()`

Direct use of `.zones`, `.leds`, `.colors`, and struct fields (e.g. `start_idx`, `leds_count`) is still required where the API does not provide getters; those are unchanged.

## Profile / ProfileManager

- The plugin does **not** call OpenRGB’s `ProfileManager` (LoadProfile, SaveProfile, etc.). It uses its own effect profiles under `plugins/settings/OpenRGB3DSpatialPlugin/EffectProfiles/`.
- OpenRGB profile version (e.g. `OPENRGB_PROFILE_VERSION`) and format changes therefore do not affect plugin logic.

## When updating the OpenRGB submodule

1. Check `OpenRGB/OpenRGBPluginInterface.h` and `OpenRGB/ResourceManagerInterface.h` for new or removed methods; adjust plugin code and, if needed, plugin API version.
2. Review **OpenRGB/Documentation/RGBControllerAPI.md** and **OpenRGBSDK.md** for any API or protocol changes (e.g. new device types, zone/segment/flag additions).
3. Search the plugin (excluding `OpenRGB/`) for any `controller->name`, `controller->location`, or direct zone/led `.name` access and replace with getters if present.
4. Rebuild and run tests.
