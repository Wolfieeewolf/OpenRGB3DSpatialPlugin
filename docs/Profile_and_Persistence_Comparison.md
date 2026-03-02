# Profile and persistence: comparison with Visual Map and Effects plugins

This document summarizes how **OpenRGBVisualMapPlugin** and **OpenRGBEffectsPlugin** handle profiles, save/load, and persistence, and how **OpenRGB3DSpatialPlugin** aligns with them and with the referenced docs.

For a **broader comparison** (architecture, startup, UI, tray menu, validation, device detection), see **Plugin_Comparison_Deep_Dive.md**.

---

## 1. Configuration paths

All plugins use `ResourceManagerInterface::GetConfigurationDirectory()` as the base. Path patterns:

| Plugin            | Base path (after config dir)     | Profile / data paths |
|------------------|-----------------------------------|------------------------|
| **Effects**      | `plugins` / `settings`           | `effect-profiles`, `effect-patterns`, `effect-shaders` (no plugin-name folder) |
| **Visual Map**   | `plugins` / `settings`           | `virtual-controllers`, `gradients` (no plugin-name folder) |
| **3D Spatial**   | `plugins` / `settings` / **OpenRGB3DSpatialPlugin** | `effect_stack.json`, `StackPresets/`, `EffectProfiles/` (plugin-name folder) |

**3D Spatial** is more namespaced: everything lives under `OpenRGB3DSpatialPlugin`, which avoids collisions with other plugins. Effects and Visual Map rely on unique subfolder names (`effect-profiles`, `virtual-controllers`). Both approaches are valid; the plugin-name folder is clearer for multi-plugin setups.

---

## 2. Directory creation

- **Effects**: `CreateSettingsDirectory()` and `CreateEffectProfilesDirectory()` (and similar for patterns/shaders) before each save/load. Uses `QDir().mkpath()` in a `create_dir()` helper.
- **Visual Map**: `CreateSettingsDirectory()` and `CreateMapsDirectory()` (or `CreateGradientsDirectory()`) before save/load. Same idea with `QDir().mkpath()`.
- **3D Spatial**: `filesystem::create_directories(plugin_dir)` in path getters (e.g. `GetEffectStackPath()`, `GetStackPresetsPath()`, `GetEffectProfilePath()`). Directories are created when the path is first requested.

All three ensure the target directory exists before writing. 3D Spatial does it at path resolution; the others in explicit “create” helpers. All are correct.

---

## 3. Save/load flow

### OpenRGBEffectsPlugin

- **Save**: `SaveProfileAction()` → dialog with `SaveProfilePopup` → builds `json` with `version`, `Effects` array (each effect’s `ToJson()` + zones + AutoStart) → `OpenRGBEffectSettings::SaveUserProfile(settings, profile_name)`. Optional “load at startup” updates global settings and `WriteGlobalSettings()`.
- **Load**: `LoadProfile(QString)`:
  1. **StopAll(); ClearAll();** (tear down running effects and UI first).
  2. Load JSON with `LoadUserProfile(profile.toStdString())`.
  3. **Version check**: if `!settings.contains("version") || settings["version"] != OpenRGBEffectSettings::version` → abort with message (no partial load).
  4. Iterate `settings["Effects"]`, call `LoadEffect(effect_settings)` in try/catch; one bad effect does not stop the rest.
  5. Set `latest_loaded_profile`.

So: **clear/stop first**, then load; **version guard**; **per-effect try/catch** for resilience.

### OpenRGBVisualMapPlugin

- **Save**: `SaveVmapAction()` → input dialog for filename → build `json` with `ctrl_zones` and `grid_settings` → `VisualMapSettingsManager::SaveMap(filename, j)`.
- **Load**: `LoadVmapAction()` → list from `GetMapNames()`, user picks file → `LoadFile(filename)` which:
  1. `json j = VisualMapSettingsManager::LoadMap(filename)`.
  2. `RenameController(filename)`.
  3. `LoadJson(j)` which **clears** the virtual controller and repopulates from JSON (zone matching by name/vendor/serial/location/zone_idx, with HID location special case). Shows a message box if “some components could not be loaded”.

So: **load replaces state**; no explicit “clear UI then load” in one place, but `LoadJson` clears the controller and rebuilds from JSON. No version field in the snippet seen.

### OpenRGB3DSpatialPlugin (current)

