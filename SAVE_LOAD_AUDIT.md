# Save/Load Audit for OpenRGB 3D Spatial Plugin

This document audits all plugin state persistence to ensure correctness and completeness.

## Persistence Systems

### 1. **SettingsManager** (JSON, per-plugin key)
- **File**: `~/.config/OpenRGB/OpenRGB.json` (or Windows equivalent)
- **Key**: `"3DSpatialPlugin"`
- **Access**: `GetPluginSettings()` / `SetPluginSettings()` / `SetPluginSettingsNoSave()`
- **When**: `SetPluginSettings()` calls `SaveSettings()` immediately; use `SetPluginSettingsNoSave()` for batching

### 2. **Layout Profiles** (File-based, user-managed)
- **Folder**: `config_dir/plugins/settings/OpenRGB3DSpatialPlugin/layouts/`
- **Format**: `.json` files (one per profile)
- **Functions**: `SaveLayout()` / `LoadLayout()` / `LoadLayoutFromJSON()`
- **Contains**: Controllers, transforms, reference points, display planes, zones, effect stack, grid settings, room dimensions, camera

### 3. **Effect Profiles** (File-based, user-managed)
- **Folder**: `config_dir/plugins/settings/OpenRGB3DSpatialPlugin/effect_profiles/`
- **Format**: `.json` files (one per profile)
- **Functions**: `SaveEffectProfile()` / `LoadEffectProfile()`
- **Contains**: Effect stack only (instances, settings, order)

### 4. **Custom Controllers** (Folder, auto-saved)
- **Folder**: `config_dir/plugins/settings/OpenRGB3DSpatialPlugin/custom_controllers/`
- **Format**: `.json` files (one per custom controller)
- **Functions**: `SaveCustomControllers()` / `LoadCustomControllers()`
- **When**: Called after add/remove/edit of custom controllers

### 5. **Stack Presets** (Folder, user-managed)
- **Folder**: `config_dir/plugins/settings/OpenRGB3DSpatialPlugin/stack_presets/`
- **Format**: `.json` files (one per preset)
- **Functions**: `LoadStackPresets()` / `SaveStackPresets()`
- **Contains**: Effect stack configurations (reusable templates)

### 6. **OpenRGB Profile Integration** (NEW in API v5)
- **Functions**: `OnProfileSave()` / `OnProfileLoad()` / `OnProfileAboutToLoad()`
- **When**: Called when user does Save/Load Profile in main OpenRGB
- **Should contain**: Layout state (or reference to layout profile)

---

## Audit Checklist

### ✅ Layout (File-based profiles)

**Save paths:**
- [x] "Save Layout Profile" button → `SaveLayout(layout_path)` ✓
- [x] Layout includes: controllers, transforms, reference points, display planes, zones, effect stack, grid, room, camera ✓

**Load paths:**
- [x] "Load Layout Profile" button → `LoadLayout(layout_path)` ✓
- [x] Auto-load on startup → `TryAutoLoadLayout()` (if enabled) ✓
- [x] `LoadLayoutFromJSON()` restores all sections ✓

**Settings persistence:**
- [x] Selected profile name → `SaveCurrentLayoutName()` → plugin settings `"SelectedProfile"` ✓
- [x] Auto-load enabled → plugin settings `"AutoLoadEnabled"` ✓
- [x] Restored on tab show → `TryAutoLoadLayout()` ✓

**Issues found:**
- None

---

### ⚠️ Custom Controllers

**Save paths:**
- [x] After add from preset → `SaveCustomControllers()` ✓
- [x] After edit (CustomControllerDialog OK) → `SaveCustomControllers()` ✓
- [x] After delete → `SaveCustomControllers()` ✓
- [x] After import → `SaveCustomControllers()` ✓

**Load paths:**
- [x] On tab init → `LoadCustomControllers()` ✓

**Issues found:**
- [ ] **TODO**: Verify CustomControllerDialog saves when user edits name, grid size, spacing, or mappings (check if OK button triggers save)

