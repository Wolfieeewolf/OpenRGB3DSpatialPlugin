# Quick Start - OpenRGB 3D Spatial Plugin

This guide walks through the minimum steps required to get spatial effects running. It assumes you already have OpenRGB and the plugin installed.

## 1. Calibrate the Room

1. Pick a reference corner: front-left floor corner when you face the room. That point is `(0,0,0)`.
2. Measure the room width (X), depth (Y), and height (Z) in millimetres. Centimetres x 10 works fine.
3. In the **3D Spatial** tab open **Grid Settings**:
   - Enable **Use Manual Room Size**.
   - Enter the X/Y/Z measurements.
   - Optional: adjust **Grid Scale (mm/unit)** if you prefer another unit; default 10 mm keeps coordinates easy.

Tip: Measurements only need to be within a few centimetres. Perfect accuracy is unnecessary for most effects.

## 2. Place Devices

For each controller you load into the scene:

1. Add it from **Available Controllers**.
2. With the device selected, set its centre position:
   - Measure the device width/depth/height.
   - Measure the distance from the origin corner to the device's front-left-bottom corner.
   - Centre coordinates (in mm) are:
     - `X = corner_offset_x + width / 2`
     - `Y = corner_offset_y + depth / 2`
     - `Z = corner_offset_z + height / 2`
   - Convert to grid units (divide by grid scale) and enter into **Position X/Y/Z**.
3. Use the viewport gizmo for fine adjustments or rotation if needed.

Use **Grid Snap** when aligning multiple devices along shelves or desks.

## 3. Verify the Layout

- Check that every controller appears where you expect in the viewport.
- Toggle **Use Manual Room Size** off/on to see the difference between automatic and measured bounds.
- If LEDs look mirrored, double-check which side you measured from and flip the appropriate axis.

## 4. Run an Effect

1. Switch to the **Effects** tab.
2. Choose an effect (for example, **Wave 3D**) and a zone or "All Controllers".
3. Click **Start Effect** and confirm the animation sweeps across the room as expected.

If the effect only covers part of the room, revisit the room dimensions or device positions.

## Troubleshooting Cheatsheet

| Symptom | Likely Cause | Quick Fix |
| --- | --- | --- |
| Device appears underground | Z centre too low | Raise Position Z by desk height / grid scale |
| Effect stops halfway across room | Room width/depth too small | Re-measure and update Grid Settings |
| LEDs animate diagonally instead of front-to-back | Measured from wrong wall | Swap X/Y values or rotate controller |
| Nothing moves | Effect timer was stopped | Hit **Start Effect** or check plugin log |

## Next Steps

- Define reference points (user, speakers, TV) for origin-aware effects.
- Group controllers into zones for targeted animations.
- Explore the **Effect Stack** tab to layer multiple spatial effects.

For deeper detail see:
- `docs/guides/GridSDK.md` for the SDK surface
- `docs/guides/ContributorGuide.md` for development workflows

