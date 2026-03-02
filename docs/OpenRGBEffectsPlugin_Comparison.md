# OpenRGB Effects Plugin vs OpenRGB 3D Spatial Plugin – Full Comparison

This document summarizes the **OpenRGB Effects Plugin** (`F:\MCP\OpenRGBEffectsPlugin`) and how it compares to **OpenRGB 3D Spatial Plugin**, including updates after a fresh review of the latest Effects plugin codebase.

---

## 0. What’s new / changed in Effects plugin (latest review)

- **Plugin API** – Uses `GetPluginInfo()` returning `OpenRGBPluginInfo` (Name, Description, Version, Commit, URL, **Label**, **Location**, **Icon**). We have Label and Location; we do not set `info.Icon`.
- **DeviceList** – Now uses `InitControllersList()` (no longer `Init(std::vector<ControllerZone*>)`). Builds the list from `GetRGBControllers()` itself; supports **zone segments** (each zone can expose full zone + per-segment entries). Uses `DeviceListItem` per controller (with `ZoneListItem`-style entries). Global toggles: **Select all**, **Reverse**, **Brightness** (sun icon).
- **ControllerZone** – **Segment support**: `is_segment`, `segment_idx`; constructor and `to_json()`/display_name include segment. `startidx()`, `leds_count()`, etc. respect segment.
- **ZoneListItem** – Per-zone widget: enable, reverse, brightness slider; `Enabled`, `Reversed`, `BrightnessChanged` signals; can disable controls when device list is disabled.
- **EffectListManager** – Still no sorting inside the manager. `GetCategorizedEffects()` returns `map<category, vector<effect_names>>`. **EffectList::AddEffectsMenus()** sorts each category by `ui_name` when building the menu. We sort inside **GetCategorizedEffects()** in our manager.
- **effect_names** – Struct with `classname` and `ui_name` (in **EffectsName.h**). They use `classname`; we use `class_name` in `EffectRegistration3D`.
- **OpenRGBEffectSettings** – `ShadersFolder()`, `ListShaders()`, `SaveShader()`; `PatternsFolder()`, `SaveEffectPattern`, `ListPattern`, `LoadPattern`; `CreateShadersDirectory`, `CreateEffectPatternsDirectory`. Global settings include `fpscapture`, `audio_settings`, etc.
- **EffectManager** – `AddPreview(RGBEffect*, ControllerZone*)`, `RemovePreview(RGBEffect*)` for live preview. One thread per active effect (`EffectThreadFunction`).
- **PreviewWidget** – Simple `QLabel`; double-click or key toggles fullscreen.
- **Network SDK** – `protocol_version = 2`. Packet IDs: REQUEST_EFFECT_LIST (0), START_EFFECT (20), STOP_EFFECT (21), REQUEST_EFFECTS_PROFILE_LIST (22), LOAD_EFFECTS_PROFILE (23). HandleSDK in plugin; registered via `GetServer()->RegisterPlugin(net_plugin)`.
- **License** – Newer Effects plugin files use **GPL-2.0-or-later**; we use GPL-2.0-only.

---

## 1. Plugin entry and lifecycle

| Area | OpenRGB Effects Plugin | OpenRGB 3D Spatial Plugin |
|------|------------------------|---------------------------|
| **Entry** | `OpenRGBEffectsPlugin.cpp`: Load(RM), GetWidget(), GetTrayMenu(), Unload() | Same interface; no tray menu (returns `nullptr`) |
| **Plugin info** | GetPluginInfo(): Name, Description, Version, Commit, URL, **Label** ("Effects"), **Location** (TOP), **Icon** (loaded from resource) | GetPluginInfo(): same minus Icon |
| **RM pointer** | Static `RMPointer` used by DeviceList, settings, etc. | Static `RMPointer` for tab/device list |
| **Load()** | Registers network plugin (SDK), LoadGlobalSettings() | Only stores RM, no SDK |
| **GetWidget()** | Waits for device detection, creates `OpenRGBEffectTab`, registers device-list + **detection progress** callbacks | Waits for detection, creates `OpenRGB3DSpatialTab`, device-list callback only |
| **GetTrayMenu()** | "Effects" menu: Start all, Stop all, Profiles submenu (load profile actions); ProfileListUpdated signal | Returns `nullptr` |
| **Unload()** | StopAll(), unregister both callbacks, UnregisterPlugin(info.Name) | Unregister device-list callback only |
| **Device list change** | DeviceListChangedCallback: EffectManager::ClearAssignments(), then invoke DeviceListChanged on tab | Invoke `UpdateDeviceList` on tab |