- **Stack persistence**: `SaveEffectStack()` writes `effect_stack.json` (version + effects array). `LoadEffectStack()`:
  - **LoadStackEffectControls(nullptr)** then **effect_stack.clear()** (tear down config UI and clear stack).
  - Parse JSON, repopulate `effect_stack`.
  - Caller (after layout exists) calls `UpdateEffectStackList()` and `setCurrentRow(0)` so the first effect’s controls load.
- **Stack presets**: Tear down with `LoadStackEffectControls(nullptr)`, clear stack, repopulate from preset, then **block list signals**, `UpdateEffectStackList()`, **LoadStackEffectControls(first)**, sync combos, **setCurrentRow(0)**, unblock.
- **Effect profiles**: Same idea: tear down, clear, repopulate from profile JSON, block list, update list, load selected instance’s controls, set row, unblock. Profile JSON has `version`, `stack`, `selected_stack_index`, audio/origin, etc.

So 3D Spatial: **tear down before clear**, **block list and load controls then set row** to avoid re-entrant selection; **version** in profile JSON. Aligns with the “clear then load” and “version check” ideas from Effects.

---

## 4. Error handling and logging

- **Effects**: Uses `printf` for load/save errors and version mismatch (CONTRIBUTING prefers LogManager; we use LOG in 3D Spatial).
- **Visual Map**: Uses `printf` for write/read errors in `VisualMapSettingsManager`.
- **3D Spatial**: Uses `LOG_ERROR` for serialize failures, file open/write, and load exceptions. **Try/catch** in `SaveEffectStack()` around each effect’s `ToJson()` so one bad effect does not break the whole save.

So 3D Spatial is in line with CONTRIBUTING (LogManager, no printf) and has defensive serialization.

---

## 5. Versioning

- **Effects**: `OpenRGBEffectSettings::version` (e.g. 2) stored in profile; load aborts if missing or mismatch.
- **Visual Map**: No version check in the code paths reviewed.
- **3D Spatial**: `effect_stack.json` has `"version": 1`; effect profiles have `"version": 3`. Load paths do not currently abort on version mismatch; they rely on schema and defaulted fields. Adding a strict version check on profile load (like Effects) would make upgrades safer.

---

## 6. Alignment with CONTRIBUTING and other docs

- **CONTRIBUTING**: Style (4 spaces, braces, snake_case/CamelCase), no QDebug/printf/cout, use LogManager. 3D Spatial follows this; Effects/Visual Map use printf in places.
- **OpenRGBSDK.md**: Describes SDK profile packets (save/load/delete profile); no plugin filesystem layout. No conflict with plugin-side persistence.
- Other MCP docs (KernelParameters, RGBControllerAPI, etc.) focus on devices/modes, not plugin storage. No additional persistence requirements.

---

## 7. Recommendations for 3D Spatial

1. **Paths and directories**: Current layout (`plugins/settings/OpenRGB3DSpatialPlugin/` + StackPresets, EffectProfiles, effect_stack.json) is clear and namespaced. No change required.
2. **Tear down before load**: Already done: `LoadStackEffectControls(nullptr)` and `effect_stack.clear()` before repopulating; list blocking and “load controls then set row” avoid re-entrancy. Matches the “ClearAll then load” idea from Effects.
3. **Version check on profile load**: Consider rejecting profile load when `version` is missing or greater than the version the plugin supports (and optionally support a range of known versions). Effects-style abort avoids half-loaded state.
4. **Logging**: Keep using LogManager (LOG_ERROR, etc.) instead of printf for file/serialize errors.
5. **Save errors**: Already guarded with try/catch around each effect’s `ToJson()`; continue returning/logging errors without crashing.

---

## 8. Summary table

| Aspect              | Effects plugin      | Visual Map plugin   | 3D Spatial plugin        |
|---------------------|--------------------|--------------------|---------------------------|
| Config base         | plugins/settings   | plugins/settings   | plugins/settings/OpenRGB3DSpatialPlugin |
| Create dirs         | Before save/load   | Before save/load   | In path getters           |
| Clear before load   | StopAll; ClearAll  | LoadJson clears    | LoadStackEffectControls(nullptr); clear |
| List/UI re-entrancy | N/A (different UI) | N/A                | Block list, load then set row |
| Version in file     | Yes, strict check  | No                 | Yes (no strict abort yet) |
| Per-item save guard | N/A                | N/A                | Try/catch per ToJson      |
| Logging             | printf             | printf             | LogManager                |

Overall, 3D Spatial’s profile and persistence design is consistent with the other two plugins and with CONTRIBUTING; the main optional improvement is a strict version check when loading effect profiles.
