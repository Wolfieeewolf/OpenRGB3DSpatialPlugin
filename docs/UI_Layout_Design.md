# UI Layout Design: 3D Spatial Plugin

This document describes the current UI layout problem, what other OpenRGB plugins do well, and proposed layout changes so that the viewport is available when editing effects and the "daily" flow (pick profile → Start → minimize) is simple and prominent.

---

## 1. Current layout and the problem

### Structure today

- **Root:** `QTabWidget` (`main_tabs`) with two tabs.
- **Tab 1 – "Effects / Presets"** (default):
  - Horizontal `QSplitter`: **Left** = scroll with Effect Library, Effect Stack, Zones. **Right** = Effect Configuration group + Audio panel.
  - **No viewport on this tab.**
- **Tab 2 – "Setup / Grid"**:
  - Horizontal layout: **Left** = scroll + `left_tabs` (Available Controllers, spacing, "Add to 3D View", Controllers in 3D Scene). **Middle** = viewport (`LEDViewport3D`) + `settings_tabs` (Position & Rotation, Grid Settings, Object Creator, Profiles).
  - **Viewport exists only here.**

### Why it feels disjointed

1. **Editing effects:** User wants to see the grid overlay and effect position while tweaking the stack. That requires the viewport, which is only on Setup. So they must switch to Setup to see the 3D view, then back to Effects to change effect settings → constant tab switching.
2. **Daily use:** Ideal flow is "select effect profile → Start → minimize OpenRGB". Profile and Start are on the Effects tab, but that tab has no viewport; Setup has the viewport but is the "secondary" tab. So the default tab doesn’t match the most common task (run effects with a profile).
3. **Setup vs run:** Controller creation, grid settings, object creator are setup tasks. Once layout and profiles exist, the main use is running a profile. Those two modes are mixed across two tabs without a clear "Run" vs "Setup" separation that keeps the viewport in play for both.

### Viewport is a single widget

The viewport is created once and added to `middle_panel` on the Setup tab. There is no duplicate viewport on the Effects tab. So we cannot "show the same viewport on both tabs" without changing the structure: either one shared layout where the viewport is always visible, or reparenting the viewport when switching tabs (possible but brittle and flickery). The clean approach is **one main layout where the viewport is always visible**.

---

## 2. What other plugins do (reference)

### OpenRGB main app

- **OpenRGBDialog.ui:** Top-level `QTabWidget` (Devices, Information, Settings). **Sub-tabs use `tabPosition: West`** (vertical tabs on the left). Bottom strip: MainButtonsLayout (Toggle LED View, Rescan, Save profile, etc.).
- **Takeaway:** Use **West** for secondary tab bars so the main content is to the right; keep primary actions in a consistent bar (e.g. bottom or top).

### OpenRGB Effects plugin

- **OpenRGBEffectTab.ui:** Single tab, no "Effects vs Setup" split. Layout: **QGridLayout** with:
  - **Left:** `EffectTabs` (`QTabWidget`, **tabPosition West**) – one tab per effect.
  - **Right:** Fixed-width (300px) frame with `DeviceList`.
- **EffectList.ui:** Row with `QToolButton` (start/stop all) + `QPushButton` ("Effects..."). Compact, no custom colours in the main list.
- **EffectTabHeader.ui:** Per-effect header with `QToolButton` (start/stop, rename, close). Standard widgets.
- **Takeaway:** One screen, no top-level tab that hides the main content. Vertical (West) tabs for effect list; device list in a fixed-width panel. Start/stop and "Effects..." are always visible. They don’t have a viewport; we do, so we need viewport + effect stack in one view.

### OpenRGB Visual Map plugin

- **VirtualControllerTab.ui:** **One layout per map tab.** Structure:
  - **Left:** `QScrollArea` with `DeviceList`.
  - **Center:** `QFrame` (StyledPanel, Sunken) containing the **Grid** (the main 2D view). **Center gets the main space.**
  - **Right:** `settingsFrame` with "VMap menu", `gridFrame` (GridOptions), `itemFrame` (ItemOptions). Bottom: `backgroundFrame` (BackgroundApplier).
- **Takeaway:** **Grid (their "viewport") is in the center**, with device list left and settings right. All on one tab. No separate "setup" tab that hides the grid. We can mirror this: viewport center, Run/Setup as left panel mode, right panel = context (effect config or setup settings).

### Styling (all plugins)

- **Frames:** `QFrame::StyledPanel` + `QFrame::Sunken` for content panels (OpenRGB device page, Visual Map grid/settings).
- **No custom themes:** Rely on Qt/OpenRGB look; avoid plugin-specific themes.
- **Effects plugin:** Minimal custom style: ColorPicker uses a small style for the colour button; Shaders has a small tab hover style. Our green/red Start/Stop buttons in the stack are more custom; we could keep them for emphasis or tone them down to standard buttons for consistency.

---

## 3. User flows we want to support

| Phase | Goal | Viewport role |
|-------|-----|----------------|
| **Setup** | Add controllers, place them, set grid/room, create ref points/display planes. | Essential: see and move objects. |
| **Effects** | Build effect stack, set positions/origins, see grid overlay and effect result. | Essential: grid overlay + live effect preview. |
| **Daily** | Choose effect profile → Start → minimize; reopen only to change profile. | Optional: user may not need the viewport at all. |

So:

- **Setup** and **Effects** both need the viewport visible.
- **Daily** should be as few clicks as possible: profile + Start prominent; setup/controller creation can be tucked away (e.g. "Setup" mode or secondary tab).

---

## 4. Proposed layout direction: single view with viewport always visible

### High-level idea

- **Remove** the two top-level tabs (Effects / Presets vs Setup / Grid).
- **One main layout:** `[ Left panel | Viewport (center) | Right panel ]`.
- **Viewport** is always in the center (one widget, never reparented). Same as Visual Map’s grid-in-the-middle.
- **Left panel:** Mode switch **Run** vs **Setup** (e.g. West tab widget or toolbar).
  - **Run:** Effect profile dropdown, Start/Stop, Effect stack list, Zones (compact). This is the "daily" surface.
  - **Setup:** Available controllers, Add to 3D View, Controllers in 3D Scene, spacing, etc. (current Setup left content).
- **Right panel:** Depends on mode (and selection).
  - **Run mode:** Effect Configuration (effect combo, zone, origin, dynamic effect controls) + Audio. Shown when a stack row is selected; otherwise can show a short "Select an effect or start the stack" hint.
  - **Setup mode:** Current `settings_tabs`: Position & Rotation, Grid Settings, Object Creator, Profiles (in a scroll or compact tabs).

Result:

- **Setup phase:** User picks "Setup" on the left, viewport in center, setup controls on the right. No tab switch to "see the scene".
- **Effects phase:** User picks "Run" on the left, viewport in center (with grid overlay), stack on the left, effect config on the right. They see the effect and grid while editing.
- **Daily:** Open plugin → already on "Run" (or remember last mode) → profile dropdown + Start → minimize. Viewport is there if they want it but not required.

### Alternative: keep two tabs but reorder and rename

- **Tab 1 – "Run"** (default): Same single-view layout as above (viewport center, Run left panel, effect config right). So the *content* of the first tab becomes "viewport + run", not "effects without viewport".
- **Tab 2 – "Setup"**: Viewport center again. But then the viewport would need to be in *both* tabs. That implies either:
  - **Stacked widget / same viewport:** One viewport widget; when switching tabs we’d need to reparent it into the active tab’s center. Doable but easy to get wrong (focus, resize, OpenGL context). Not recommended.
  - **Two viewports:** Duplicate state and sync (complex, wasteful). Not recommended.

So the clean path is **no top-level tab split**: one layout with Run | Setup as a **mode** in the left panel, viewport always in the center.

### Optional: collapsible left/right

- Left and right panels could be collapsible (e.g. arrow buttons or splitters) so that for "daily" use the user can collapse to just viewport + a tiny strip (profile + Start). Not required for v1 but keeps the door open.

---

## 5. UI consistency (what to borrow, what to avoid)

- **Use standard Qt/OpenRGB look:** No custom theme. Let the application style drive colours and fonts.
- **Tab position:** Prefer **West** for secondary tab bars (Run | Setup, or settings sub-tabs) so the main content (viewport) is prominent, matching OpenRGB and Effects.
- **Frames:** Use `QFrame::StyledPanel` and `QFrame::Sunken` for panels that contain the viewport or major controls (like OpenRGB and Visual Map).
- **Buttons:** Prefer `QPushButton` and `QToolButton` with standard styling. If we keep green/red for Start/Stop, use them sparingly (e.g. only the main Start/Stop), not everywhere.
- **Grouping:** Keep using `QGroupBox` for "Effect Configuration", "Effect Library", "Layout Profile", etc. Same as current and other plugins.
- **Scroll areas:** Use `setFrameShape(QFrame::NoFrame)` where we want a flat look next to the viewport; keep margins and spacing consistent (e.g. 4–6 px).
- **Spacing:** 6 px between major sections, 4 px inside panels (we already do this in many places).

---

## 6. Implementation notes

- **SetupUI()** in `OpenRGB3DSpatialTab.cpp` currently builds `main_tabs`, then `setup_tab` (with viewport in `middle_panel`), then `effects_tab` (no viewport), then `main_tabs->addTab(effects_tab, ...)` and `main_tabs->addTab(setup_tab, ...)`.
- **Refactor:** Build one root layout (e.g. horizontal layout or one main splitter). Add:
  1. Left panel widget with Run | Setup mode (e.g. `QTabWidget` with `tabPosition == West`, or a stacked widget switched by toolbar buttons).
  2. Center: the same `viewport` we create today (and all its connections).
  3. Right panel: stacked widget or tab widget that shows "effect config + audio" when Run is selected and "settings_tabs" when Setup is selected.
- **No duplicate viewport:** Create viewport once, add it to the center of the single layout. All existing `viewport->...` calls and signals stay valid.
- **Persistence:** Save/restore splitter sizes and which mode (Run vs Setup) was active if desired. Camera/grid/room state is already saved in plugin settings.

---

## 7. Summary

| Current | Proposed |
|--------|----------|
| Two tabs: Effects (no viewport) / Setup (viewport) | One layout: viewport always in center |
| Tab switch needed to see grid while editing effects | Viewport + grid visible in Run mode |
| Profile/Start on first tab but no 3D view there | Run mode: profile + Start on left, viewport center, effect config right |
| Setup on second tab | Setup mode: same viewport, setup controls on left/right |

This gives a single, consistent flow: viewport is always available when it matters; Run vs Setup is a left-panel mode switch; and "select profile → Start → minimize" stays simple with Run as the primary surface.