**Takeaway:** We could add a tray menu (Start/Stop all, load layout/profile), optional SDK, and Icon in GetPluginInfo.

---

## 2. Effect registration system

| Area | OpenRGB Effects Plugin | OpenRGB 3D Spatial Plugin |
|------|------------------------|---------------------------|
| **Manager** | `EffectListManager` singleton (pointer, heap), `get()` | `EffectListManager3D` singleton (static instance), `get()` returns `&instance` |
| **Storage** | `effects_constructors[classname]`; `categorized_effects[category]` = vector of `effect_names` (classname, ui_name) | `effects[class_name]` = EffectRegistration3D; `effect_order` = registration order |
| **RegisterEffect** | (classname, ui_name, category, constructor); adds to both maps | (class_name, ui_name, category, constructor); map + effect_order; allows overwrite by class_name |
| **Get constructor** | `GetEffectConstructor(name)` | `CreateEffect(class_name)` (includes aliases, e.g. Comet3D→TravelingLight3D) |
| **Categorization** | `GetCategorizedEffects()` → map<category, vector<effect_names>>; **no sort in manager**; EffectList sorts by ui_name when building menus | `GetCategorizedEffects()` → map<category, vector<EffectRegistration3D>>; **each category sorted by ui_name inside manager** |
| **Registration style** | **EffectRegisterer.h**: `EFFECT_REGISTERER(classname, ui_name, category, constructor)` + `REGISTER_EFFECT(T)` in effect .cpp (static _registerer) | Macro in header only: `EFFECT_REGISTERER_3D("Class3D", "UI Name", "Category", lambda)` (no separate REGISTER_EFFECT_3D in cpp) |

**Takeaway:** Our registration is similar; we sort in GetCategorizedEffects, they sort in UI. We could adopt their header+cpp registration (ClassName(), UI_Name()) for i18n.

---

## 3. Effect base class and metadata

| Area | OpenRGB Effects Plugin | OpenRGB 3D Spatial Plugin |
|------|------------------------|---------------------------|
| **Base class** | `RGBEffect` (QWidget): StepEffect(zones), LoadCustomSettings/SaveCustomSettings (json), FPS, Speed, UserColors, Brightness, Temperature, Tint, Slider2, EffectState, etc. | `SpatialEffect3D` (QWidget): Render/Tick with 3D context, SaveSettings/LoadSettings (json), zone/origin, speed, colors, etc. |
| **Metadata** | `EffectInfo`: EffectName, EffectClassName, EffectDescription, CustomName, Min/MaxSpeed, UserColors, Slider2, HasCustomSettings, etc. | `EffectInfo3D`: effect_name, category, effect_type, speed/frequency/user_colors, flags (needs_3d_origin, show_speed_control, etc.) |
| **Output** | Writes to ControllerZone (SetLED, SetAllZoneLEDs with brightness/temperature/tint) | Writes to virtual controllers / grid: 3D positions → LED indices via layout |
| **Persistence** | ToJson() / LoadCustomSettings(json) per effect; profile = list of effect JSONs + zone assignments | SaveSettings() / LoadSettings(json); effect stack = list of instances with saved_settings json |

**Takeaway:** Different models (2D zones vs 3D grid); same idea: metadata + json save/load.

---

## 4. Effect runtime (running effects and device assignment)

| Area | OpenRGB Effects Plugin | OpenRGB 3D Spatial Plugin |
|------|------------------------|---------------------------|
| **Runtime manager** | `EffectManager`: SetEffectActive/UnActive, **one thread per effect** (EffectThreadFunction), Assign(effect, zones), AddPreview/RemovePreview | No separate manager: effect stack in tab, **timer-driven** (e.g. 33 ms), PrepareStackForPlayback() creates effect instances, applies saved_settings |
| **Assignment** | effect_zones[effect] = vector<ControllerZone*>; assigning removes zones from other effects; controllers set to Direct mode | controller_transforms (position/rotation + virtual_controller or raw controller); SetCustomMode() on controllers |
| **Device list change** | EffectManager::ClearAssignments(); device list rebuilt; zone matching on load by name/vendor/serial/location | UpdateDeviceList() refreshes lists; layout/stack persisted in plugin settings |
| **Start/Stop** | Start: create thread, StepEffect in loop with FPS. Stop: join thread | Start: effect_timer starts; Tick() on each instance. Stop: timer stop |

