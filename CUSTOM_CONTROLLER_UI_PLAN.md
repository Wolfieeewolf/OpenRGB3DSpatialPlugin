# Custom Controller UI — Deep Dive & Improvement Plan

This document summarizes the current custom-controller flow, identifies pain points, and proposes a prioritized plan to make creation and management fully functional and easy to use.

---

## 1. Current Architecture & Flow

### 1.1 Where custom controllers live

- **VirtualController3D**: In-memory representation (name, grid W×H×D, LED mappings, spacing mm). Persisted to `custom_controllers/*.json` via `SaveCustomControllers()` / `LoadCustomControllers()`.
- **ControllerTransform**: Represents a controller (physical or virtual) placed in the 3D scene. For custom controllers, `controller == nullptr` and `virtual_controller` points to a `VirtualController3D`.

### 1.2 UI entry points

| Location | Widget | Purpose |
|----------|--------|---------|
| **Object Creator** tab | "Object Type" → Custom Controller | Shows **Custom Controllers** list (library), Create / Import / Export / Edit |
| **Object Creator** tab | Custom Controllers list | List of all saved custom controllers (by name). Select one to Edit or Export. |
| **Left panel** | "Available Controllers" tab | List of physical devices + custom controllers *not yet in the 3D scene* + ref points + display planes |
| **Left panel** | "Controllers in 3D Scene" | List of items currently in the viewport (physical, custom, ref points, displays) |
| **Left panel** | "Add to 3D View" | Adds the *selected item* from Available Controllers (e.g. a custom controller) to the scene |

### 1.3 Create flow (current)

1. User goes to **Object Creator** → selects "Custom Controller" → sees **Available Custom Controllers** list.
2. Clicks **Create New Custom Controller** → `CustomControllerDialog` opens.
3. In dialog: enter name; pick **Available Controllers** (physical device); choose **Select** (Whole Device / Zone / LED); pick item from dropdown; set **Grid Dimensions** (W, H, D) and **LED Spacing** (X, Y, Z mm); use **Layer** tabs and grid table to assign LEDs to cells (click cell → Assign / Clear).
4. Optional: **Transform Grid Layout** — Lock, rotate, flip, then **Apply Preview Remap** to commit.
5. **Save Custom Controller** → dialog accepts; new `VirtualController3D` is created and appended to `virtual_controllers`; list is updated; user is told they can "add it to the 3D view."
6. To actually see it in 3D: user must go to **Available Controllers**, find the new **[Custom] &lt;name&gt;** entry, select it, click **Add to 3D View**.

### 1.4 Edit flow (current)

1. In **Object Creator** → Custom Controller → select a controller from **Available Custom Controllers**.
2. Click **Edit** → same `CustomControllerDialog` opens with **Edit Custom 3D Controller** title; `LoadExistingController()` fills name, dimensions, spacing, and mappings.
3. User changes grid/mappings as needed; **Save** replaces the virtual controller in place and retargets any `ControllerTransform` that pointed at the old instance (so instances in the scene update).

### 1.5 What already works well

- Create / Edit / Import / Export of custom controllers.
- Grid assignment with Whole Device / Zone / LED granularity; cell tooltips and multi-LED display.
- Layer tabs for depth; dimension and spacing controls.
- Transform tools (rotate 90/180/270, flip H/V, lock + preview remap).
- Color refresh from OpenRGB for grid preview.
- Null checks for `resource_manager` in dialog; selection sync and clear handling in main tab.

---

## 2. Pain Points & Gaps

### 2.1 UX and flow

| Issue | Detail |
|-------|--------|
| **Two-step “create then add”** | After creating a custom controller, the user must remember to go to **Available Controllers**, find **[Custom] &lt;name&gt;**, and click **Add to 3D View**. No “Add to scene now?” after Save; no auto-selection in Available Controllers. |
| **Object Creator vs Available Controllers** | “Available Custom Controllers” (library) vs “Available Controllers” (things that can be added to the scene) use similar wording; users can confuse the two. |
| **No delete from library** | There is no way to delete a custom controller from the **Custom Controllers** list. Only option is Replace on Import. Users cannot remove mistaken or obsolete definitions. |
| **Transform lock is hard to discover** | “Lock Effect Direction (preview-only)” and “Apply Preview Remap” are powerful but not explained in the UI. Users may not understand when to lock, what “preview” means, or that they must click Apply to commit. |

### 2.2 Data and validation

| Issue | Detail |
|-------|--------|
| **Ghost mappings when shrinking grid** | If the user reduces **Width** or **Height**, mappings with `x >= new_width` or `y >= new_height` remain in `led_mappings` but no longer appear in the grid. They are still saved and can confuse edit/export. Same if **Depth** is reduced: mappings in removed layers stay in memory. |
| **No cleanup on dimension change** | `on_dimension_changed()` → `RebuildLayerTabs()` + `UpdateGridDisplay()`; mappings are not clamped or removed. Recommendation: when W/H/D decrease, remove or clamp mappings that fall outside the new bounds (and optionally warn). |
| **Save validation is minimal** | Save only requires non-empty name and at least one LED. No check for “all mappings within current grid bounds” (so ghost mappings can be saved). |
| **Duplicate name on create** | Creating a second custom controller with the same name overwrites the first (same filename). No “name already exists” warning or rename prompt. |

