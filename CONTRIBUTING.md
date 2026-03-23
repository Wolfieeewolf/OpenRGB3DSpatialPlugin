# Contributing to OpenRGB 3D Spatial Plugin

Follow:
- `f:/MCP/CONTRIBUTING.md`
- `f:/MCP/RGBControllerAPI.md`

Scope note:
- Plugin rules apply to plugin-owned code in this repository.
- Do not modify files under `OpenRGB/` (upstream subtree) unless explicitly requested.

## Code quality

- No debug code, dead code, or compatibility migration branches unless explicitly required.
- Keep code simple: DRY, KISS, YAGNI, single-responsibility functions.
- Comments should be minimal and explain non-obvious intent only.

## Style

- Follow existing style in the file first.
- 4 spaces, braces on their own lines, `snake_case` variables, `CamelCase` types/functions.
- Avoid `printf`, `std::cout`, and `QDebug`; use `LogManager`.
- Prefer explicit types when they improve readability; avoid unnecessary `auto`.
- Prefer indexed loops where index math matters; range loops are fine for simple read-only traversal.
- File headers should be minimal: SPDX line plus a short header only when useful.

## RGBController Safety Rules

- Always guard pointers before dereference (`controller`, `transform`, `virtual_controller`, `zone_manager`).
- Before `zones[idx]`, validate `idx < zones.size()`.
- Before color writes, validate global LED index against `colors.size()`.
- For mapped LEDs, validate mapping index against `led_positions.size()` before use.
- Prefer early-continue guards for invalid mappings or stale data.

## UI

- Keep signal wiring/disconnect paths symmetrical for dynamically created effect UI widgets.
- Keep list, combo, and selection state synchronized.
- Clear/disable dependent controls when selection is invalid.

## Building

- Qt 5.15+ (or 6), OpenRGB submodule initialized.
- Windows: `qmake OpenRGB3DSpatialPlugin.pro CONFIG+=release` then `nmake`.
- Linux: `qmake OpenRGB3DSpatialPlugin.pro PREFIX=/usr` then `make -j$(nproc)`.

## Releasing

- `PROJECT_VERSION` in root uses `YY.MM.DD.V`.
- Tag format: `vYY.MM.DD.V`.