---

### ⚠️ Effect Stack

**Save paths:**
- [x] After add effect → `SaveEffectStack()` ✓
- [x] After remove effect → `SaveEffectStack()` ✓
- [x] After reorder → `SaveEffectStack()` ✓
- [ ] **TODO**: After effect settings change (e.g. color, speed, blend mode) → verify `SaveEffectStack()` is called

**Load paths:**
- [x] Loaded as part of layout → `LoadLayoutFromJSON()` reads `"effect_stack"` ✓

**Settings persistence:**
- [x] Effect instance `saved_settings` → stored in layout JSON ✓
- [x] Restored when loading layout → `LoadSettings()` called on effect ✓

**Issues found:**
- [ ] **TODO**: Audit effect parameter UI (sliders, combos, checkboxes) - do they call `SaveEffectStack()` on change or only on explicit save?

---

### ⚠️ Effect Profiles (File-based)

**Save paths:**
- [x] "Save Effect Profile" button → `SaveEffectProfile(filename)` ✓

**Load paths:**
- [x] "Load Effect Profile" button → `LoadEffectProfile(filename)` ✓
- [x] Auto-load after layout loads → `TryAutoLoadEffectProfile()` (if enabled) ✓

**Settings persistence:**
- [x] Selected effect profile name → `SaveCurrentEffectProfileName()` → plugin settings `"SelectedEffectProfile"` ✓
- [x] Auto-load enabled → plugin settings `"EffectAutoLoadEnabled"` ✓

**Issues found:**
- [ ] **TODO**: Verify effect profile dropdown updates when profiles are saved/deleted

---

### ⚠️ Stack Presets

**Save paths:**
- [ ] **TODO**: When are stack presets saved? Find the "Save Stack Preset" UI and verify it calls `SaveStackPresets()`

**Load paths:**
- [x] On tab init → `LoadStackPresets()` ✓

**Issues found:**
- [ ] **TODO**: Find and verify stack preset save/delete/rename UI

---

### ❌ Reference Points **[BUG FOUND]**

**Save/Load:**
- [x] Saved as part of layout → `SaveLayout()` includes reference points ✓
- [x] Loaded from layout → `LoadLayoutFromJSON()` restores reference points ✓

**Issues found:**
- [x] **BUG**: `SaveReferencePoints()` is a stub - does nothing! Changes to reference points are NOT persisted unless user explicitly saves layout profile.
- [ ] **FIX**: Add auto-save or dirty flag system

---

### ❌ Zones **[BUG FOUND]**

**Save/Load:**
- [x] Saved as part of layout → `SaveLayout()` includes zones ✓
- [x] Loaded from layout → `LoadLayoutFromJSON()` restores zones ✓
- [x] `SaveZones()` called after add/edit/delete ✓

**Issues found:**
- [x] **BUG**: `SaveZones()` is a stub - does nothing! Changes to zones are NOT persisted unless user explicitly saves layout profile.
- [ ] **FIX**: Add auto-save or dirty flag system

---

### ❌ Display Planes **[BUG FOUND]**

**Save/Load:**
- [x] Saved as part of layout → `SaveLayout()` includes display planes ✓
- [x] Loaded from layout → `LoadLayoutFromJSON()` restores display planes ✓

**Issues found:**
- [x] **BUG**: No `SaveDisplayPlanes()` function exists! `on_add_display_plane_clicked()` and `on_remove_display_plane_clicked()` do NOT call any save function. Changes are NOT persisted unless user explicitly saves layout profile.
- [ ] **FIX**: Add auto-save or dirty flag system
- [ ] **TODO**: Verify monitor preset selection updates display plane and persists

---

### ⚠️ Camera & Room Grid

