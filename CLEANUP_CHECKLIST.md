# Codebase cleanup checklist

Use this list to audit each file against: **CONTRIBUTING.md**, **DRY**, **KISS**, **SOLID**, **YAGNI**, **Tell Don't Ask**, **UI/links**, **null safety**, **layout**, and **clear/unstick**. Remove debug/dead/legacy/deprecated code; keep comments minimal.

**Per-file checks:** x = done, - = not done (needs audit), n/a = not applicable.

**Columns:** **Links** = UI connections, selection sync, clear-selection handling (ui/ only; n/a elsewhere). **Null** = viewport/widget pointer use guarded. **Layout** = UI layout makes sense; no overlapping/squashed controls; tab content fits (ui/ only; n/a elsewhere). **Clear** = clear on unselect + no stuck state (e.g. effect detail UI closes when effect removed from stack; ui/ only; n/a elsewhere).

**Re-audit:** Files that already have DRY/KISS/TDA/etc. marked done should be **re-checked** for **Links**, **Null**, **Layout**, and **Clear** (Pass 2). UI files must pass all four where applicable; non-UI files use n/a for Links, Layout, Clear and x/n/a for Null.

---

## Root

| File | DRY | KISS | TDA | No dead/debug | Comments | Links | Null | Layout | Clear | Done |
|------|-----|------|-----|---------------|----------|-------|------|--------|-------|------|
| OpenRGB3DSpatialPlugin.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| OpenRGB3DSpatialPlugin.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| ControllerLayout3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| ControllerLayout3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| DisplayPlane3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| DisplayPlane3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| DisplayPlaneManager.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| EffectInstance3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| EffectInstance3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| EffectListManager3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| EffectRegisterer3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Geometry3DUtils.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| GridSpaceUtils.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| GridSpaceUtils.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| LEDPosition3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| QtCompat.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| ScreenCaptureManager.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| ScreenCaptureManager.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| SpatialEffect3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| SpatialEffect3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| SpatialEffectTypes.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| StackPreset3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| StackPreset3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| VirtualController3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| VirtualController3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| VirtualReferencePoint3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| VirtualReferencePoint3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Zone3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Zone3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| ZoneManager3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| ZoneManager3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |

## Audio/

| File | DRY | KISS | TDA | No dead/debug | Comments | Links | Null | Layout | Clear | Done |
|------|-----|------|-----|---------------|----------|-------|------|--------|-------|------|
| AudioInputManager.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| AudioInputManager.h | x | x | x | x | x | n/a | x | n/a | n/a | x |

## ui/

| File | DRY | KISS | TDA | No dead/debug | Comments | Links | Null | Layout | Clear | Done |
|------|-----|------|-----|---------------|----------|-------|------|--------|-------|------|
| OpenRGB3DSpatialTab.cpp | x | x | x | x | x | x | x | x | x | x |
| OpenRGB3DSpatialTab.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| OpenRGB3DSpatialTab_Audio.cpp | x | x | x | x | x | x | x | x | x | x |
| OpenRGB3DSpatialTab_EffectProfiles.cpp | x | x | x | x | x | x | x | x | x | x |
| OpenRGB3DSpatialTab_EffectStack.cpp | x | x | x | x | x | x | x | x | x | x |
| OpenRGB3DSpatialTab_EffectStackPersist.cpp | x | x | x | x | x | x | x | x | x | x |
| OpenRGB3DSpatialTab_EffectStackRender.cpp | x | x | x | x | x | x | x | x | x | x |
| OpenRGB3DSpatialTab_ObjectCreator.cpp | x | x | x | x | x | x | x | x | x | x |
| OpenRGB3DSpatialTab_Profiles.cpp | x | x | x | x | x | x | x | x | x | x |
| OpenRGB3DSpatialTab_RefPoints.cpp | x | x | x | x | x | x | x | x | x | x |
| OpenRGB3DSpatialTab_StackPresets.cpp | x | x | x | x | x | x | x | x | x | x |
| OpenRGB3DSpatialTab_Zones.cpp | x | x | x | x | x | x | x | x | x | x |
| LEDViewport3D.cpp | x | x | x | x | x | n/a | x | x | n/a | x |
| LEDViewport3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| CustomControllerDialog.cpp | x | x | x | x | x | x | x | x | x | x |
| CustomControllerDialog.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Gizmo3D.cpp | x | x | x | x | x | n/a | x | x | n/a | x |
| Gizmo3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |

## Effects3D/

| File | DRY | KISS | TDA | No dead/debug | Comments | Links | Null | Layout | Clear | Done |
|------|-----|------|-----|---------------|----------|-------|------|--------|-------|------|
| ScreenMirror3D/ScreenMirror3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| ScreenMirror3D/ScreenMirror3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Wave3D/Wave3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Wave3D/Wave3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Wipe3D/Wipe3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Wipe3D/Wipe3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Plasma3D/Plasma3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Plasma3D/Plasma3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Spiral3D/Spiral3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Spiral3D/Spiral3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Spin3D/Spin3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Spin3D/Spin3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Explosion3D/Explosion3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Explosion3D/Explosion3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| BreathingSphere3D/BreathingSphere3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| BreathingSphere3D/BreathingSphere3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| DNAHelix3D/DNAHelix3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| DNAHelix3D/DNAHelix3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Rain3D/Rain3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Rain3D/Rain3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Tornado3D/Tornado3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Tornado3D/Tornado3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Lightning3D/Lightning3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Lightning3D/Lightning3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Matrix3D/Matrix3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| Matrix3D/Matrix3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| BouncingBall3D/BouncingBall3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| BouncingBall3D/BouncingBall3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| BandScan3D/BandScan3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| BandScan3D/BandScan3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| BeatPulse3D/BeatPulse3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| BeatPulse3D/BeatPulse3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| AudioLevel3D/AudioLevel3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| AudioLevel3D/AudioLevel3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| SpectrumBars3D/SpectrumBars3D.cpp | x | x | x | x | x | n/a | x | n/a | n/a | x |
| SpectrumBars3D/SpectrumBars3D.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| EffectHelpers.h | x | x | x | x | x | n/a | x | n/a | n/a | x |
| AudioReactiveCommon.h | x | x | x | x | x | n/a | x | n/a | n/a | x |

---

*Do not modify files under `OpenRGB/` (upstream submodule).*
