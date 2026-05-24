# Plugin data on disk

OpenRGB config root (e.g. `%AppData%\Roaming\OpenRGB` on Windows).

## `OpenRGB.json` — `3DSpatialPlugin` key

Small preferences live in the host app settings file (not separate plugin-only JSON for these):

| Area | Examples |
|------|----------|
| Viewport / room | `Camera`, `Grid` (X/Y/Z, `ScaleMM`, `SnapEnabled`), `Room` (`UseManual`, `WidthMM`, …), `RoomGrid` overlay |
| UI chrome | `Ui.LeftModeTab` (Setup / Effects / Settings left tabs) |
| Layout profiles | `SelectedProfile`, `AutoLoadEnabled` |
| Effect profiles | `EffectSelectedProfile`, `EffectAutoLoadEnabled` |
| LED defaults | `LEDSpacing` X/Y/Z (Object Creator spacing) |
| Audio (main panel) | Device, gain, band count, FFT size |
| Audio (advanced) | `AudioSmoothingPct`, `AudioBandPeakDecayPct`, `AudioBassPeakDecayPct`, `AudioActivityPeakDecayPct`, `AudioVisualizerPeakDecayPct`, `AudioVisualizerFloorPct`, `AudioAutoLevelEnabled`, `AudioAutoLevelPeakDecayPct`, `AudioAutoLevelFloorDecayPct`, `AudioBassCrossoverHz`, `AudioMidCrossoverHz` |

Other OpenRGB plugins use their own top-level keys; keys do not collide.

## `plugins/settings/OpenRGB3DSpatialPlugin/`

All plugin-owned files go here.

### Subfolders

| Folder | Contents |
|--------|----------|
| `controllers/` | LED layout `.json` — import from [OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets) |
| `layouts/` | Scene profiles: device poses, zones, display planes |
| `spatial-shaders/` | User GLSL for **Shader Field** (`*.fs`) |

### Files in plugin root

| Pattern | Purpose |
|---------|---------|
| `effect_stack.json` | Auto-saved stack (session resume) |
| `*.effectprofile.json` | Named effect profiles (Settings → Profiles) |
| `*.stack.json` | Named stack presets (Effects → Stack Presets) |

**Open folder:** Settings → Profiles → **Open config folder**. Shader Field → **Open user shaders folder**.

## Backup tip

Copy the whole `OpenRGB3DSpatialPlugin` folder plus export/note your `3DSpatialPlugin` section from `OpenRGB.json` before major upgrades.
