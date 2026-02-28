# Contributing to OpenRGB 3D Spatial Plugin

Align with **OpenRGB CONTRIBUTING** (style, C-style preferences, no QDebug/printf/cout) and **OpenRGB RGBController API** (device/zone/LED/colors, `0x00BBGGRR`, null and bounds checks). Do not modify files under `OpenRGB/` (upstream submodule).

## Code quality

- No debug code, dead code, or legacy/deprecated APIs.
- Comments: minimal; explain why when non-obvious. Comment and code on separate lines.

## Style

- Follow existing style in the file. 4 spaces; braces on own lines. `snake_case` variables, `CamelCase` types/functions.

## Practices

- DRY, KISS, YAGNI. Single responsibility; prefer commands over "get then if/else" in callers.
- RGBController: guard pointers; use `zones[].start_idx` and zone LED counts; check bounds before indexing.

## UI

- Filter/sort: label "Filter:" and "Sort:" with combos; use `color: gray; font-size: small;` for labels.
- Connections complete; list and viewport selection in sync; clear or disable dependent UI on unselect; null-check viewport and widget pointers.

## Building

- Qt 5.15+ (or 6), OpenRGB submodule init. **Windows**: `qmake OpenRGB3DSpatialPlugin.pro CONFIG+=release` then `nmake`. **Linux**: `qmake ... PREFIX=/usr` then `make -j$(nproc)`.

## Releasing

- **PROJECT_VERSION** in root (one line `YY.MM.DD.V`). Tag format `vYY.MM.DD.V`. **Actions** → **Create release tag** → Run workflow; or **Releases** → Draft new release; or `.\create-release-tag.ps1` then push tag.

## Custom Controller UX

Custom controller creation, editing, import/export, and 3D preview (Phases 1–3) are implemented in the Object Creator and CustomControllerDialog.

## Save/Load and profile

- **OpenRGB has two persistence systems:**
  - **ProfileManager** (main app Save/Load Profile): saves device colors and zone sizes to `.orp` / `.ors` files; does *not* currently include plugin-specific state.
  - **SettingsManager**: JSON keyed by section (e.g. `"3DSpatialPlugin"`). The plugin uses `GetPluginSettings()` / `SetPluginSettings()` / `SetPluginSettingsNoSave()`; `SetPluginSettings()` calls `SaveSettings()` so changes persist. This is separate from the main profile file.
- **Align with upstream:** When implementing or auditing save/load, follow patterns from OpenRGB’s CONTRIBUTING and RGBController API (e.g. `G:\MCP\CONTRIBUTING.md`, `G:\MCP\RGBControllerAPI.md` or `OpenRGB/Documentation/RGBControllerAPI.md`). Use SettingsManager for plugin UI/layout state; do not rely on ProfileManager for plugin data unless OpenRGB adds plugin-in-profile support (e.g. `next_profile_updates` branch).
- **Plugin areas to keep consistent:**
  - **Layout:** `SaveLayout` / `LoadLayout` (file-based profiles in `plugins/settings/OpenRGB3DSpatialPlugin/layouts/`); `SaveCurrentLayoutName` / `TryAutoLoadLayout` (selected profile + auto-load flag in plugin settings).
  - **Custom controllers:** `SaveCustomControllers` / `LoadCustomControllers` (folder `custom_controllers/`); call `SaveCustomControllers()` after add/remove/edit.
  - **Effect stack:** `SaveEffectStack` / `LoadEffectStack` (in layout JSON); effect instance `saved_settings` for per-effect state; call `SaveEffectStack()` when stack or effect settings change.
  - **Effect profiles:** `SaveEffectProfile` / `LoadEffectProfile` (file-based); `SaveCurrentEffectProfileName` / `TryAutoLoadEffectProfile` (selected effect profile in plugin settings).
  - **Stack presets:** `LoadStackPresets` / `SaveStackPresets` (stack preset list; ensure load/save when presets change).
  - **Reference points:** `SaveReferencePoints` / `LoadReferencePoints` (in layout JSON).
  - **Zones:** `SaveZones` / `LoadZones` (in layout JSON).
  - **Camera / Room grid:** Stored in plugin settings (`Camera`, `RoomGrid`); restore on tab show (e.g. from `GetPluginSettings()`).
  - **Display planes / monitor presets:** Loaded from layout or display-plane data; ensure they are part of `SaveLayout` / `LoadLayoutFromJSON` and that monitor preset list is loaded once (e.g. `LoadMonitorPresets()`).
- **Audit checklist:** For each area above, confirm: (1) every user-visible state change that should persist calls the appropriate Save (or triggers a path that does); (2) on load/restore, all dependent UI and viewport state is updated; (3) no duplicate or missing save/load paths; (4) use `SetPluginSettings()` when you need to persist immediately, `SetPluginSettingsNoSave()` when batching updates before one save.
- **Future:** OpenRGB’s `next_profile_updates` (or similar) branch may add saving all plugin settings as part of the main profile; when that lands, the plugin may need to hook into that so “Save Profile” also captures our state.

## Code audit

When touching code, keep DRY, KISS, null safety, layout, and clear/unstick in mind: selection and viewport stay in sync, no stuck state when items are removed.
