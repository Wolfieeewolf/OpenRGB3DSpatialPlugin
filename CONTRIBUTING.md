# Contributing to OpenRGB 3D Spatial Plugin

## OpenRGB reference documentation (MCP mirror)

When changing plugin behavior—especially anything that touches devices, colors, zones, builds, or user-facing setup—stay consistent with the upstream OpenRGB docs in the MCP tree (adjust the drive letter if your mirror lives elsewhere):

| Document | Use it for |
| -------- | ---------- |
| `f:/MCP/CONTRIBUTING.md` | Merge-request style, C++/Qt style guidelines, logging (`LogManager` not `QDebug`/`printf`), general OpenRGB contribution expectations. |
| `f:/MCP/RGBControllerAPI.md` | LEDs, zones, modes, `colors` layout, `SetCustomMode` / `UpdateLEDs` expectations—any path that reads or writes controller state. |
| `f:/MCP/OpenRGBSDK.md` | Network SDK packet layout and plugin-related IDs—only if you change or document SDK-facing behavior. |
| `f:/MCP/Common-Modes.md` | Naming and meaning of **firmware** modes on devices; plugin spatial effects are software-driven but avoid confusing users with conflicting terminology where it overlaps UI copy. |
| `f:/MCP/Compiling.md` | How OpenRGB itself is built; align user-facing build hints (Qt, qmake, deps) with this where relevant. |
| `f:/MCP/UdevRules.md` | Linux permissions for USB/HID access—reference when documenting Linux install or troubleshooting detection. |
| `f:/MCP/USBAccess.md` | USB setup and permission notes—user docs or issues involving USB devices. |
| `f:/MCP/SMBusAccess.md` | SMBus/I2C setup (Windows/Linux/macOS)—user docs for RAM/motherboard RGB access issues. |
| `f:/MCP/KernelParameters.md` | Kernel cmdline (e.g. ACPI/SMBus conflicts)—user docs, not plugin C++ unless you add explicit guidance text. |

**Minimum bar for this repo:** treat `CONTRIBUTING.md` and `RGBControllerAPI.md` as binding for code style and anything that touches `RGBController` / LED indices / zones. The others apply when you document behavior, debug hardware access, or work near those subsystems.

## Scope

- Plugin rules apply to plugin-owned code in this repository (e.g. `Effects3D/`, `ui/`, `Game/`, root `.pro`).
- Do not modify files under `OpenRGB/` (upstream subtree) unless explicitly requested.

## Code quality

- No debug code, dead code, or compatibility migration branches unless explicitly required.
- Keep code simple: DRY, KISS, YAGNI, single-responsibility functions.
- Comments should be minimal and explain non-obvious intent only.

## Style

- Follow **existing style in the file first**, then `f:/MCP/CONTRIBUTING.md` where it does not conflict.
- 4 spaces, braces on their own lines, `snake_case` variables, `CamelCase` types/functions.
- Avoid `printf`, `std::cout`, and `QDebug`; use `LogManager`.
- Prefer explicit types when they improve readability; avoid unnecessary `auto`.
- Prefer indexed loops where index math matters; range loops are fine for simple read-only traversal.
- New/changed files: SPDX license identifier in the header; keep headers minimal unless the file warrants more.

## RGBController safety rules

Aligned with `f:/MCP/RGBControllerAPI.md` and safe patterns for mapped devices:

- Always guard pointers before dereference (`controller`, `transform`, `virtual_controller`, `zone_manager`).
- Before `zones[idx]`, validate `idx < zones.size()`.
- Before color writes, validate global LED index against `colors.size()`.
- For mapped LEDs, validate mapping index against `led_positions.size()` before use.
- Prefer early-continue guards for invalid mappings or stale data.

## Game / Minecraft and similar effects

- Telemetry (e.g. UDP on localhost) is separate from the RGBController wire protocol; still apply logging, validation, and thread-safety patterns from the rest of the plugin.
- Effect UI that targets the stack (library hub, global settings, add-to-stack flow) must keep **list/combo/signal** state consistent—see **UI** below.

## UI

- Keep signal wiring/disconnect paths symmetrical for dynamically created effect UI widgets.
- Keep list, combo, and selection state synchronized.
- Clear/disable dependent controls when selection is invalid.

## Building

- Qt 5.15+ (or 6), OpenRGB submodule initialized.
- Windows: `qmake OpenRGB3DSpatialPlugin.pro CONFIG+=release` then `nmake`.
- Linux: `qmake OpenRGB3DSpatialPlugin.pro PREFIX=/usr` then `make -j$(nproc)`.
- For full OpenRGB build prerequisites, see `f:/MCP/Compiling.md`.

## Releasing

- `PROJECT_VERSION` / release naming: calendar-style `YY.MM.DD.V` (e.g. `26.04.06.1`).
- GitHub Actions **Create Release** (`.github/workflows/release.yml`) runs on tag push when the tag matches **`v*`** or **`*.*.*.*`** (e.g. `v26.04.06.1` or `26.04.06.1`). Use either form consistently with your release process.
