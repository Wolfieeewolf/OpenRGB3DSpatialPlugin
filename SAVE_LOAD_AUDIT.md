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

### ⚠️ Reference Points

**Save/Load:**
- [x] Saved as part of layout → `SaveLayout()` includes reference points ✓
- [x] Loaded from layout → `LoadLayoutFromJSON()` restores reference points ✓

**Issues found:**
- [ ] **TODO**: Verify add/edit/delete reference point triggers layout save or marks dirty

---

### ⚠️ Zones

**Save/Load:**
- [x] Saved as part of layout → `SaveLayout()` includes zones ✓
- [x] Loaded from layout → `LoadLayoutFromJSON()` restores zones ✓

**Issues found:**
- [ ] **TODO**: Verify add/edit/delete zone triggers layout save or marks dirty

---

### ⚠️ Display Planes

**Save/Load:**
- [x] Saved as part of layout → `SaveLayout()` includes display planes ✓
- [x] Loaded from layout → `LoadLayoutFromJSON()` restores display planes ✓

**Issues found:**
- [ ] **TODO**: Verify add/edit/delete display plane triggers layout save or marks dirty
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

### High Priority
1. **Effect settings changes** - Verify effect parameter changes trigger `SaveEffectStack()`
2. **OpenRGB Profile API** - Implement full `OnProfileSave()` / `OnProfileLoad()` integration
3. **Dirty flag system** - Add/edit/delete of reference points, zones, display planes should mark layout dirty or auto-save

### Medium Priority
4. **Stack presets UI** - Find and verify save/delete/rename triggers `SaveStackPresets()`
5. **Effect profile dropdown** - Verify it updates when profiles saved/deleted
6. **CustomControllerDialog** - Verify OK button saves changes

### Low Priority
7. **Grid/room settings** - Verify changes trigger layout save or mark dirty

---

## Next Steps

1. Search for effect parameter UI and verify save triggers
2. Implement full OnProfileSave/OnProfileLoad
3. Add dirty flag system for layout changes
4. Test each area systematically