**Takeaway:** They use one thread per effect and optional preview; we use one timer and tick the whole stack. Fits our 3D blending model.

---

## 5. UI layout and structure

| Area | OpenRGB Effects Plugin | OpenRGB 3D Spatial Plugin |
|------|------------------------|---------------------------|
| **Main layout** | Grid: **left** = EffectTabs (tab per effect, first tab = placeholder); **right** = DeviceList (fixed 300px) | Splitter: **left** = effect stack list + library + object creator; **right** = detail (effect config + 3D view or setup) |
| **Effect list** | EffectList as **tab bar button** (RightSide); "New effect" **menu** (categories + EffectSearch); AddEffect(name) creates tab | Effect Library: **category combo** + **search line edit** + **list** + "Add To Stack"; stack list separate (reorder, enable/disable, config per row) |
| **Effect tab** | Each effect = tab; tab bar = EffectTabHeader (name, start/stop, close); content = OpenRGBEffectPage (effect UI + device selection) | Single "Effects / Presets" tab: left = stack + library; right = effect config (zone, origin, custom UI) for **selected** stack entry |
| **Device/zone selection** | **Shared** DeviceList on right; ApplySelection(GetAssignedZones(current effect)); SelectionChanged → Assign | **Per-effect** zone and origin in effect config; controllers from 3D scene (transforms/virtual controllers) |

**Takeaway:** Different layouts by design; we adopted category + search + GetCategorizedEffects.

### 5a. UI/layout ideas from Effects plugin we could reuse

- **Start all / Stop all** – Single control to run or stop the whole stack. We added **"Start all"** and **"Stop all"** buttons next to the effect stack list; they call the same logic as the per-effect Start/Stop and stay in sync with `effect_running`.
- **Icon button for Start/Stop all** – They use a single icon button (play/track icon) that toggles; we use two text buttons for clarity. Optional: use an icon font (e.g. play/stop) if we add one later.
- **Global settings in a menu** – Their "New effect" dropdown also has Profiles, Settings, About. We don't need a dropdown, but a **Settings** or **Effect defaults** entry (e.g. in a toolbar or under the stack) could open a small dialog for global FPS cap, brightness, preferred colors (see 7a).
- **Tab bar button** – They put the effect-add control as a tab bar button (RightSide). We use a separate Effect Library panel; our layout is already different and works.
- **Per-effect description** – OpenRGBEffectPage shows effect name and description; we could show a one-line description under the effect type in the config panel if effects expose it in EffectInfo3D.

---

## 6. Device / zone list

| Area | OpenRGB Effects Plugin | OpenRGB 3D Spatial Plugin |
|------|------------------------|---------------------------|
| **Abstraction** | **ControllerZone**: controller, zone_idx, **is_segment**, **segment_idx**; reverse, self_brightness; startidx(), leds_count(), type(), display_name(), to_json(), SetLED, SetAllZoneLEDs | No ControllerZone; controller_transforms, VirtualController3D (grid mappings), ZoneManager zones for effect targeting |
| **DeviceList** | **InitControllersList()**: builds from GetRGBControllers(); per-zone and per-segment ControllerZones; hide_unsupported (no Direct = skip); **DeviceListItem** per controller (zones/segments as **ZoneListItem**); GetSelection(), ApplySelection(), **GetSelectAll/SetSelectAll**; toggles: **select all**, **reverse**, **brightness** | Available controllers list (object creator); custom controllers list; zones list (ZoneManager); effect zone combo for targeting |
| **ZoneListItem** | Enable, reverse, brightness slider; DisableControls/EnableControls; signals Enabled, Reversed, BrightnessChanged | N/A (we use zone combo, not per-zone widgets) |
| **Persistence** | ControllerZones in profile JSON (zone_idx, is_segment, segment_idx, controller id); load matches by name/vendor/serial/location | Layout and effect stack in plugin settings; zones by name |

**Takeaway:** They have full zone+segment list with per-zone controls; we use zones for effect targeting only. Segment support is a notable difference.

---

## 7. Profiles and global settings

