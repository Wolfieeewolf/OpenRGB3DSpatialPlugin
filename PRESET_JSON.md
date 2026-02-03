# Custom controller preset JSON format

Preset files live in the **controller_presets** folder (plugin config dir). Each preset is one `.json` file. Use **Add from preset** in the Object Creator to add them. Your saved custom controllers stay in **custom_controllers** (one file per controller); presets are templates in **controller_presets**.

## Matching (presets “just work”)

Matching uses **OpenRGB device name** (`controller_name`) and **location** from the preset’s `mappings`. When you add from preset, the plugin finds your detected controllers that match that name. So anyone with the same device (same name in OpenRGB) can use the preset without setup—the universal OpenRGB device name is enough.

## Naming rules

- **Required:** `name` (short label for the preset list, e.g. "Fan", "Onboard LED") and `mappings` (LED layout).
- **Optional but recommended:** `category`, `brand`, `model` for filtering and for clean display names.

When you add from preset, the virtual controller name is built as:

- **Single matching device:** When `brand` and `model` are set → **Device** only (e.g. "Corsair SP120 RGB PRO"). When not set → **Category - ControllerName - Name** (e.g. "Motherboards - ASUS ROG … - Onboard LED").
- **Multiple matching devices:** Always **Device 1**, **Device 2**, **Device 3** (same for all presets). Device = `brand` + `model` if set, otherwise the OpenRGB controller name.
- **Device** = `brand` + `model` when both are set, otherwise the OpenRGB controller name from the mappings.

## JSON format

| Field        | Required | Description |
|-------------|----------|-------------|
| `name`      | Yes      | Short label shown in the preset list and used in the final name (e.g. "Front Fan", "Onboard LEDs"). |
| `mappings`  | Yes      | Array of LED mappings (grid position + controller/zone/LED). Same structure as exported custom controller JSON. |
| `width`     | Yes      | Grid width (cells). |
| `height`    | Yes      | Grid height (cells). |
| `depth`     | Yes      | Grid depth (cells). |
| `category`  | No       | Device type for filtering and naming. See [Categories](#categories). Default `other`. |
| `brand`     | No       | Manufacturer (e.g. "Corsair", "MSI", "AMD"). Used with `model` for the "Device" part of the name. |
| `model`     | No       | Product model (e.g. "SP120 RGB PRO", "MPG X570 GAMING EDGE WIFI"). Used with `brand` for the "Device" part of the name. |
| `spacing_mm_x`, `spacing_mm_y`, `spacing_mm_z` | No | LED spacing in mm (default 10). |

Each entry in `mappings` must include: `x`, `y`, `z`, `controller_name`, `controller_location`, `zone_idx`, `led_idx`. These come from OpenRGB when you export a custom controller; **controller_name** is used to match your detected devices when adding from preset.

## Categories

Use one of the [OpenRGB device types](https://openrgb.org/devices.html) (or the extras below):

- `graphics_cards`, `motherboards` (or `motherboard`), `ram_sticks` (or `ram`), `mouses`, `keyboards`, `mousemats`, `coolers`, `led_strips`, `headsets`, `gamepads`, `accessories`, `microphones`, `speakers`, `storages`, `cases`
- Plus: `fans`, `cpu_cooler`, `waterblock`, `psu`, `other` (default)

## Example

```json
{
    "name": "Front Fan",
    "category": "fans",
    "brand": "Corsair",
    "model": "SP120 RGB PRO",
    "width": 2,
    "height": 2,
    "depth": 1,
    "spacing_mm_x": 10,
    "spacing_mm_y": 10,
    "spacing_mm_z": 10,
    "mappings": [
        {
            "x": 0, "y": 0, "z": 0,
            "controller_name": "Corsair SP120 RGB PRO",
            "controller_location": "1:1",
            "zone_idx": 0,
            "led_idx": 0,
            "granularity": 2
        }
    ]
}
```

When you add this preset and have one matching Corsair SP120 RGB PRO, the custom controller is named: **Corsair SP120 RGB PRO**. If you have three matching devices (fans, mice, motherboards, etc.), you always get **Device 1**, **Device 2**, **Device 3** (e.g. **Corsair SP120 RGB PRO 1**, **2**, **3**).

## Preset examples (categories and display names)

- **Razer Deathadder Elite:** `category`: `"mouses"`, `brand`: `"Razer"`, `model`: `"Deathadder Elite"`. Mappings must use the OpenRGB controller name (e.g. "Razer DeathAdder Elite" or whatever your OpenRGB reports). Display: "Razer Deathadder Elite".
- **Corsair SP120 RGB PRO (fans):** `category`: `"fans"`, `brand`: `"Corsair"`, `model`: `"SP120 RGB PRO"`. One fan → "Corsair SP120 RGB PRO"; multiple → "Corsair SP120 RGB PRO 1", "2", "3".
- **AMD Wraith Prism:** `category`: `"cpu_cooler"`, `brand`: `"AMD"`, `model`: `"Wraith Prism"`. Display: "AMD Wraith Prism".
- **Onboard LED (motherboard):** `category`: `"motherboard"` (or `"motherboards"`), no `brand`/`model`. Use OpenRGB motherboard name in mappings. `name`: `"Onboard LED"`. Display: "Motherboards - [Full motherboard name] - Onboard LED".

## Adding presets

1. Create a custom controller in the plugin, assign LEDs, then **Export** it to a file.
2. Copy the JSON into the preset folder: `.../OpenRGB/plugins/settings/OpenRGB3DSpatialPlugin/controller_presets/` (one file per preset).

The repo ships **ui/controller_presets/** with an example; copy that folder (or files) to the config path above to use it, or add your own preset files there.
3. Edit the JSON: set `name` to a short label, add `category` (e.g. `"fans"`, `"mouses"`, `"cpu_cooler"`, `"motherboard"`), and for device-only names set `brand` and `model`.
4. Use **Add from preset** in the Object Creator; filter by category if needed, select the preset, and add. Matching uses OpenRGB device name; virtual controller(s) use the naming rules above.

The preset list shows **\[Category] Brand Model - Name** (or **Name — ControllerName** when brand/model are not set).