### 2.3 Dialog layout and clarity

| Issue | Detail |
|-------|--------|
| **Dense, single-screen layout** | All controls (name, controllers list, granularity, item combo, dimensions, spacing, layer tabs, grid, transform group, buttons) on one 1000×600 dialog. Works but can feel overwhelming. |
| **No short help or steps** | No “Step 1: Name and grid size”, “Step 2: Assign LEDs”, etc. New users may not know the order of operations. |
| **Cell selection vs “Assign to Selected Cell”** | Users must click a cell, then click **Assign to Selected Cell**. No drag-from-list-to-cell or double-click-to-assign; discoverability is low. |
| **Item combo hides assigned items** | The item dropdown hides controller/zone/LED if that item is already assigned anywhere on the grid. Intent is to avoid double-use, but it prevents “same zone in multiple regions” (e.g. a strip repeated). Optional: allow re-use with a “Allow same zone/LED in multiple cells” option. |

### 2.4 Robustness and edge cases

| Issue | Detail |
|-------|--------|
| **Edit when controller is in scene** | Edit flow correctly retargets transforms; if the user deletes LEDs or shrinks the grid in edit, any transform using that virtual controller still updates. Good. Ensure we never leave dangling pointers when replacing the virtual controller. |
| **Import with missing/mismatched physical device** | `VirtualController3D::FromJson` uses `controllers` to resolve controller pointers. If the imported file was created on another machine or device list changed, some mappings may have null controller. Dialog and runtime should handle null controller (e.g. “Unknown device” in tooltip, skip in color preview). |
| **Unicode in grid tooltips** | Code uses `"â€¢"` and `"â—"` — likely mojibake for bullet (U+2022) and circle (U+25CF). Should use proper UTF-8 or `QString::fromUtf8` / `\u2022` for portability. |

### 2.5 Object Creator tab structure

| Issue | Detail |
|-------|--------|
| **Object type dropdown then stack** | “Select to Create...” then Custom Controller / Reference Point / Display Plane is clear. The Custom Controller page could have a short description: “Create a grid of LEDs from your physical devices, then add it to the 3D view.” |
| **Custom Controllers list empty state** | When the list is empty, only “Create New Custom Controller” and Import are obvious. A one-line “No custom controllers yet. Create one or import from file.” would help. |

---

## 3. Prioritized Plan

### Phase 1 — Critical UX & robustness (do first)

1. **Post-create: “Add to 3D View” option**  
   After **Save** in Create flow, show a dialog or inline choice: “Add [name] to 3D view now?” If Yes, call the same logic as **Add to 3D View** (select the new custom controller in Available Controllers and add it to the scene), then optionally switch to the main tab or focus the 3D scene list. This removes the easy-to-miss second step.

2. **Delete custom controller from library**  
   Add a **Delete** (or **Remove**) button next to Edit/Export on the Custom Controllers page. On click: if the controller is in the scene, warn “This controller is in the 3D scene. Remove from scene first?” and either remove from scene then delete, or only delete from library and remove from scene. Then delete from `virtual_controllers`, refresh lists, and persist (`SaveCustomControllers`).

3. **Clamp or remove out-of-bounds mappings**  
   When **Width**, **Height**, or **Depth** is decreased in the dialog:
   - Remove (or optionally “move to nearest edge”) any mapping where `x >= new_width`, `y >= new_height`, or `z >= new_depth`.
   - After cleanup, call `UpdateGridDisplay()` / `UpdateCellInfo()` / `UpdateItemCombo()`.
   - Optionally show a brief message: “X mappings were outside the new grid and were removed.”

4. **Duplicate name handling**  
   On **Save** (Create): if a custom controller with the same name already exists, prompt “A custom controller named ‘X’ already exists. Replace it or enter a different name?” with Replace / Cancel / “Choose new name” (re-open name field or small dialog). On **Save** (Edit): allow same name (no change) or new name; if new name matches another, same logic.

5. **Fix tooltip Unicode**  
   Replace `"â€¢"` / `"â—"` with proper bullet (U+2022) and circle (U+25CF) (e.g. `QString::fromUtf8("\xE2\x80\xA2")` or `\u2022` in a string that’s UTF-8). Ensures tooltips display correctly on all platforms.

### Phase 2 — UI polish & clarity

6. **Short help text in Custom Controller dialog**  
   At the top of the dialog (under or next to the name field), add one or two lines: “Select a physical controller and an item (device/zone/LED), then click a grid cell and **Assign to Selected Cell**. Use layers for 3D depth.”

