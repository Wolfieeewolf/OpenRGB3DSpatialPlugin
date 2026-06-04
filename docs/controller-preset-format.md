# Custom controller JSON format

Custom controller layouts are stored as `.json` files under:

`OpenRGB/plugins/settings/OpenRGB3DSpatialPlugin/controllers/`

The plugin loads them on startup and when you import from [OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets).

There are **two kinds** of files. Only the first should be shared or committed to the presets repo.

---

## 1. Portable preset (shareable)

Used in **OpenRGB3DSpatialPresets**. Works on any PC that has the same OpenRGB device.

| Field | Required | Purpose |
| ----- | -------- | ------- |
| `format` | yes | Must be `OpenRGB3DSpatialCustomController` |
| `version` | yes | Must be `1` |
| `name` | yes | Layout title in the plugin **Custom controllers** list (see naming rules below) |
| `brand` | recommended | Hardware vendor — helps find device when names differ slightly |
| `model` | recommended | Product model — paired with `brand` for matching |
| `category` | optional | Preset taxonomy only (`graphics_cards`, `cpu_cooler`, `fans`, …) — **not** shown as the layout name |
| `width`, `height`, `depth` | yes | Grid size in cells |
| `spacing_mm_x/y/z` | yes | LED spacing in millimetres |
| `mappings` | yes | LED assignments (see below) |
| `light_blockers` | optional | Grid cells that block spatial lighting (see below) |

**Light blockers** (optional array): each entry has `x`, `y`, `z` (layer). Mark empty or LED cells where light must not pass through (e.g. keyboard base). Used by **Spatial · Lighting** effects.

**Every mapping** in a portable preset:

| Field | Value |
| ----- | ----- |
| `controller_name` | Exact string from OpenRGB **Devices** tab → device name (`RGBController::GetName()`) |
| `controller_location` | Always `"1:1"` (portable placeholder; do not use HID/USB paths) |
| `x`, `y`, `z` | Grid cell |
| `zone_idx`, `led_idx` | OpenRGB zone/LED index |
| `granularity` | Usually `2` (per-LED) |

**Filename:** lowercase `snake_case` slug, e.g. `intel_arc_a770_limited_edition.json`. Does not have to match `name`, but should be recognizable.

---

## 2. Machine backup (auto-saved library)

Files in your `controllers/` folder written by the plugin when layouts are **saved in the library** stay machine-specific so they reload reliably on your PC:

- `controller_location` = real OpenRGB location (`HID:…`, `DDP:…`, `I2C:…`, etc.)
- Usually no `brand` / `model` / `category`

## Export button (portable preset)

**Object Creator → Export** writes the **shareable preset** format (`ToPortablePresetJson`):

- `controller_location` = `"1:1"` on every mapping
- `brand`, `model`, and `category` when all LEDs map to one OpenRGB device
- Default filename is `snake_case.json`

Use Export when contributing to [OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets). Use the auto-saved library files only for local backups.

---

## OpenRGB naming rules (consistency)

OpenRGB exposes each device through `RGBController` with:

| OpenRGB field | Use in preset JSON |
| ------------- | ------------------ |
| **Name** (`GetName()`) | Copy verbatim into every mapping’s `controller_name` |
| **Vendor** | Optional → preset `brand` |
| **Description** | Reference only when verifying the device; do not use as layout `name` |
| **Location** | Never copy into portable presets — use `"1:1"` |
| **Zones / LEDs** | `zone_idx` / `led_idx` in mappings |

### Layout `name` (top-level)

| Situation | `name` should be |
| --------- | ---------------- |
| **Full device layout** (one OpenRGB device, whole or typical use) | **Same as** `controller_name` / OpenRGB device name |
| **Partial layout** (one zone or region on a device) | `{OpenRGB device name} - {short layout label}` — e.g. `MSI MPG X570 GAMING EDGE WIFI AM4 ATX Motherboard - Onboard LED` |
| **Several layouts, same device** (e.g. three case-fan positions) | Short **layout** label is OK — e.g. `Case Light Bottom`, `Case Light Middle` — but `controller_name` must still be the real OpenRGB name |

### Do **not** use as layout `name`

- Device **type** words: `Cooler`, `Fan`, `Mouse`, `Keyboard`, `GPU`, `Motherboard`, …
- Preset **category** strings: `graphics_cards`, `cpu_cooler`, `mouses`, `fans`
- Generic placeholders: `Graphics Card`, `Onboard LED` alone (without the OpenRGB device name)

Those belong in `category` / `brand` / `model` only, not in `name`.

### How to get the correct `controller_name`

1. Open OpenRGB → **Devices**.
2. Select the device → copy the **name** exactly (spacing and capitalisation matter).
3. Paste into all mappings’ `controller_name`.
4. Set top-level `name` using the table above.

---

## Mapping object schema

```json
{
  "x": 0,
  "y": 0,
  "z": 0,
  "controller_name": "Intel Arc A770 Limited Edition",
  "controller_location": "1:1",
  "zone_idx": 3,
  "led_idx": 0,
  "granularity": 2
}
```

---

## Minimal portable preset example

See [presets/template.controller.json](../presets/template.controller.json) and [presets/examples/amd_wraith_prism.json](../presets/examples/amd_wraith_prism.json).

---

## Checklist before publishing a preset

- [ ] `controller_name` matches OpenRGB **Devices** name on a real system
- [ ] All mappings use `"controller_location": "1:1"`
- [ ] Top-level `name` follows naming rules (not `Cooler` / `Mouse` / `Fan` / category slug)
- [ ] `brand` and `model` filled in for matching
- [ ] `zone_idx` / `led_idx` verified in OpenRGB
- [ ] File is valid JSON (no trailing commas)
- [ ] Filename is `snake_case.json`

---

## Presets repo workflow

1. Copy [presets/template.controller.json](../presets/template.controller.json).
2. Build layout in the plugin custom controller editor (or edit JSON carefully).
3. Validate against this document.
4. Open a PR in [OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets).

Plugin issues (load failures, UI) → [OpenRGB3DSpatialPlugin](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/issues).  
Wrong LED positions / names in JSON → [OpenRGB3DSpatialPresets](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPresets/issues).
