# Deep dive: 3D Spatial vs Visual Map and Effects plugins

This document compares **OpenRGB3DSpatialPlugin** with **OpenRGBVisualMapPlugin** and **OpenRGBEffectsPlugin** across architecture, startup, profiles/persistence, UI patterns, and robustness. It highlights differences and suggests improvements we could adopt.

---

## 1. Architecture overview

| Aspect | Effects | Visual Map | 3D Spatial |
|--------|---------|------------|------------|
| **Main widget** | `OpenRGBEffectTab` (single tab) | `OpenRGBVisualMapTab` (tab bar of maps) | `OpenRGB3DSpatialTab` (split: layout / viewport + effect stack) |
| **Effect/list model** | One tab per effect; `EffectManager` singleton (effect↔zones, threads) | One tab per virtual map; no effect engine | Stack of `EffectInstance3D`; single timer drives all; no per-effect threads |
| **Registration** | `EffectListManager` singleton; categorized effects; search widget | N/A | `EffectListManager3D` singleton; categorized; library list + search |
| **Settings storage** | `OpenRGBEffectSettings` static helpers; global struct | `VisualMapSettingsManager` static helpers | Path getters + direct JSON in tab; plugin settings (camera, grid) via `GetPluginSettings` |

**Takeaway:** We already use a clear split (layout vs effects) and a single render timer. Effects uses a dedicated EffectManager and per-effect threads; we use one timer and one render path. Both are valid; our approach is simpler and avoids thread sync.

---

## 2. Startup and initialization

| Aspect | Effects | Visual Map | 3D Spatial |
|--------|---------|------------|------------|
| **Wait for devices** | `WaitForDeviceDetection()` in `GetWidget()` | No explicit wait in snippet | `WaitForDeviceDetection()` in `GetWidget()` ✓ |
| **Deferred load** | `QTimer::singleShot(startup_timeout, …)` then `LoadProfileList()` + optional `LoadProfile(startup_profile)` | `SearchAndAutoLoad()` in ctor: scan maps, load any with `grid_settings.auto_load` | Stack/presets loaded after layout exists; `TryAutoLoadLayout()` / `TryAutoLoadEffectProfile()` use `first_load` / timer |
| **Device list refresh** | `RegisterDeviceListChangeCallback` **and** `RegisterDetectionProgressCallback`; in callback, **clear** list and only **re-init when `GetDetectionPercent() == 100`** | Not shown (no device list in same way) | Only `RegisterDeviceListChangeCallback`; no detection-percent check |
| **Global settings at Load()** | `OpenRGBEffectSettings::LoadGlobalSettings()` in plugin `Load()` | None in `Load()` | None in `Load()` (we load stack in tab when layout is ready) |

**What we could do better:**

1. **Register detection progress and gate device list**  
   Effects registers `RegisterDetectionProgressCallback` and in the callback clears the device list and only calls `InitControllersList()` when `GetDetectionPercent() >= 100`. We could do the same: register a detection progress callback and only run `UpdateDeviceList()` (or our equivalent refresh) when detection is complete, avoiding a half-filled controller list during startup.

2. **Optional startup delay**  
   Effects uses `startup_timeout` (e.g. 2000 ms) so “all virtual devices are ready” before loading profile list and startup profile. We could add a small delay before loading effect stack / auto-load profile if we ever see races with other plugins (e.g. Visual Map) that register virtual controllers.

---

## 3. Profiles and persistence (summary)

(Detailed comparison remains in `Profile_and_Persistence_Comparison.md`.)

| Aspect | Effects | Visual Map | 3D Spatial |
|--------|---------|------------|------------|
| **Clear before load** | `StopAll(); ClearAll();` | `LoadJson` clears controller | `LoadStackEffectControls(nullptr); effect_stack.clear();` ✓ |
| **Version check on load** | Abort if version missing or mismatch | No | Profile has version; **no strict abort** yet |
| **Save dialog** | Dedicated `SaveProfilePopup`: name + “load at startup” + “save effects state”; **filename validator** `^[\w\-_.]+$` | Simple `QInputDialog::getText` for filename | `QInputDialog::getText` for name; overwrite confirm ✓; **no filename validator** |
| **Per-item save guard** | N/A | N/A | Try/catch per effect `ToJson()` ✓ |
| **Logging** | printf | printf | LogManager ✓ |

**What we could do better:**

3. **Strict version check when loading effect profiles**  
   Reject load if profile `version` is missing or newer than supported (and optionally support a range of known versions), with a user-visible message. Prevents half-loaded or incompatible state.

4. **Profile name validation**  
   Effects restricts profile filenames to `[\w\-_.]+` via `QRegularExpressionValidator`. We could add the same (or similar) so we never write invalid filenames or paths (e.g. slashes, reserved names).

---

## 4. UI patterns and features

| Feature | Effects | Visual Map | 3D Spatial |
|--------|---------|------------|------------|
| **Tray menu** | Full menu: “Start all”, “Stop all”, submenu of profile names (load via `QMetaObject::invokeMethod(..., "LoadProfile", Qt::QueuedConnection)`); menu refreshed on `ProfileListUpdated` | Not shown | **Returns `nullptr`** (no tray menu) |
| **“Open folder”** | Not in snippet | “Open VMaps folder” → `QDesktopServices::openUrl(MapsFolder())` | No “Open EffectProfiles folder” / “Open plugin folder” |
| **Save profile UX** | Dialog with existing profile list or new name; “Load at startup” and “Save effects state” checkboxes | Simple name dialog | Name dialog + overwrite confirm; auto-load stored separately in config |
| **i18n** | `SetLanguage()`; follows main window language combo; `QTranslator`; retranslate UI and effect list menus | Not in snippet | Not implemented |
| **Lights Off integration** | Connects to main window `ActionLightsOff` → `OnStopEffects()` | Not in snippet | Not implemented |
| **Plugin info / About** | `PluginInfoAction()` → dialog with plugin info | “About” → dialog with `PluginInfo` | Not in snippet (could add) |

