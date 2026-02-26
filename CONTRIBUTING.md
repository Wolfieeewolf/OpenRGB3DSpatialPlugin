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
