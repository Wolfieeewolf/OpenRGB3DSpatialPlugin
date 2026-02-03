# Monitor presets (`monitors.json`)

Display plane presets live in `monitors.json` (plugin config dir) so you can pick a monitor by name and have **width** and **height** (mm) filled automatically instead of measuring.

## Where to get dimensions

There is **no free bulk API** for monitor specs. The best free source for up‑to‑date dimensions is:

- **[DisplayDB](https://www.displaydb.com/)** — TVs and monitors database. Search by brand/model, open a monitor page, then use **Screen Width** and **Screen Height** in the *Display* section (they’re given in mm). Use those values for `width_mm` and `height_mm`.

Other reference sites (e.g. DisplaySpec, manufacturer spec sheets) also list physical dimensions in mm; use **screen** width/height (viewable area), not full case size, when the site gives both.

Paid option: [TechSpecs API](https://developer.techspecs.io/) has a large product DB with specs (including dimensions) for programmatic/bulk use.

## JSON format

Each entry:

| Field      | Required | Description |
|-----------|----------|-------------|
| `id`      | No*      | Unique slug (e.g. `lg_27gp950`). If omitted, generated from brand + model (lowercase, non‑alphanumeric → `_`). |
| `brand`   | Yes      | Manufacturer (e.g. `LG`, `Dell`). |
| `model`   | Yes      | Model name as shown in UI (e.g. `27GP950-B`, `Alienware AW3423DW`). |
| `width_mm`| Yes      | Screen width in mm (from DisplayDB “Screen Width”). |
| `height_mm`| Yes     | Screen height in mm (from DisplayDB “Screen Height”). |

\* `id` must be unique; duplicate ids are skipped when loading.

Example:

```json
{
    "id": "lg_34wk95u",
    "brand": "LG",
    "model": "34WK95U",
    "width_mm": 794,
    "height_mm": 340
}
```

## Adding more monitors

1. Open [DisplayDB](https://www.displaydb.com/) and search for the monitor (e.g. “LG 34WK95U”).
2. On the monitor page, note **Screen Width** and **Screen Height** (mm) in the Display section.
3. Add a new object to the `monitors.json` array with `brand`, `model`, `width_mm`, `height_mm`, and optionally `id` (or leave `id` out to auto‑generate from brand + model).
4. Restart OpenRGB or reload the plugin so the new preset appears; then type the name in the display plane monitor combo to find it and auto‑fill dimensions.

The built‑in list is also defined in code as `kDefaultMonitorPresetJson` in `OpenRGB3DSpatialTab_ObjectCreator.cpp`; that’s used when no `monitors.json` exists. User/installed presets are loaded from the config file and can add or override entries.
