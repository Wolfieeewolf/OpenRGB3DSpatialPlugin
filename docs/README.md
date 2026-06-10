# OpenRGB 3D Spatial Plugin — developer docs

| Document | Description |
| -------- | ----------- |
| [../CONTRIBUTING.md](../CONTRIBUTING.md#no-legacy-or-backward-compatibility-paths) | **No legacy / backward-compat paths** — what to remove; viewport-only exception |
| [CODEBASE_AUDIT.md](CODEBASE_AUDIT.md) | Trim dead code; log removed legacy paths |
| [controller-preset-format.md](controller-preset-format.md) | **Custom controller JSON** — schema, OpenRGB naming rules, portable presets vs exports |
| [SPATIAL_MODES_GUIDE.md](SPATIAL_MODES_GUIDE.md) | **User-facing** — room modes vs samplers vs shader field; what stacks together |
| [SPATIAL_ROOM.md](SPATIAL_ROOM.md) | **Spatial room evaluation** — `SpatialRoom/` taxonomy, capabilities, dual-path (small vs full room) |
| [SPATIAL_LIGHTING_ENGINE.md](SPATIAL_LIGHTING_ENGINE.md) | **Spatial lighting engine** — room-native lights/occlusion; viewer anchor vs effect origin |
| [SPATIAL_LIGHTING_TEST.md](SPATIAL_LIGHTING_TEST.md) | Manual test checklist for **Spatial · Lighting** effects |
| [VIEWPORT.md](VIEWPORT.md) | 3D viewport, HiDPI, gizmo, grid settings |
| [../presets/template.controller.json](../presets/template.controller.json) | Copy-paste starter file for new presets |
| [../presets/examples/](../presets/examples/) | Reference presets that follow the format |

User-facing config paths are described in the root [README.md](../README.md#config-on-disk).

Community presets are published in [OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets).
