# Codebase cleanup checklist

Use this list to audit each file against: **CONTRIBUTING.md**, **DRY**, **KISS**, **SOLID**, **YAGNI**, **Tell Don't Ask** (tell objects what to do; avoid ask-then-decide in callers). Remove debug/dead/legacy/deprecated code; keep comments minimal.

**Per-file checks:** [ ] = not done, [x] = done

---

## Root

| File | DRY | KISS | TDA | No dead/debug | Comments | Done |
|------|-----|------|---------------|----------|------|
| OpenRGB3DSpatialPlugin.cpp | x | x | x | x | x | x |
| OpenRGB3DSpatialPlugin.h | x | x | x | x | x | x |
| ControllerLayout3D.cpp | x | x | x | x | x | x |
| ControllerLayout3D.h | x | x | x | x | x | x |
| DisplayPlane3D.cpp | x | x | x | x | x | x |
| DisplayPlane3D.h | x | x | x | x | x | x |
| DisplayPlaneManager.h | x | x | x | x | x | x |
| EffectInstance3D.cpp | x | x | x | x | x | x |
| EffectInstance3D.h | x | x | x | x | x | x |
| EffectListManager3D.h | x | x | x | x | x | x |
| EffectRegisterer3D.h | x | x | x | x | x | x |
| Geometry3DUtils.h | x | x | x | x | x | x |
| GridSpaceUtils.cpp | x | x | x | x | x | x |
| GridSpaceUtils.h | x | x | x | x | x | x |
| LEDPosition3D.h | x | x | x | x | x | x |
| QtCompat.h | x | x | x | x | x | x |
| ScreenCaptureManager.cpp | x | x | x | x | x | x |
| ScreenCaptureManager.h | x | x | x | x | x | x |
| SpatialEffect3D.cpp | x | x | x | x | x | x |
| SpatialEffect3D.h | x | x | x | x | x | x |
| SpatialEffectTypes.h | x | x | x | x | x | x |
| StackPreset3D.cpp | x | x | x | x | x | x |
| StackPreset3D.h | x | x | x | x | x | x |
| VirtualController3D.cpp | x | x | x | x | x | x |
| VirtualController3D.h | x | x | x | x | x | x |
| VirtualReferencePoint3D.cpp | x | x | x | x | x | x |
| VirtualReferencePoint3D.h | x | x | x | x | x | x |
| Zone3D.cpp | x | x | x | x | x | x |
| Zone3D.h | x | x | x | x | x | x |
| ZoneManager3D.cpp | x | x | x | x | x | x |
| ZoneManager3D.h | x | x | x | x | x | x |

## Audio/

| File | DRY | KISS | TDA | No dead/debug | Comments | Done |
|------|-----|------|---------------|----------|------|
| AudioInputManager.cpp | x | x | x | x | x | x |
| AudioInputManager.h | x | x | x | x | x | x |

## ui/

| File | DRY | KISS | TDA | No dead/debug | Comments | Done |
|------|-----|------|---------------|----------|------|
| OpenRGB3DSpatialTab.cpp | x | x | x | x | x | x |
| OpenRGB3DSpatialTab.h | x | x | x | x | x | x |
| OpenRGB3DSpatialTab_Audio.cpp | x | x | x | x | x | x |
| OpenRGB3DSpatialTab_EffectProfiles.cpp | x | x | x | x | x | x |
| OpenRGB3DSpatialTab_EffectStack.cpp | x | x | x | x | x | x |
| OpenRGB3DSpatialTab_EffectStackPersist.cpp | x | x | x | x | x | x |
| OpenRGB3DSpatialTab_EffectStackRender.cpp | x | x | x | x | x | x |
| OpenRGB3DSpatialTab_ObjectCreator.cpp | x | x | x | x | x | x |
| OpenRGB3DSpatialTab_Profiles.cpp | x | x | x | x | x | x |
| OpenRGB3DSpatialTab_RefPoints.cpp | x | x | x | x | x | x |
| OpenRGB3DSpatialTab_StackPresets.cpp | x | x | x | x | x | x |
| OpenRGB3DSpatialTab_Zones.cpp | x | x | x | x | x | x |
| LEDViewport3D.cpp | x | x | x | x | x | x |
| LEDViewport3D.h | x | x | x | x | x | x |
| CustomControllerDialog.cpp | x | x | x | x | x | x |
| CustomControllerDialog.h | x | x | x | x | x | x |
| Gizmo3D.cpp | x | x | x | x | x | x |
| Gizmo3D.h | x | x | x | x | x | x |