7. **Transform lock / preview explanation**  
   In the **Transform Grid Layout** group, add a line: “Lock to preview rotation/flip without changing the grid size; click **Apply Preview Remap** to apply.” Or a small “?” tooltip on the Lock checkbox.

8. **Object Creator Custom Controller page**  
   - Under “Available Custom Controllers”, if the list is empty: show placeholder text “No custom controllers yet. Create one or import from file.”  
   - Subtitle or label: “These can be added to the 3D view from the **Available Controllers** list.”

9. **Dialog layout tweaks (optional)**  
   - Group “Step 1: Name & grid” (name, width, height, depth, spacing) and “Step 2: Assign LEDs” (controllers list, granularity, item combo, grid) with group boxes or labels.  
   - Consider making the grid area resizable or giving it more space relative to the left column.

10. **Save validation**  
    Before `accept()` in the dialog: ensure every mapping has `x in [0, width)`, `y in [0, height)`, `z in [0, depth)`. If any are out of bounds (e.g. left from a previous shrink), strip them and then save, and optionally show “Some invalid mappings were removed.”

### Phase 3 — Enhancements (later)

11. **Double-click or drag to assign**  
    Double-click on a grid cell to assign the current “item” from the dropdown (same as Assign to Selected Cell). Optional: drag from item combo (or a small icon list) onto a cell to assign.

12. **Allow same zone/LED in multiple cells**  
    Add a checkbox “Allow reusing the same zone/LED in multiple cells” (e.g. for strips). When unchecked, keep current behavior (hide assigned items). When checked, show all items and allow multiple cells to map to the same zone/LED.

13. **Import robustness**  
    In `VirtualController3D::FromJson` and in the dialog when loading an imported controller: if a mapping’s device/zone/LED is not found (e.g. null controller), mark it as “Unknown device” in tooltip and skip it in color preview; optionally show “X mappings could not be matched to current devices.”

14. **Keyboard shortcuts**  
    In the dialog: e.g. Ctrl+S = Save, Escape = Cancel, arrow keys to move selected cell (then Assign/Clear with a shortcut).

15. **Preview in 3D (stretch)**  
    Optional “Preview in viewport” from the dialog: temporarily add the current grid to the scene as a non-saved transform so the user can check scale/position before saving. Removed on Cancel or on closing the dialog without Save.

---

## 4. Implementation Notes

- **CustomControllerDialog**: Keep a single dialog for both Create and Edit; avoid duplicating logic. All “post-save” behavior (e.g. “add to scene?”) should be handled in the caller (`on_create_custom_controller_clicked` / `on_edit_custom_controller_clicked`).
- **Controller list sync**: When deleting a custom controller from the library, update `custom_controllers_list`, `available_controllers_list`, and `controller_list`; remove any `ControllerTransform` that referenced that virtual controller (same pattern as in the Import-replace flow).
- **Persistence**: Any change to `virtual_controllers` (create, edit, delete, import) should call `SaveCustomControllers()` so the library stays in sync with disk.
- **Null controller in mappings**: In `CustomControllerDialog` (tooltips, color, `UpdateItemCombo`) and in `VirtualController3D::GenerateLEDPositions`, guard against `mapping.controller == nullptr` so we never dereference null.

---

## 5. Summary

| Priority | Focus | Key items |
|----------|--------|------------|
| **Phase 1** | Critical UX & robustness | Add to 3D after create; Delete from library; clamp mappings on shrink; duplicate-name handling; fix tooltip Unicode |
| **Phase 2** | Polish & clarity | Help text in dialog; transform lock explanation; empty state and subtitle on Custom Controllers page; optional layout/grouping; save validation |
| **Phase 3** | Nice-to-have | Double-click/drag assign; allow re-use of zone/LED; import resilience; shortcuts; optional 3D preview |

Implementing Phase 1 and Phase 2 makes custom controller creation and management fully functional, predictable, and easier to understand; Phase 3 adds discoverability and resilience.

---

## 6. Implementation Status

All phases have been implemented:

- **Phase 1** — Post-create “Add to 3D view now?”; Delete from library (with scene handling); out-of-bounds mappings removed on dimension shrink; duplicate-name prompt (Replace/Cancel) on Create and Edit; tooltip Unicode (bullet U+2022, circle U+25CF).
- **Phase 2** — Step-based help text in dialog; transform lock/preview explanation; Step 1 / Step 2 group boxes; subtitle and empty-state label on Custom Controllers page; save validation (strip out-of-bounds mappings before accept).
- **Phase 3** — Double-click grid cell to assign; Ctrl+S / Escape / Delete shortcuts; currentCellChanged sync for keyboard navigation; “Allow reusing same zone/LED in multiple cells” checkbox; import robustness (unknown device in tooltips, null-safe ToJson/GenerateLEDPositions, import feedback); “Preview in 3D View” button (temporary transform, cleared on dialog close).
