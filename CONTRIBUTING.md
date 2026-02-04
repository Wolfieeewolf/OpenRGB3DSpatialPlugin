# Contributing to OpenRGB 3D Spatial Plugin

This plugin follows the coding practices and API guidelines of the OpenRGB project. When in doubt, align with:

- **OpenRGB CONTRIBUTING** (style, merge request practices, C-style preferences, no QDebug/printf/cout).
- **OpenRGB RGBController API** (device/zone/LED/color vectors, `0x00BBGGRR` colors, name/location identity, null and bounds checks).

## Coding practices

- **DRY** (Don't Repeat Yourself): Extract repeated logic into shared helpers or types; avoid copy-paste.
- **KISS** (Keep It Simple): Prefer clear, straightforward code over clever or over-abstract solutions.
- **SOLID**: Single responsibility, open/closed, Liskov substitution, interface segregation, dependency inversion where they simplify the design.
- **YAGNI** (You Aren't Gonna Need It): Don't add code for hypothetical future needs; add it when required.
- **Tell, Don't Ask**: Prefer telling objects what to do (commands) over asking for data and deciding in the caller. Put behavior in the type that has the data; avoid "get X, get Y, then if X do A else B" in callers when that logic belongs on the object.

## Code quality

- **No debug code**: Remove `printf`, `qDebug`, temporary logging, or debug-only branches before committing.
- **No dead code**: Remove unused functions, variables, includes, and unreachable paths.
- **No legacy/deprecated**: Replace deprecated APIs and remove code that is no longer used or supported.
- **Comments**: Prefer minimal, meaningful comments. Avoid long decorative blocks; explain *why* when non-obvious. **Keep comments and code on separate lines**: a line like `// do XSomeFunction();` comments out the call; put the comment on one line and the statement on the next.

## Style

- Follow existing style in the file you edit.
- 4 spaces per indent; braces on their own lines.
- `snake_case` for variables, `CamelCase` for types and functions.
- Do not modify files under `OpenRGB/` (upstream submodule); only plugin-owned sources.

## OpenRGB RGBController API

The plugin consumes OpenRGB’s **RGBController** (device name, location, zones, LEDs, colors). When reading or writing controller/zone/LED data, follow the OpenRGB RGBController API: use `name` and `location` for device identity; use `zones[].start_idx` and zone LED counts for indices into `leds` and `colors`; colors are 32-bit `0x00BBGGRR`. Always guard controller pointers: custom controllers use `controller == nullptr` and `virtual_controller != nullptr`; physical controllers use `controller != nullptr`. Check `zone_idx < controller->zones.size()` and similar before indexing.

## Filter and sort (universal pattern)

Lists and preset pickers use a **unified filter/sort pattern** so behaviour and look are consistent across the plugin:

- **Filter:** Label **"Filter:"** with a combo that narrows the list (e.g. by brand for monitors, by category for controller presets, by category for the effect library). Use style: `color: gray; font-size: small;` for the label.
- **Sort:** Label **"Sort:"** with a combo that sets list order (e.g. Brand / Model / Size for monitors; Name / Category / Brand for presets). Same label style as Filter.
- When adding new list UIs (presets, devices, etc.), use this Filter + Sort row so users see the same pattern everywhere.

## UI correctness and null safety

- **Connections**: Every signal that should drive UI or viewport behaviour is connected; no missing or duplicate connects.
- **Selection sync**: List selection (e.g. controller list, reference points, display planes) stays in sync with the viewport; selecting in list updates viewport, selecting in viewport updates list where applicable.
- **Clear selection**: When the user clears selection (e.g. list row -1), both viewport and UI (position/rotation controls, selection label, LED spacing) are updated so nothing appears “stuck” on the previous selection.
- **Layout**: UI layout makes sense; sections and tabs are grouped logically; no overlapping or squashed controls; tab content fits (scroll or min size where needed).
- **Clear on unselect**: When selection is cleared, all dependent UI clears or disables; nothing sticks to the previous selection.
- **No stuck state**: When an item is removed (e.g. effect removed from stack), dependent UI resets or closes (e.g. effect detail UI must not stay open for a removed effect); combo/list selection and detail panels stay in sync with actual data.
- **Null checks**: Any use of `viewport`, widget pointers, or other optional pointers is guarded (e.g. `if(viewport)`) before use so teardown or partial init cannot cause crashes.
- **Widget guards**: In slots that update many widgets (e.g. position/rotation spins and sliders), guard with null checks so reorder or late call cannot dereference null.

## Releasing

**Recommended: let GitHub create the tag and release for you** — no timezone or script issues.

- **Version file**: The repo has a **PROJECT_VERSION** file in the root (one line: `YY.MM.DD.V`, e.g. `26.02.03.5`). The build and release workflows use it when present: qmake injects it into the plugin, and **Create release tag** (GitHub or `create-release-tag.ps1`) uses it for the tag. Update **PROJECT_VERSION** to the next release version before creating the tag so the version number is correct everywhere.
- **Tag format**: `vYY.MM.DD.V` — YY=year, MM=month, DD=day, V=version (e.g. `v26.02.03.1` = 3 Feb 2026, first release that day).
- **One-click from GitHub**: **Actions** → **Create release tag** → **Run workflow**. If **PROJECT_VERSION** exists, that version is used for the tag. Else leave **Release date** blank for today (UTC), or set it; leave **Version** at 0 to auto-increment. Click **Run workflow**. The workflow creates the tag, pushes it, and **Create Release** then builds and publishes the release.
- **Manual release in GitHub**: Repo → **Releases** → **Draft a new release** → create tag (e.g. `v26.02.03.2`) and publish.
- **Optional (CLI)**: From the repo root, `.\create-release-tag.ps1` (uses **PROJECT_VERSION** if present) or `.\create-release-tag.ps1 -Date "YYYY-MM-DD"` then push the tag.

## Building

The plugin is built with **Qt** (qmake) and **OpenRGB** as a submodule. Only plugin-owned sources are built; `OpenRGB/` is used for headers and linking.

- **Prerequisites**: Qt 5.15+ (or Qt 6), MSVC (Windows) or GCC/Clang (Linux), OpenRGB submodule initialized (`git submodule update --init`).
- **Windows**: Open an **x64 Native Tools Command Prompt for VS** (or run `vcvars64.bat`), ensure Qt’s `bin` is on PATH, then from the repo root:
  - `qmake OpenRGB3DSpatialPlugin.pro CONFIG+=release`
  - `nmake`
- **Linux**: From the repo root: `qmake OpenRGB3DSpatialPlugin.pro PREFIX=/usr` then `make -j$(nproc)`.
- **Qt Creator**: Open `OpenRGB3DSpatialPlugin.pro`, configure the kit (Qt + compiler), then build. Ensure the OpenRGB submodule is present.
- **CI**: GitHub Actions (see `.github/workflows/build.yml` and `release.yml`) and GitLab CI build on push/tag; no local Qt required for release builds.

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
