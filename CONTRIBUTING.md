# Contributing to OpenRGB 3D Spatial Plugin

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

- **Tag format**: `vYY.MM.DD.V` — YY=year, MM=month, DD=day, V=version (e.g. `v26.02.03.1` = 3 Feb 2026, first release that day).
- **One-click from GitHub**: **Actions** → **Create release tag** → **Run workflow**. Leave **Release date** blank for today (UTC), or set it (e.g. `2026-02-03`). Leave **Version** at 0 to auto-increment. Click **Run workflow**. The workflow creates the tag on `master`, pushes it, and **Create Release** then builds and publishes the release. No local commands or manual tag needed.
- **Manual release in GitHub**: Repo → **Releases** → **Draft a new release** → create tag (e.g. `v26.02.03.2`) and publish.
- **Optional (CLI)**: From the repo root, `.\create-release-tag.ps1 -Date "YYYY-MM-DD"` then push the tag.

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

## Code audit

When touching code, keep DRY, KISS, null safety, layout, and clear/unstick in mind: selection and viewport stay in sync, no stuck state when items are removed.