**What we could do better:**

5. **Tray menu**  
   Provide a non-null tray menu (e.g. “Start all” / “Stop all” for effect stack, and optionally “Load profile” submenu with current profile list). Use `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` so actions run on the tab in a safe thread context, like Effects.

6. **“Open plugin config folder”**  
   Add an action (e.g. under a menu or in a settings/about area) that opens the plugin config directory (e.g. `GetConfigurationDirectory() / "plugins" / "settings" / "OpenRGB3DSpatialPlugin"`) in the system file manager via `QDesktopServices::openUrl`. Helps power users find and backup profiles/stack.

7. **Optional: Lights Off**  
   If the main window exposes an “Lights Off” (or similar) action, connect it to stop our effect stack so global “lights off” also stops 3D Spatial effects.

8. **Layout and viewport flow**
   Our plugin splits "Effects / Presets" (no viewport) and "Setup / Grid" (viewport) into two top-level tabs, so users must switch tabs to see the 3D view while editing effects. Visual Map keeps the grid in the center of a single tab; Effects uses West tabs and one main view. A dedicated **UI layout design** is in `docs/UI_Layout_Design.md`: single layout with viewport always in the center, Run vs Setup as a left-panel mode, and right panel for effect config or setup settings—so the viewport is available for both setup and effect editing without tab switching.

---

## 5. Error handling and resilience

| Area | Effects | Visual Map | 3D Spatial |
|------|---------|------------|------------|
| **Load profile** | Version check; per-effect try/catch; printf on error | try/catch in `SearchAndAutoLoad`; printf | Try/catch in auto-load; LOG_ERROR; **no strict version abort** |
| **File I/O** | printf on write/read error | printf on write/read error | LOG_ERROR; try/catch around save serialize ✓ |
| **Zone/controller matching** | Detailed match (name, serial, vendor, location, zone_idx, segment); HID location special case | name, vendor, serial, location, zone_idx; HID special case | N/A (we don’t match controllers from profile to current list in same way) |

We already do better on logging (LogManager) and defensive save (try/catch per effect). Adding a version check on profile load would bring us in line with Effects’ load safety.

---

## 6. Code organization and reuse

| Aspect | Effects | Visual Map | 3D Spatial |
|--------|---------|------------|------------|
| **Settings / paths** | Central `OpenRGBEffectSettings` (paths, create dirs, read/write, list) | Central `VisualMapSettingsManager` (paths, create dirs, read/write, list) | Path getters in tab (GetEffectStackPath, GetStackPresetsPath, GetEffectProfilePath); create_directories in getters; save/load in tab / split files |
| **JSON serialization** | Effect `ToJson()`; zone `to_json`; manual field read in load | `VisualMapJsonDefinitions.h`: `to_json` / `from_json` for zones, grid, controller | EffectInstance3D `ToJson`/`FromJson`; manual profile JSON build/parse |
| **List/profile refresh** | `LoadProfileList()`; `ProfileListUpdated` signal used by tray menu | N/A | `PopulateEffectProfileDropdown()`; `UpdateEffectStackList()`; no signal for “profile list changed” |

**Takeaway:** We could optionally introduce a small “SettingsManager” or “Paths” helper that centralizes plugin base path, effect stack path, stack presets path, and effect profile path (and directory creation), so all persistence code goes through one place. Not required for correctness but can reduce duplication and make path changes easier.

---

## 7. Recommendations summary

**High value, low risk**

- **Strict version check** when loading effect profiles (abort + message if version missing or unsupported).
- **Profile name validation** for save (e.g. `[\w\-_.]+` or similar) so we never write invalid filenames.
- **Register detection progress** and only refresh device list (or equivalent) when `GetDetectionPercent() == 100`, to avoid half-filled lists at startup.

**Nice to have**

- **Tray menu**: Start all / Stop all, and optionally load profile submenu (with QueuedConnection invocations).
- **“Open plugin config folder”** action using `QDesktopServices::openUrl`.
- **Optional startup delay** before loading stack/auto-load profile if we see races with other plugins in practice.

**Optional / later**

- Centralize config paths and directory creation in a small helper (no functional change).
- i18n and “Lights Off” integration if we want parity with Effects in those areas.
- Emit a “profile list updated” (or “stack list updated”) signal if we add a tray or external UI that needs to refresh.

---

## 8. What we’re already doing well

- **Namespaced config path** under `OpenRGB3DSpatialPlugin` (clearer than shared `plugins/settings` subfolders).
- **Tear down before load** (`LoadStackEffectControls(nullptr)`, clear stack, then repopulate).
- **Block list signals** when updating selection after add/load so no re-entrant selection handlers.
- **deleteLater()** for effect control teardown to avoid re-entrancy during destroy.
- **LogManager** instead of printf for errors.
- **Try/catch** around each effect’s `ToJson()` in save so one bad effect doesn’t break the whole save.
- **Auto-load** for both layout and effect profile (with config and first_load/timer).
- **Plugin settings** for camera, grid, room, LED spacing, selected layout profile, and auto-load flag so UI state survives close/reopen.

This deep dive shows we’re aligned with the other plugins on the important persistence and lifecycle patterns; the main improvements are version checking, validation, device list timing, and optional UX (tray menu, open folder).
