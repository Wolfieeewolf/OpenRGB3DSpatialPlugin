# Custom controller presets – design note

**Implemented:** “Add from preset” uses the **custom_controllers** folder (same as saved custom controllers). Presets are JSON files there. Preset format and naming are in [PRESET_JSON.md](PRESET_JSON.md). Matching uses OpenRGB controller name (and location) from the preset mappings, so presets "just work" for anyone with the same device—no setup. When brand and model are set, names are device-only; when not set (e.g. onboard LED), names use category + controller name + preset name. Previously we matched by OpenRGB controller name and named entries with the official device (e.g. “Corsair SP120 RGB PRO - Front Fan 1”, “2”, “3” for multiple fans).

## Is there an external list we could use?

**No.** There isn’t a public database of “LED layout” that fits our system.

- **Monitors:** Physical size (width/height mm) is standard; sites like DisplayDB list them, so we can have a static `monitors.json`.
- **RGB controllers:** OpenRGB gives us **zones** and **LED counts** per device. The **spatial layout** (which LED goes in which grid cell `(x,y,z)`) is something *we* define in the Custom Controller Creator. No one else publishes “Corsair K70: LED 0 at grid (0,0), LED 1 at (1,0)…” in a form we can consume.
- **QMK** has per-keyboard LED positions in firmware, but that’s key-matrix → LED index and physical (x,y) in their own units, not our grid + OpenRGB controller/zone/LED. So it doesn’t map directly.

So any “list” of controller layouts has to be **our own** (and user-contributed), not pulled from somewhere else.

## What we can do: our own preset library

We already have everything needed for presets:

- **Export:** A custom controller is saved as JSON (`VirtualController3D::ToJson`) with name, grid size, spacing, and per-LED: `controller_name`, `controller_location`, `zone_idx`, `led_idx`, `x,y,z`.
- **Import:** `VirtualController3D::FromJson` matches `controller_name` + `controller_location` to the user’s detected controllers and rebuilds the grid. If a controller isn’t found, we already support “unknown device” (e.g. for import).

So a **preset** is just that same JSON. If User A has a “Corsair K70 RGB MK.2” and builds a layout, they export it. We (or they) put that JSON into a preset pack. User B, who has the same keyboard (same name + location in OpenRGB), loads the preset → FromJson matches their controller → layout is applied. No extra format or external API.

## How to build the list

1. **Ship a few presets** – For popular devices we (or contributors) create a layout once and add the JSON to the repo (e.g. `controller_presets/` or a single `controller_presets.json` with an array of preset objects).
2. **Let users add to it** – “Export as preset” could write to a user-configurable preset directory (or copy path), and “Add from preset” could scan that directory + the built-in list so users can share presets (e.g. via GitHub) and drop new JSONs in.
3. **Optional: community repo** – A separate repo or wiki listing “preset for Corsair K70”, “preset for Razer BlackWidow”, etc., with download links or pasteable JSON. Still our format; we don’t pull from an external API.

So we don’t pull LED layout “from somewhere” – we **create and curate** the list, and users extend it by exporting and sharing.

## Suggested implementation (high level)

- **Preset storage:**  
  - Either a single JSON file (e.g. `controller_presets.json`) with an array of `{ "name": "Corsair K70 RGB MK.2", "data": <VirtualController3D JSON> }` (or just an array of VirtualController3D JSONs),  
  - Or a folder (e.g. `controller_presets/`) with one `.json` file per preset.  
  - Built-in presets live in the repo; user presets in a config dir (e.g. same place as `monitors.json`).
- **UI:**  
  - Next to “Create” / “Import”, add **“Add from preset”** (or “Add from library”): list presets by name (and maybe controller name), user picks one → load with FromJson (using current detected controllers) → add to custom controllers list (and optionally to 3D view).  
  - **“Export as preset”** on a saved custom controller: save current VirtualController3D JSON to the user preset folder (or “Save as…” so they can add it to the library / share it).
- **Matching:**  
  - FromJson already matches by `controller_name` + `controller_location`. If no controller matches, mappings keep `controller == nullptr` (unknown device); we can show a short message like “X mappings could not be matched to current devices” like we do for import.

No new “LED layout” format or external source – just reuse existing export/import and add preset discovery (list + pick) and optional “export to library” so the list can grow over time.

---

## Categories (motherboard, fans, RAM, etc.)

Presets can include an optional **`"category"`** field so the Add-from-preset list is grouped and names stay simple but clear.

- **Allowed values:** Match [OpenRGB Supported Devices](https://openrgb.org/devices.html) device types: `graphics_cards`, `motherboards` (or `motherboard`), `ram_sticks` (or `ram`), `mouses`, `keyboards`, `mousemats`, `coolers`, `led_strips`, `headsets`, `gamepads`, `accessories`, `microphones`, `speakers`, `storages`, `cases`; plus `fans`, `cpu_cooler`, `waterblock`, `psu`, `other` (default if missing).
- **In the Add-from-preset dialog:** A **Category** dropdown filters the list (All, Motherboard, Fans, RAM, CPU cooler, Waterblock, PSU, Other). Each preset is shown as `[Category] PresetName — ControllerName`.
- **In the virtual controller name when added:** When preset has `brand` and `model`, name is device-only: single → **Device** (e.g. **Corsair SP120 RGB PRO**, **Razer Deathadder Elite**, **AMD Wraith Prism**), multiple → **Device 1**, **Device 2**, **Device 3**. When `brand`/`model` are not set (e.g. onboard LED), name is **Category - ControllerName - PresetName** (e.g. **Motherboards - MSI MPG X570 - Onboard LED**). Category is used for filtering in the preset list.

Add to your preset JSON: `"category": "fans"`, `"category": "motherboard"`, etc.

## Naming and matching

- **Preset list label:** The `"name"` field in the preset JSON is the short label in the preset list (e.g. “Front Fan”, “Onboard LEDs”). You can edit this in the JSON so the preset list shows something clear.
- **Matching:** Each mapping has `controller_name` and `controller_location` from OpenRGB. When you add from preset we match `controller_name` to the user's detected devices. So the same preset works for anyone with that device—OpenRGB reports the same device name. Export from a PC that has the device so the JSON gets the exact `controller_name` OpenRGB uses.
- **Display names when added:** With brand and model set: single device is Device (e.g. Corsair SP120 RGB PRO); multiple is Device 1, 2, 3. Without brand/model (e.g. motherboard onboard LED): Category - ControllerName - Name (e.g. Motherboards - MSI MPG X570 - Onboard LED). “MSI MPG X570 GAMING EDGE WIFI AM4 ATX Motherboard”). For preset creation, set `"name"` in the JSON to something short like “Onboard LEDs”. When others (or you) add from preset, they get “MSI MPG X570… - Onboard LEDs” and can tell which motherboard it is.