**Save paths:**
- [x] Camera (distance, yaw, pitch, target) → plugin settings `"Camera"` ✓
- [x] Room grid (show, brightness, point size, step) → plugin settings `"RoomGrid"` ✓
- [x] Saved via viewport callback → `SetPluginSettingsNoSave()` (batched) ✓

**Load paths:**
- [x] On tab show → restored from `GetPluginSettings()` ✓

**Issues found:**
- None

---

### ⚠️ Grid Settings & Room Dimensions

**Save/Load:**
- [x] Saved as part of layout → `SaveLayout()` includes grid settings and room dimensions ✓
- [x] Loaded from layout → `LoadLayoutFromJSON()` restores them ✓

**Issues found:**
- [ ] **TODO**: Verify changing grid scale, room size, or grid settings triggers layout save or marks dirty

---

### ❌ OpenRGB Profile Integration (NEW - API v5)

**Current status:**
- [x] `OnProfileSave()` implemented (stub) ✓
- [x] `OnProfileLoad()` implemented (stub) ✓
- [ ] **TODO**: Wire up `OnProfileSave()` to export current layout state
- [ ] **TODO**: Wire up `OnProfileLoad()` to import layout from profile
- [ ] **TODO**: Decide: save full layout or just reference to layout profile name?

---

## Summary of Issues

### 🔴 Critical Bugs (Data Loss)
1. **SaveZones() / SaveReferencePoints() are stubs** - They're called after add/edit/delete but do NOTHING. Changes are lost unless user manually saves layout profile.
2. **Display planes** - No save function called after add/remove. Changes lost unless user saves layout.
3. **Controller transforms** - Position/rotation/scale changes in UI likely don't trigger save (need to verify).

### 🟡 High Priority
4. **OpenRGB Profile API** - Implement full `OnProfileSave()` / `OnProfileLoad()` integration (currently stubs)
5. **Dirty flag system** - Add layout dirty flag; prompt user to save on close/profile switch if dirty

### 🟢 Working Correctly
- ✅ **Effect stack** - Zone/blend changes call `SaveEffectStack()` ✓
- ✅ **Effect settings** - Parameter changes trigger lambda → `SaveEffectStack()` (line 660) ✓
- ✅ **Custom controllers** - Add/edit/delete call `SaveCustomControllers()` ✓
- ✅ **Layout profiles** - Save/load/auto-load working ✓
- ✅ **Effect profiles** - Save/load/auto-load working ✓
- ✅ **Camera/Room grid** - Persisted via plugin settings ✓

### 🔵 To Verify
6. **Stack presets** - Find save/delete UI and verify triggers
7. **Effect profile dropdown** - Verify updates when profiles saved/deleted
8. **Grid/room settings** - Verify changes persist (likely in layout, but may not auto-save)

---

## Recommended Fixes

### Option A: Auto-save on every change (simplest)
- Make `SaveZones()` / `SaveReferencePoints()` call `SaveLayout()` with current profile
- Add `SaveDisplayPlanes()` that calls `SaveLayout()`
- Pro: No data loss, no dirty flag complexity
- Con: Frequent disk writes; no undo

### Option B: Dirty flag + prompt (better UX)
- Add `layout_dirty` flag
- Set dirty when zones/reference points/display planes/transforms change
- Prompt "Save changes?" when loading different profile or closing tab
- Add "Save" button that's enabled when dirty
- Pro: User control, fewer disk writes
- Con: More complex, need to track all change points

### Option C: Hybrid (recommended)
- Auto-save for small changes (zone/ref point add/delete, transform edits)
- Dirty flag for major changes (adding controllers, effect stack)
- Pro: Balance of safety and control
- Con: Need to decide which changes are "small"

---

## Next Steps

1. **Fix critical bugs** - Implement auto-save or dirty flag for zones/reference points/display planes
2. **Implement OnProfileSave/OnProfileLoad** - Full integration with OpenRGB profiles
3. **Verify remaining areas** - Stack presets, grid settings
4. **Test systematically** - Add zone → close plugin → reopen → verify zone persisted