| Area | OpenRGB Effects Plugin | OpenRGB 3D Spatial Plugin |
|------|------------------------|---------------------------|
| **Global settings** | OpenRGBEffectSettings::globalSettings: fps, fpscapture, brightness, temperature, tint, prefer_random, use_prefered_colors, prefered_colors, startup_timeout, startup_profile, hide_unsupported, **audio_settings**; LoadGlobalSettings() in Load() | Plugin settings (grid, room, camera, layout profile, effect stack, etc.) via SetPluginSettings; no separate global effect settings |
| **Profiles** | ListProfiles(), SaveUserProfile(json, name), LoadUserProfile(name), DeleteProfile(name); SaveProfilePopup; profile = effect JSONs + zone assignments | Layout profiles (names, auto-load); effect stack with layout; effect profiles (per-stack presets) in tab |
| **Startup** | startup_profile + startup_timeout; Load(profile) after delay in GetWidget() | Optional layout auto-load; effect stack restored from saved state |
| **Patterns / Shaders** | **SaveEffectPattern**, **ListPattern**, **LoadPattern** (per effect type); **ShadersFolder**, **ListShaders**, **SaveShader**; CreateEffectPatternsDirectory, CreateShadersDirectory | Presets/saved settings in stack; no separate pattern or shader folders |

**Takeaway:** We could add global effect settings (FPS, brightness) and/or “load profile at startup” for layout+stack.

### 7a. Global effect settings: what they have vs what we have

| Setting | Effects plugin | 3D Spatial plugin |
|--------|----------------|-------------------|
| **FPS** | Global fps (default 60); applied when creating effect (SetFPS) | No global FPS; each effect has effect_fps (default 30); timer uses max of stack GetEffectiveTargetFPS() when starting |
| **FPS capture** | fpscapture for capture-based effects | N/A |
| **Brightness** | Global brightness (0-100); ColorUtils::apply_adjustments in SetLED | No global brightness; per-effect color handling |
| **Temperature / Tint** | Global temperature, tint; applied in apply_adjustments | No global temperature/tint |
| **Preferred colors** | use_prefered_colors, prefered_colors when adding new effect | No preferred colors when adding effects |
| **Prefer random** | prefer_random set on new effect | No global prefer random |
| **Startup** | startup_timeout, startup_profile; load profile after delay | Layout auto-load; effect stack restored from plugin settings |
| **Hide unsupported** | DeviceList skips controllers without Direct mode | N/A |
| **Audio** | audio_settings; GlobalSettings has Audio settings button | Audio settings in our tab, in plugin settings |

**What we could add:** Global FPS cap (low effort); global brightness (medium); preferred colors when adding effect (medium); temperature/tint only if we want color adjustment.

---

## 8. Per-effect UI (effect page)

| Area | OpenRGB Effects Plugin | OpenRGB 3D Spatial Plugin |
|------|------------------------|---------------------------|
| **Container** | OpenRGBEffectPage: name, description, Speed, Slider2, FPS, Brightness, Temperature, Tint, User colors (ColorPickers), Random/OnlyFirst; **ExtraOptions** layout = effect widget; menu: Save/Load/Edit pattern, Open patterns folder | Effect config: zone, origin; custom UI from SetupCustomUI(); common controls from CreateCommonEffectControls (speed, colors, etc.) |
| **Preview** | Preview button → **LivePreviewController** (fake RGBController + zone presets); effect runs on fake zone; **PreviewWidget** (QLabel, fullscreen toggle) | “Preview in 3D View” in custom controller dialog; main viewport shows 3D scene |
| **Patterns** | Save/Load/Edit pattern (json), open patterns folder | No pattern menu; settings in stack entry |

**Takeaway:** We could add per-effect “Save/Load preset” in the effect panel and/or a small preview widget.

---

## 9. Search and categories

| Area | OpenRGB Effects Plugin | OpenRGB 3D Spatial Plugin |
|------|------------------------|---------------------------|
| **Categories** | GetCategorizedEffects(); category **submenus** in “New effect” menu; categories from registration (CAT_SIMPLE, CAT_AUDIO, etc.) | GetCategorizedEffects(); **category combo**; categories from registration (“3D Spatial”, “Audio”, “Ambilight”) |
| **Search** | **EffectSearch** widget **inside** “New effect” menu: line edit, filter by ui_name (case-insensitive), results list; click or Enter adds effect; Searching(true) hides category menus | **Search line edit** above effect library list; filters current category by ui_name (case-insensitive); on_effect_library_search_changed → PopulateEffectLibrary() |
| **Sort** | Effects in each category **sorted in AddEffectsMenus()** by ui_name (not in manager) | **Sorted in GetCategorizedEffects()** by ui_name |

