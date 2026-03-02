# Naming and consistency

Suggestions for UI label renames and effect parameter naming so the plugin reads clearly and consistently.

---

## 1. Main UI labels (tabs, panels, buttons)

| Current | Suggestion | Notes |
|--------|------------|--------|
| **Run** (left tab) | Keep or "Effects" | "Run" = daily use (profile + start). "Effects" = same content, might be clearer for new users. |
| **Setup** (left tab) | Keep or "Layout" | "Setup" = controllers, grid, objects. "Layout" = same idea. |
| **Profile:** (Run panel) | Keep | Clear. |
| **Effect Library** | Keep | Clear. |
| **Effect Layers** (group) | Keep | Good. |
| **Active Effect Stack** | **Layers** | Shorter; list is the layers. |
| **Effect Configuration** (right panel) | **Layer settings** | Makes clear it's settings for the *selected layer*. |
| **Add To Stack** | **Add to stack** | Sentence case for buttons (optional). |
| **- Remove Effect** | **Remove layer** | Matches "layer" wording; drop leading "-". |
| **Start all** / **Stop all** | Keep | Clear. |
| **Available Controllers** | Keep | Clear. |
| **Controllers in 3D Scene** | Keep | Clear. |
| **Add to 3D View** | Keep | Clear. |
| **Position & Rotation** | **Scene object: position & rotation** (or keep + hint) | This tab moves *controllers, reference points, display planes* in the room (layout). It does *not* change the camera view. Making that explicit avoids confusion with viewport rotate/pan/zoom. |
| **Camera:** (viewport hint) | **View (camera only):** or **3D view – camera only** | Viewport rotate/pan/zoom only change *how you look* at the scene; they do not change the effect or layout. Label so users know it's "view only". |
| **Origin:** (Layer settings) | **Effect origin:** (optional) or keep + tooltip | Center point for the *effect* (Room Center or a reference point). Distinct from scene object position. |
| **Grid Settings** | Keep | Clear. |
| **Object Creator** | Keep | Clear. |
| **Layout Profile** / **Effect Profile** (Profiles tab) | Keep | Clear. |
| **Open plugin config folder** | Keep | Clear. |

---

## 2. View vs layout vs effect (rotation, position, scale)

It’s easy to mix up three different concepts:

| Concept | What it changes | Where it is in the UI |
|--------|------------------|------------------------|
| **View (camera)** | Only how you look at the 3D scene. Rotate/pan/zoom the view. **Does not change the effect or layout.** | 3D viewport: right mouse = rotate view, left drag = pan, scroll = zoom. Hint label above viewport. |
| **Scene object position & rotation** | Where *controllers, reference points, display planes* sit in the room (your layout). Changes where LEDs are in 3D. | Setup → "Position & Rotation" tab. Select an item in "Controllers in 3D Scene" (or ref points, display planes), then edit position/rotation. |
| **Effect origin** | Center point for an *effect* (e.g. Room Center or a reference point). Affects how the effect is positioned when rendering. | Layer settings (right panel when a layer is selected): "Origin" combo. |

**Naming and hints:**

- **Viewport:** Label the camera hint so it’s clear it’s "view only", e.g. "View (camera only – does not change effect or layout): Right mouse = Rotate view | Left drag = Pan | Scroll = Zoom | Left click = Select/Move objects."
- **Position & Rotation tab:** Either rename to "Scene object: position & rotation" or add a line: "Moves the selected controller, reference point, or display plane in the room. Does not change the camera view."
- **Transform help (in that tab):** Add that these values move the *selected item* in the room (layout), not the camera.
- **Origin (layer settings):** Tooltip or label "Effect origin" so it’s clear it’s the center point for *the effect*, not the view or a scene object.

(Some effects also have internal rotation/scale in their sampling; that’s effect-specific and separate from both view and scene object transform.)

**Slider grouping and consistent naming:**

- **Group by purpose:** Put "scene object" position sliders in one group (e.g. **Scene object position (mm)**) and rotation in another (**Scene object rotation (°)**). That way it's obvious these are for the selected object in the room, not the camera or the effect.
- **Row labels:** When the group title states the quantity and units, use short row labels **"X:", "Y:", "Z:"** to avoid repeating "Position X (mm):" etc.
- **Same pattern everywhere:** For any transform controls (position, rotation, scale, offset), use the same pattern: a **group title** that states what it is and units (e.g. "Scene object position (mm)", "Scene object rotation (°)"), then **X / Y / Z** (or equivalent) inside. If we ever add view/camera sliders, group them under **View (camera)** so they're clearly view-only. If an effect has its own position/rotation/scale/offset, label the group **Effect position** or **Effect transform** so it's not confused with scene object or view.