## Effects3D/

| File | DRY | KISS | TDA | No dead/debug | Comments | Done |
|------|-----|------|---------------|----------|------|
| ScreenMirror3D/ScreenMirror3D.cpp | x | x | x | x | x | x |
| ScreenMirror3D/ScreenMirror3D.h | x | x | x | x | x | x |
| Wave3D/Wave3D.cpp | x | x | x | x | x | x |
| Wave3D/Wave3D.h | x | x | x | x | x | x |
| Wipe3D/Wipe3D.cpp | x | x | x | x | x | x |
| Wipe3D/Wipe3D.h | x | x | x | x | x | x |
| Plasma3D/Plasma3D.cpp | x | x | x | x | x | x |
| Plasma3D/Plasma3D.h | x | x | x | x | x | x |
| Spiral3D/Spiral3D.cpp | x | x | x | x | x | x |
| Spiral3D/Spiral3D.h | x | x | x | x | x | x |
| Spin3D/Spin3D.cpp | x | x | x | x | x | x |
| Spin3D/Spin3D.h | x | x | x | x | x | x |
| Explosion3D/Explosion3D.cpp | x | x | x | x | x | x |
| Explosion3D/Explosion3D.h | x | x | x | x | x | x |
| BreathingSphere3D/BreathingSphere3D.cpp | x | x | x | x | x | x |
| BreathingSphere3D/BreathingSphere3D.h | x | x | x | x | x | x |
| DNAHelix3D/DNAHelix3D.cpp | x | x | x | x | x | x |
| DNAHelix3D/DNAHelix3D.h | x | x | x | x | x | x |
| Rain3D/Rain3D.cpp | x | x | x | x | x | x |
| Rain3D/Rain3D.h | x | x | x | x | x | x |
| Tornado3D/Tornado3D.cpp | x | x | x | x | x | x |
| Tornado3D/Tornado3D.h | x | x | x | x | x | x |
| Lightning3D/Lightning3D.cpp | x | x | x | x | x | x |
| Lightning3D/Lightning3D.h | x | x | x | x | x | x |
| Matrix3D/Matrix3D.cpp | x | x | x | x | x | x |
| Matrix3D/Matrix3D.h | x | x | x | x | x | x |
| BouncingBall3D/BouncingBall3D.cpp | x | x | x | x | x | x |
| BouncingBall3D/BouncingBall3D.h | x | x | x | x | x | x |
| BandScan3D/BandScan3D.cpp | x | x | x | x | x | x |
| BandScan3D/BandScan3D.h | x | x | x | x | x | x |
| BeatPulse3D/BeatPulse3D.cpp | x | x | x | x | x | x |
| BeatPulse3D/BeatPulse3D.h | x | x | x | x | x | x |
| AudioLevel3D/AudioLevel3D.cpp | x | x | x | x | x | x |
| AudioLevel3D/AudioLevel3D.h | x | x | x | x | x | x |
| SpectrumBars3D/SpectrumBars3D.cpp | x | x | x | x | x | x |
| SpectrumBars3D/SpectrumBars3D.h | x | x | x | x | x | x |
| EffectHelpers.h | x | x | x | x | x | x |
| AudioReactiveCommon.h | x | x | x | x | x | x |

---

*Do not modify files under `OpenRGB/` (upstream submodule).*