**Takeaway:** Aligned on categories + search; we sort in manager, they sort in UI.

---

## 10. Other features (Effects plugin)

| Feature | Description |
|--------|--------------|
| **Network SDK** | HandleSDK: REQUEST_EFFECT_LIST, START_EFFECT, STOP_EFFECT, REQUEST_EFFECTS_PROFILE_LIST, LOAD_EFFECTS_PROFILE; RegisterPlugin/UnregisterPlugin with protocol_version = 2 |
| **i18n** | QTranslator; language from main window; effect list retranslated; QT_TR_NOOP / QCoreApplication::translate for effect names (by classname) |
| **OpenRGBPluginsFont** | Icon font for buttons (play, tv, chevron, sun, etc.) |
| **PluginInfo** | “About” dialog (version, etc.) |
| **ColorUtils** | apply_adjustments (brightness, temperature, tint), RandomRGBColor |
| **QTooltipedSlider** | Slider with value in tooltip |
| **Shaders** | Shaders effect: ShadersFolder(), ListShaders(), GLSL editor, ShaderRenderer, etc. |
| **Audio** | AudioSettingsStruct, AudioManager; audio-reactive effects |
| **Layers** | Layers effect: LayerEntry, LayerGroupEntry (nested effects) |
| **Screen capture** | ScreenCapturer (Windows/Qt/Wayland/PipeWire) |
| **GifPlayer / Sequence / Stack** | Additional effect types |

**Takeaway:** We have audio and screen mirror; we don’t have SDK, full i18n, or shader system.

---

## 11. Summary: what we already took / could still take

**Already adopted**

- **GetCategorizedEffects()** and categorization; **sort by ui_name** (we do it in manager, they do it in UI).
- **Effect library search** (line edit filtering current category).
- Registration pattern similar (macro; we use string literals, they use classname/ui_name).
- **Start all / Stop all** – "Start all" and "Stop all" buttons next to the effect stack list; they start or stop the whole stack and stay in sync with the per-effect Start/Stop buttons.

**Could consider later**

- Global effect defaults (FPS cap, brightness, preferred colors) – see section 7a; add a small "Effect defaults" block or dialog.
- Global effect settings (FPS, brightness, preferred colors) when creating/starting effects.
- Per-effect “Save/Load pattern” or preset in effect panel.
- Network SDK (list effects, start/stop, load profile) for external tools.
- i18n for effect names and UI (translate by class name).
- ~~“Start/Stop all” button in Effects tab~~ (done).
- Icon in GetPluginInfo() if OpenRGB shows it.
- Device list change: they clear assignments and re-match on load; we refresh list and keep layout/stack.
- Optional: icon button for Start/Stop all; per-effect description line in effect config (if EffectInfo3D gains a description field).

---

## 12. File mapping (quick reference)

| Effects plugin | 3D Spatial plugin (rough equivalent) |
|----------------|-------------------------------------|
| OpenRGBEffectsPlugin.cpp/h | OpenRGB3DSpatialPlugin.cpp/h |
| OpenRGBEffectTab | OpenRGB3DSpatialTab |
| EffectListManager + EffectList | EffectListManager3D + Effect Library panel |
| EffectList (menu + EffectSearch) | SetupEffectLibraryPanel (combo + search line edit + list) |
| EffectManager | No single manager; stack + timer in tab |
| OpenRGBEffectPage | Effect config panel + SpatialEffect3D::SetupCustomUI |
| DeviceList + DeviceListItem + ZoneListItem | Zones combo, controller_transforms, VirtualController3D |
| ControllerZone (with segments) | No direct equivalent (we use zones from ZoneManager) |
| OpenRGBEffectSettings | Plugin settings via SetPluginSettings |
| SaveProfilePopup, LoadProfile | Layout profiles, effect stack save/load |
| GlobalSettings | No equivalent (could add) |
| EffectRegisterer.h / REGISTER_EFFECT | EffectRegisterer3D.h / EFFECT_REGISTERER_3D |
| EffectsName.h (effect_names) | EffectRegistration3D (class_name, ui_name, category, constructor) |
| RGBEffect | SpatialEffect3D |
| LivePreviewController + PreviewWidget | Preview in 3D viewport / custom controller dialog |
| HandleSDK, NetworkServer | No equivalent |

This gives an up-to-date picture of both plugins for future upgrades and consistency.