**Effect global settings (layer settings panel):** In the effect config (Run → right panel when an effect is selected), use **Effect …**-prefixed names so it’s clear these apply to the effect, not the view or scene:
- **Effect center:** Combo for where the effect is centered (Room Center or reference point). Label was "Effect origin"; tooltip clarifies it doesn’t move camera or scene objects.
- **Effect scale (X / Y / Z %):** Scale the effect along each axis; row labels X:, Y:, Z:.
- **Effect scale rotation (°):** Rotate the scale axes (yaw, pitch, roll). Different from effect rotation.
- **Effect position offset (%):** Offset the effect center from its origin; row labels X:, Y:, Z:.
- **Effect rotation (°):** Rotate the effect around its center; row labels Yaw:, Pitch:, Roll:.
- **Effect path / plane:** Axis and plane for the effect (e.g. wave direction); tooltip clarifies not camera or layout.

---

## 3. Effect parameter labels (sliders, combos, etc.)

Effects build their own UI in `SetupCustomUI()` with `QLabel` + control. Current mix:

- **Capitalization:** Some "Title Case" (e.g. "Core Radius:", "Char Height:"), some "Sentence case" (e.g. "Beam width:", "Wave style:").
- **Colons:** Almost all use a trailing colon; keep that.
- **Units:** Sometimes in the label ("Arches/sec:", "Day length (min):"), sometimes only in the value ("%", "x"). Both are fine; prefer units in the label when it helps ("Flash rate (Hz)" vs just "Flash rate" with value "5").

**Suggested convention:**

- **Label style:** Sentence case for parameter names: "Beam width", "Ring thickness", "Peak boost". (Or Title Case everywhere; pick one and stick to it.)
- **Units:** Put units in the label when they’re fixed: "Arches/sec", "Day length (min)". For %, put in value or label: "Edge width (%)" or show "50%" next to slider.
- **Consistency across effects:** Same concept, same word: e.g. "Smoothing", "Falloff", "Peak boost" in audio effects; "Arms" in spiral/spin; "Thickness" vs "Ring thickness" vs "Beam thickness" – prefer one term per concept where possible.

**Examples of normalizations (optional):**

| Effect(s) | Current | Normalized (example) |
|-----------|---------|----------------------|
| Several | "Peak Boost:" | "Peak boost:" (sentence case) |
| Matrix3D | "Char Height:", "Char Gap:", "Char Variation:", "Char Spacing:" | "Char height:", "Char gap:", etc. |
| Spiral3D | "Gap Size:" | "Gap size:" |
| Wipe3D | "Edge Shape:" | "Edge shape:" |
| Tornado3D | "Core Radius:", "Height:" | "Core radius:", "Height:" |
| DNAHelix3D | "Helix Radius:" | "Helix radius:" |
| BouncingBall3D | "Ball Size:", "Balls:" | "Ball size:", "Ball count:" (if "Balls" is count) |
| Lightning3D | "Arches/sec:", "Max arcs:" | Keep or "Strike rate (per sec):", "Max arcs:" |
| FreqFill3D, etc. | "Edge Width:", "Smoothing:" | "Edge width:", "Smoothing:" (sentence case) |

No need to change behaviour or JSON keys; only the visible label strings.

---

## 3. What’s already consistent

- **Colons** on parameter labels.
- **Audio effects:** "Smoothing", "Falloff", "Peak Boost" repeated in many effects.
- **Generic terms:** "Mode", "Style", "Type", "Direction" used sensibly.
- **Group titles:** "Effect Library", "Effect Layers", "Layout Profile", "Effect Profile" are clear.

---

## 5. Implementation notes

- **UI strings:** Main tab and panel labels are in `OpenRGB3DSpatialTab.cpp`, `OpenRGB3DSpatialTab_EffectStack.cpp`, `OpenRGB3DSpatialTab_Profiles.cpp`, etc. Search for the current string to rename.
- **Effect parameters:** Each effect’s `SetupCustomUI()` in `Effects3D/<Name>/<Name>.cpp` adds its own labels; normalize per effect in a separate pass.
- **i18n:** If you add translation later, these strings will move into `tr()`; renames still apply to the source English text.

A first pass can apply only the main UI renames (e.g. "Active Effect Stack" → "Layers", "Effect Configuration" → "Layer settings", "- Remove Effect" → "Remove layer"); effect parameter cleanup can be done gradually per effect.
