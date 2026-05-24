# UI code layout

## `OpenRGB3DSpatialTab` implementation files

| File | Responsibility |
|------|----------------|
| `OpenRGB3DSpatialTab.cpp` | Construction, viewport, effect library, grid hooks, accessors |
| `OpenRGB3DSpatialTab_Setup.cpp` | Scene setup: devices, controllers, display planes, status helpers |
| `OpenRGB3DSpatialTab_Layout.cpp` | Layout profiles, custom controller library on disk |
| `OpenRGB3DSpatialTab_Scene.cpp` | Zones and reference points |
| `OpenRGB3DSpatialTab_Effects.cpp` | Effect stack UI, render path, profiles, stack presets |
| `OpenRGB3DSpatialTab_Settings.cpp` | `OpenRGB.json` session UI, profiles panel glue |
| `OpenRGB3DSpatialTab_Audio.cpp` | Audio input panel |

Pattern: `OpenRGB3DSpatialTab_<area>.cpp` — one area per file, no `ObjectCreator_*` suffix.

## Panel widgets (`ui/*.cpp`)

| File | Widget |
|------|--------|
| `*TabPanel.cpp` / `*Panel.cpp` | Designer form + `bindTab()` |
| `ControllerCards.cpp` | `SpatialControllerCardWidget`, `SpatialControllerCardList` |
| `CustomControllerWidgets.cpp` | `CustomControllerDeviceWidget`, `CustomControllerDeviceList` |
| `CustomControllerDialog.cpp` | Full editor dialog |
| `CustomControllerPreviewDialog.cpp` | Preview dialog |

## Forms

`ui/forms/*.ui` — Qt Designer only; keep names aligned with the widget class (e.g. `ObjectCreatorTabPanel.ui`).
