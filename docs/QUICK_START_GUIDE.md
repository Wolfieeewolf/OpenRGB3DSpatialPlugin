# Quick Start: Set Up a Custom 3D Controller Grid

This guide shows how to measure your layout and map your LEDs onto the 3D grid in the Custom Controller dialog.

## Measure Your Setup First
- Count LEDs along each axis:
  - Width (X): number of LEDs left → right on a layer.
  - Height (Y): number of LEDs top → bottom on a layer.
  - Depth (Z): number of stacked layers (use 1 if it’s a single plane).
- Measure center-to-center spacing (in millimeters):
  - Spacing X: distance between adjacent LEDs horizontally.
  - Spacing Y: distance between adjacent LEDs vertically.
  - Spacing Z (if multiple layers): physical gap between layers.
- Note origin and orientation:
  - Assume X grows left → right and Y grows top → bottom.
  - “Layer 0” is the front/nearest layer; higher indices go back.

## Create the Grid
- Open the plugin tab → Custom Controllers → Create New Custom Controller.
- Name it clearly (e.g., “Case Front 8×14×1”).
- Set Grid Dimensions:
  - Width, Height, Depth to match your LED counts.
  - Set Spacing X/Y/Z to your measured distances (mm).
- Use the Layer tabs to switch between Z layers when Depth > 1.

## Map LEDs to Grid Cells
- Select a source on the left:
  - Granularity: choose Whole Device, Zone, or individual LED.
  - Pick an item from the list (items show average color to help identify).
- Click a grid cell to select it, then click “Assign to Selected Cell”.
- To correct a cell: use “Clear Selected Cell”.
- To start over: “Remove All LEDs from Grid”.
- Tip: map a few LEDs first, verify orientation, then complete the rest.

## Orient and Calibrate (Preview + Bake)
- Use Transform Grid Layout on the right to match physical orientation:
  - Rotate: free-angle or quick 90°/180°/270° buttons.
  - Flip: Horizontal / Vertical.
- For preview-only changes that you want to bake in:
  - Turn on “Lock Effect Direction (preview-only)”.
  - Apply rotation/flip presets as needed for a correct preview.
  - Click “Apply Preview Remap” to commit the preview into the mapping.
- Re-check that animations move in the expected physical direction.

## Save and Use
- Click “Save Custom Controller”.
- Your controller appears under Available Custom Controllers for use in transforms and effects.

## Quick Troubleshooting
- Grid is mirrored or rotated wrong:
  - Use Rotate 90°/180°/270° and Flip Horizontal/Vertical.
  - If you used Lock Effect Direction, remember to “Apply Preview Remap”.
- Colors/segments don’t match device:
  - Verify you assigned the right Zone/LEDs; re-assign specific cells.
- Some LEDs don’t respond:
  - Confirm device connectivity in OpenRGB; ensure the item isn’t assigned elsewhere.
- Spacing looks off in 3D:
  - Re-measure center-to-center spacing; update Spacing X/Y/Z in mm.

---
Tip: Keep a small paper sketch of your matrix with LED indices. Map a few indices to grid cells, test an effect, then finish the mapping for fewer redo cycles.

## Device Reference Point (What To Measure To)
- By default, devices are centered in their own local space. The plugin centers LED layouts so the transform origin is the device’s LED centroid.
- Recommendation: measure to the physical center of the device (or its LED centroid for irregular layouts). This will match the default origin the best.
- If your device is very asymmetric, you can still measure to the center and then nudge Position X/Y/Z a few units until LEDs visually line up in the viewport.

## Map A Room: Real‑World To Grid
- Choose a coordinate system:
  - Units: use millimeters (mm) for consistent spacing.
  - Origin: front‑left‑floor corner of the room (default in the UI).
  - Axes (world space): X = left → right, Y = front → back (depth), Z = floor → ceiling.
- Measure physical spans with a tape or laser:
  - Room width (X) and height (Y).
  - For perimeter runs, measure each wall segment length between corner LEDs.
  - Note where controllers, power injection, and breaks occur.
- Determine LED counts and spacing:
  - If using a known pitch strip (e.g., 60 LEDs/m), spacing ≈ 1000 / 60 = 16.67 mm.
  - If LEDs are custom‑placed, spacingX ≈ spanX / (countX − 1), spacingY ≈ spanY / (countY − 1).
  - Keep spacing consistent across segments if you want straight visual motion.
- Decide grid dimensions:
  - Width = total columns you want to represent along the run; Height = rows (often 1 for a strip or the number of vertical LEDs on a wall). Depth = 1 unless you model separate layers (e.g., floor vs ceiling as different Z).
  - For a continuous perimeter, either:
    - Model as a single long 1×N grid (simple), or
    - Model each wall as its own row/segment and connect via transforms (preserves geometry).
- Place controllers and zones:
  - Create one Custom Controller per physical controller for clarity, or use Granularity (Device/Zone/LED) to split a single device across cells.
  - Use “Assign to Selected Cell” to place the first LED of each run at the measured origin.
  - Continue assigning LEDs in physical order along X (and Y if vertical runs).
- Calibrate orientation:
  - Run a test effect. If motion is reversed or rotated, use Rotate/Flip.
  - Use “Lock Effect Direction (preview‑only)” + “Apply Preview Remap” to bake the orientation once it looks correct.
- Validate and iterate:
  - Check corners: the effect should turn correctly across wall transitions.
  - If spacing looks off (stretched/squashed), refine Spacing X/Y to match measured center‑to‑center distances.

### Worked Example (Perimeter Strip)
- Room 4.0 m × 3.0 m, one continuous strip around the ceiling perimeter, 60 LEDs/m.
- Total LEDs ≈ (4+3+4+3) m × 60 = 840 LEDs.
- Spacing X ≈ 1000/60 = 16.67 mm. Use Height = 1, Width = 840, Depth = 1.
- Set origin to front‑left ceiling corner. Assign the first LED to grid (0,0) and continue along X in the travel direction.
- If the effect travels the wrong way around the room, Flip Horizontal or Rotate 180°, then “Apply Preview Remap”.
