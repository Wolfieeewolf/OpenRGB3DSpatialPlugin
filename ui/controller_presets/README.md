# Controller presets (one file per preset)

**Add from preset** in the Object Creator scans this folder. Each `.json` file is one preset (template). Add a preset by adding a file; fix one by editing that file.

At runtime the plugin uses the **config dir** copy:
`.../OpenRGB/plugins/settings/OpenRGB3DSpatialPlugin/controller_presets/`

Copy this folder (or individual files) there to use these presets, or create your own presets in that folder.

## Shipped presets

| File | Device | Category |
|------|--------|----------|
| corsair_sp120_rgb_pro.json | Corsair SP120 RGB PRO | Fans |
| razer_deathadder_elite.json | Razer Deathadder Elite | Mouse |
| amd_wraith_prism.json | AMD Wraith Prism | CPU cooler |
| msi_mpg_x570_gaming_edge_wifi_onboard_led.json | MSI MPG X570 GAMING EDGE WIFI — Onboard LED | Motherboard |

**msi_mpg_x570_gaming_edge_wifi_onboard_led.json** uses `controller_name` "MSI MPG X570 GAMING EDGE WIFI AM4 ATX Motherboard" so it matches OpenRGB for that board. For other motherboards, copy the file and edit `controller_name` in the mappings to match what OpenRGB reports.

See [PRESET_JSON.md](../PRESET_JSON.md) for the JSON format. Matching uses OpenRGB device name, so presets work for anyone with the same device.
