# Monitor presets (monitors/ folder)

Display plane presets live in a **monitors/** folder (plugin config dir). Each monitor is **one JSON file** (e.g. `lg_27gp950.json`). The plugin scans the folder at load and picks up every `.json` file. Add a monitor by adding a file; fix one by editing that file.

**Path (at runtime):** `.../OpenRGB/plugins/settings/OpenRGB3DSpatialPlugin/monitors/`

The repo ships **ui/monitors/** with one `.json` per default monitor. The app creates **config/monitors/** on first run and fills it from the old monitors.json (if present) or from built-in defaults. To use the shipped files, copy **ui/monitors/** to your config path above.

## One file per monitor

Each file is a **single JSON object** (not an array). Filename can be anything; the `id` inside the file is used as the preset id. If `id` is missing, the filename (without `.json`) is used.

| Field      | Required | Description |
|-----------|----------|-------------|
| `id`      | No*      | Unique slug (e.g. `lg_27gp950`). If omitted, taken from the filename stem. |
| `brand`   | Yes      | Manufacturer (e.g. `LG`, `Dell`). |
| `model`   | Yes      | Model name as shown in UI (e.g. `27GP950-B`, `Alienware AW3423DW`). |
| `width_mm`| Yes      | Screen width in mm (from DisplayDB “Screen Width”). |
| `height_mm`| Yes     | Screen height in mm (from DisplayDB “Screen Height”). |

\* Duplicate ids (same id in different files) are skipped; the first seen wins.

Example file **lg_27gp950.json**:

```json
{
    "id": "lg_27gp950",
    "brand": "LG",
    "model": "27GP950-B",
    "width_mm": 609,
    "height_mm": 355
}
```

## First run / migration

If the **monitors/** folder is empty, the plugin either reads the old **monitors.json** (if present) and splits it into one file per monitor in **monitors/**, or writes the built-in default presets as individual files. After that, you can add or edit files in the folder and they are picked up on next load. The old single **monitors.json** file is no longer read once **monitors/** exists and has files.

## Where to get dimensions

- **[DisplayDB](https://www.displaydb.com/)** — Search by brand/model, use **Screen Width** and **Screen Height** (mm) in the Display section for `width_mm` and `height_mm`.
- Use **screen** (viewable) dimensions, not full case size, when the source gives both.

## Adding or fixing a monitor

1. Add a new file in **monitors/** (e.g. `my_monitor.json`) with `brand`, `model`, `width_mm`, `height_mm`, and optionally `id` (or leave `id` out and the filename stem is used).
2. To fix one monitor, edit only that file and save.
3. Restart OpenRGB or reload the plugin so the list refreshes; then pick the monitor in the display plane combo to auto-fill dimensions.

## Controller presets (same idea)

**Controller presets** already use a folder: **custom_controllers/** (same config dir). Each preset is one `.json` file. Add from preset scans that folder; add or fix a controller preset by adding or editing a file there. See [PRESET_JSON.md](PRESET_JSON.md).
