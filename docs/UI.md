# UI (Qt Designer)

## Architecture today

- **Shell:** `ui/forms/OpenRGB3DSpatialTab.ui` embeds panel widgets (`EffectLibraryPanel`, `ControllerListPanel`, `AudioInputPanel`, …).
- **Access:** `ui->panel` on the tab; panel classes expose getters (`ZonesPanel::zonesList()`, …); tab forwards via methods at the end of `OpenRGB3DSpatialTab.cpp`. **No** duplicate widget pointers on `OpenRGB3DSpatialTab`.
- **Effect chrome:** `ui/forms/Effect*.ui` for stack/library panels; all effects build custom settings in C++ via **`EffectUiRows.h`** (media effects also embed **`MediaTextureAmbienceBlock.ui`**).
- **Reusable rows:** `EffectSliderRow`, `EffectLabeledComboRow`, `EffectLabeledSpinRow`, `EffectCheckRow`, `EffectInfoLabel`, `EffectSectionHeading`, `AudioEqBandColumn`.
- **Count:** **85** `.ui` files under `ui/forms/` — treat the directory as the inventory.

## Panel groups

| Group | Role |
|-------|------|
| Run mode | Library, stack, zones, global settings, controls host, Minecraft hub, audio input, dynamic blend row |
| Settings | Profiles, grid/room, scene transform, object creator stack |
| Controllers | Combined list panel + card list + card widget (available vs in-scene modes in C++) |
| Effect common | Surfaces, motion, output, geometry, colors, sampler, layer banner, controls root |
| Per-effect / adjunct | `EffectUiRows`, `MediaTextureAmbienceBlock`, screen mirror shells, Minecraft scroll |
| Custom controller | Editor dialog, device list, layer tab, GL preview dialog |
| Dialogs | Zone controller picker, **Audio Advanced** analyzer dialog |

## Dynamic UI (stays in C++)

| Piece | Why |
|-------|-----|
| Audio EQ row | Band count 8 / 16 / 32 rebuilds columns |
| Stack blend row | One `EffectStackBlendRow.ui` inserted per selected layer |
| Transport buttons | Borrowed from layer banner or `EffectTransportRow` |
| Controller backing stores | In-memory lists, not widgets |

## Theme (plugin chrome)

- **No** `setStyleSheet` on structural chrome.
- **No** custom `QPalette` on panels / `QGroupBox` — inherit OpenRGB / system theme.
- **Allowed:** `EffectInfoLabel` hints; card selection tint on `cardFrame` via host `QPalette::Highlight` only (`SpatialControllerCardWidget`, `CustomControllerDeviceWidget`).
- **Strings:** user-visible Unicode via `QStringLiteral("\\uXXXX")` in sources (MSVC `/utf-8` in `.pro`).

## OpenGL (not Designer)

`LEDViewport3D`, `Gizmo3D`, capture preview paint — see [VIEWPORT.md](VIEWPORT.md).

## When editing forms

- Merge only truly duplicate layouts; **do not** bulk-merge per-effect settings files.
- Prefer `EffectInfoLabel` over one-off `QLabel` hints when you touch a form.
- After changes: rebuild plugin and run the [smoke checklist](#smoke-check-after-ui-changes).

## Smoke check after UI changes

Effect stack + blend; zones; scroll per-effect settings; audio start/stop (**48px** spectrum, **Advanced…** saves); controller lists; settings profiles + grid; Minecraft hub if touched; no mojibake in combo labels.
