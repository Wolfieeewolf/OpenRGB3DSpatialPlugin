# Effect System Architecture

## Overview

The OpenRGB 3D Spatial Plugin has **two different UI tabs** that both use the same effect system:

1. **Effects Tab** - Run a **single** effect across all devices (simple mode)
2. **Effect Stack Tab** - Stack **multiple** effects with per-zone targeting and blend modes (advanced compositing)

**IMPORTANT:** Both tabs use the **same effects** from the effect registry. The difference is HOW effects are applied, not WHICH effects are available.

## Effects Tab (Simple Mode)

### Purpose

Run **ONE** effect across **ALL** controllers/LEDs at once.

### Use Case

Simple scenarios where you want a single effect for your entire setup:
- "Make everything do Wave3D"
- "Make all my LEDs pulse to audio"
- "Run ScreenMirror3D on my whole setup"

### Characteristics

- **One effect at a time** (no stacking)
- **All controllers** get the same effect
- **No blend modes** (just the raw effect)
- **Simple, quick setup**

---

## Effect Stack Tab (Advanced Compositing)

### Purpose

Stack **MULTIPLE** effects and **target specific zones/controllers**, then composite them together to create complex lighting scenes.

### Use Case

Advanced scenarios with different effects on different parts of your setup:
- "Wave3D on my keyboard + Wipe3D on my wall lights + AudioLevel3D on my speakers"
- "ScreenMirror3D on back-of-monitor LEDs + Plasma3D on ceiling"
- "Create a preset 'Movie Mode' with multiple stacked effects"

### Characteristics

- **Multiple effects** running simultaneously
- **Per-zone targeting** (keyboard, walls, speakers, etc.)
- **Blend modes** for compositing layers
- **Save as presets** for later use
- **Complex, powerful compositions**

### Location in UI

**Right side tabs → "Effect Stack" tab**

### Workflow Example

Creating a complex "Gaming Mode" stack:

1. **Layer 1 (Bottom)**: ScreenMirror3D
   - Target: Back-of-monitor LEDs (Zone: "Monitor Strip")
   - Blend: Replace
   - Shows screen content with 3D falloff

2. **Layer 2 (Middle)**: AudioLevel3D
   - Target: Speaker LEDs (Zone: "Speakers")
   - Blend: Add
   - Pulsing audio visualization on speakers

3. **Layer 3 (Top)**: Wave3D
   - Target: Keyboard (Zone: "Keyboard")
   - Blend: Replace
   - Cool wave effect on keyboard

**Result:** Each zone has its own effect, all running simultaneously!

**Save as "Gaming Mode" preset** → Can load this entire stack later with one click.

---

## The Same Effect Registry

### Key Concept

**Both the Effects tab AND the Effect Stack tab pull from the SAME list of effects.**

When you register an effect with `EFFECT_REGISTERER_3D`, it becomes available in:
- ✅ The Effects tab dropdown
- ✅ The Effect Stack tab dropdown

**There is only ONE effect system.** The difference is:
- **Effects Tab** = Simple interface for one effect on everything
- **Effect Stack Tab** = Advanced interface for multiple effects with zones

---

## Effect Registration System

### How Effects Register

#### 1. Effect Registration
Effects auto-register themselves using the `EFFECT_REGISTERER_3D` macro system:

**In the header (.h):**
```cpp
class MyEffect3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit MyEffect3D(QWidget* parent = nullptr);

    // This macro creates a static _register class and _registerer member
    EFFECT_REGISTERER_3D("MyEffect3D", "My Effect Name", "Category", [](){return new MyEffect3D;});

    // ... rest of class
};
```

**In the source (.cpp):**
```cpp
#include "MyEffect3D.h"

// This instantiates the static registerer (MUST be after header include)
REGISTER_EFFECT_3D(MyEffect3D);

// ... rest of implementation
```

#### 2. Effect Discovery

When the Effect Stack tab is created:

1. Calls `EffectListManager3D::get()->GetAllEffects()`
2. Iterates through all registered effects
3. Populates the effect type dropdown with:
   - **UI Name** (e.g., "Screen Mirror 3D")
   - **Class Name** (e.g., "ScreenMirror3D")
   - **Category** (e.g., "Ambilight")

**Code location:** `ui/OpenRGB3DSpatialTab_EffectStack.cpp:83-88`

#### 3. Effect Instances

Each effect in the stack is an `EffectInstance3D` containing:

```cpp
struct EffectInstance3D
{
    std::string effect_class_name;      // "ScreenMirror3D"
    SpatialEffect3D* effect;            // Actual effect object
    bool enabled;                       // Can be toggled on/off
    int target_zone_id;                 // Which zone/controllers
    BlendMode blend_mode;               // How to composite
    nlohmann::json settings;            // Effect-specific settings
};
```

#### 4. Rendering Pipeline

When effects are rendered:

```cpp
void OpenRGB3DSpatialTab::RenderEffectStack()
{
    for each EffectInstance in effect_stack:
        if instance.enabled:
            Calculate colors for all LEDs using instance.effect
            Blend with previous layer using instance.blend_mode
            Apply to LEDs in instance.target_zone_id
}
```

**Code location:** `ui/OpenRGB3DSpatialTab_EffectStackRender.cpp`

### Blend Modes

Effects can be composited using different blend modes:
- **Replace** - Overwrite previous layer
- **Add** - Additive blending
- **Multiply** - Multiplicative blending
- **Overlay** - Overlay blending
- **Alpha** - Alpha blending

### Effect Categories

Effects are organized into categories for better organization:

- **3D Spatial** - Wave3D, Spiral3D, Plasma3D, etc.
- **Audio** - Audio-reactive effects
- **Ambilight** - Screen capture effects (ScreenMirror3D)
- **Diagnostic** - Testing and debugging effects

---

## Adding a New Effect

### Step-by-Step Guide

#### 1. Create Effect Directory
```bash
mkdir -p Effects3D/MyEffect3D
```

#### 2. Create Header File

**Effects3D/MyEffect3D/MyEffect3D.h:**
```cpp
#ifndef MYEFFECT3D_H
#define MYEFFECT3D_H

#include <QWidget>
#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class MyEffect3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit MyEffect3D(QWidget* parent = nullptr);
    ~MyEffect3D();

    // Auto-registration (creates static members)
    EFFECT_REGISTERER_3D("MyEffect3D", "My Cool Effect", "3D Spatial", [](){return new MyEffect3D;});

    static std::string const ClassName() { return "MyEffect3D"; }
    static std::string const UIName() { return "My Cool Effect"; }

    // Required overrides
    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    // Settings persistence
    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    // Your effect-specific parameters
    float my_parameter;
};

#endif
```

#### 3. Create Source File

**Effects3D/MyEffect3D/MyEffect3D.cpp:**
```cpp
#include "MyEffect3D.h"

// CRITICAL: This instantiates the static registerer
REGISTER_EFFECT_3D(MyEffect3D);

#include <QVBoxLayout>
// ... other includes

MyEffect3D::MyEffect3D(QWidget* parent)
    : SpatialEffect3D(parent)
    , my_parameter(1.0f)
{
}

MyEffect3D::~MyEffect3D()
{
}

EffectInfo3D MyEffect3D::GetEffectInfo()
{
    EffectInfo3D info = {};
    info.info_version = 2;
    info.effect_name = "My Cool Effect";
    info.effect_description = "Does something cool in 3D space";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_WAVE; // Use existing type or add new
    info.has_custom_settings = true;
    // ... other settings
    return info;
}

void MyEffect3D::SetupCustomUI(QWidget* parent)
{
    QVBoxLayout* layout = new QVBoxLayout();
    // Add your UI controls
    parent->setLayout(layout);
}

void MyEffect3D::UpdateParams(SpatialEffectParams& /*params*/)
{
    // Update standard parameters if needed
}

RGBColor MyEffect3D::CalculateColor(float /*x*/, float /*y*/, float /*z*/, float /*time*/)
{
    // Simple per-LED calculation (used by Effects tab)
    // If you want the same behavior in both tabs, implement your logic here
    return ToRGBColor(0, 0, 0);
}

RGBColor MyEffect3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    // THIS is where the magic happens
    // x, y, z are in grid units (world position)
    // time is animation time in seconds
    // grid contains room dimensions

    float value = sin((x + time) / 10.0f);
    uint8_t brightness = (uint8_t)((value + 1.0f) * 127.5f);
    return ToRGBColor(brightness, 0, 0);
}

nlohmann::json MyEffect3D::SaveSettings() const
{
    nlohmann::json settings;
    settings["my_parameter"] = my_parameter;
    return settings;
}

void MyEffect3D::LoadSettings(const nlohmann::json& settings)
{
    if(settings.contains("my_parameter"))
        my_parameter = settings["my_parameter"].get<float>();
}
```

#### 4. Add to Build System

**OpenRGB3DSpatialPlugin.pro:**
```qmake
HEADERS += \
    # ... existing headers ...
    Effects3D/MyEffect3D/MyEffect3D.h \

SOURCES += \
    # ... existing sources ...
    Effects3D/MyEffect3D/MyEffect3D.cpp \
```

#### 5. Build and Test

1. **Clean build** (important for static initialization):
   ```bash
   rm -rf build
   qmake
   make
   ```

2. **Restart OpenRGB** with the new plugin DLL

3. **Check both tabs** - your effect should appear in:
   - Effects tab dropdown (simple mode)
   - Effect Stack tab dropdown (advanced mode)

### Common Pitfalls

#### Effect Not Appearing

1. **Forgot `REGISTER_EFFECT_3D()`** in the .cpp file
2. **Didn't do a clean rebuild** - static initialization needs fresh compile
3. **Header include order** - make sure `REGISTER_EFFECT_3D()` comes after `#include "YourEffect.h"`
4. **Looking in wrong place** - Effects appear in BOTH tabs (Effects AND Effect Stack)

#### Registration Pattern

```cpp
// ❌ WRONG - Registration before header
REGISTER_EFFECT_3D(MyEffect3D);
#include "MyEffect3D.h"

// ✅ CORRECT - Registration after header
#include "MyEffect3D.h"
REGISTER_EFFECT_3D(MyEffect3D);
```

---

## User Workflows

### Effects Tab (Simple Mode)

1. Open **Effects** tab (right side)
2. Select an effect from the **dropdown**
3. Configure effect settings
4. Effect runs on **all controllers**
5. Select a different effect to switch

**Use when:** You want one effect for everything

### Effect Stack Tab (Advanced Mode)

1. Open **Effect Stack** tab (right side)
2. Click **+ Add Effect**
3. New layer appears in the stack list
4. Select **Effect Type** from dropdown (same effects as Effects tab!)
5. Configure effect settings in the UI below
6. Choose **Target Zone** (which controllers/zones to affect)
7. Select **Blend Mode** (how to combine with other layers)
8. Repeat to add more layers
9. Toggle layers on/off by double-clicking in the list
10. Stack is rendered bottom-to-top, compositing each layer

**Use when:** You want different effects on different zones

### Persistence

Effect stacks can be saved as **Stack Presets**:
- Saves entire stack configuration
- Includes all effect settings, zones, blend modes
- Can be loaded later or shared

---

## Accessing Global Resources

### Display Planes (for Ambilight Effects)

Use the `DisplayPlaneManager` singleton:

```cpp
#include "DisplayPlaneManager.h"

// Get all display planes
std::vector<DisplayPlane3D*> planes = DisplayPlaneManager::instance()->GetDisplayPlanes();

// Get specific plane by ID
DisplayPlane3D* plane = DisplayPlaneManager::instance()->GetPlaneById(plane_id);

// Access plane properties
float width_mm = plane->GetWidthMM();
Transform3D transform = plane->GetTransform();
std::string capture_id = plane->GetCaptureSourceId();
```

### Screen Capture (for Ambilight Effects)

Use the `ScreenCaptureManager` singleton:

```cpp
#include "ScreenCaptureManager.h"

// Initialize if needed
ScreenCaptureManager::Instance().Initialize();

// Get available monitors
std::vector<CaptureSourceInfo> monitors = ScreenCaptureManager::Instance().GetAvailableSources();

// Start capture for a monitor
ScreenCaptureManager::Instance().StartCapture(source_id);

// Get latest frame
std::shared_ptr<CapturedFrame> frame = ScreenCaptureManager::Instance().GetLatestFrame(source_id);
if(frame && frame->valid)
{
    // frame->data is RGBA pixels
    // frame->width, frame->height
}
```

### Audio Input (for Audio Effects)

Use the `AudioInputManager` singleton:

```cpp
#include "Audio/AudioInputManager.h"

// Get bass energy
float bass = AudioInputManager::instance()->getBandEnergyHz(20.0f, 250.0f);

// Get specific frequency band
float energy = AudioInputManager::instance()->getBandEnergyHz(low_hz, high_hz);
```

---

## Effect Lifecycle

1. **Registration** - Static initialization creates entry in `EffectListManager3D`
2. **Discovery** - UI enumerates all registered effects
3. **Instantiation** - User adds effect to stack, constructor is called
4. **Setup** - `SetupCustomUI()` creates UI controls
5. **Rendering** - `CalculateColorGrid()` called for each LED, each frame
6. **Persistence** - `SaveSettings()` / `LoadSettings()` for saving state
7. **Destruction** - Effect removed from stack, destructor called

---

## Thread Safety

- **Effect rendering** happens on a worker thread
- **UI updates** must happen on the main Qt thread
- **Global managers** (DisplayPlaneManager, ScreenCaptureManager, AudioInputManager) are thread-safe
- Use mutexes if your effect maintains shared state

---

## Performance Considerations

- `CalculateColorGrid()` is called **for every LED, every frame** (30-60 FPS)
- Keep calculations lightweight
- Cache expensive operations where possible
- Use grid context for adaptive scaling
- Consider distance-based LOD for far-away LEDs

---

## Debugging Tips

1. **Effect not in list?**
   - Check `REGISTER_EFFECT_3D()` is present
   - Verify clean rebuild
   - Look in Effect Stack tab, not Effects tab

2. **Effect crashes on add?**
   - Check constructor doesn't access null pointers
   - Verify all UI pointers are initialized to nullptr

3. **Settings not saving?**
   - Implement `SaveSettings()` / `LoadSettings()`
   - Use JSON serialization properly

4. **Effect not rendering?**
   - Check `CalculateColorGrid()` is implemented
   - Verify effect is enabled in stack
   - Check target zone is correct

---

## Example: ScreenMirror3D

The ScreenMirror3D effect demonstrates:

- ✅ Auto-registration with category "Ambilight"
- ✅ Accessing DisplayPlanes via DisplayPlaneManager
- ✅ Accessing screen capture via ScreenCaptureManager
- ✅ 3D spatial projection using Geometry3DUtils
- ✅ Distance-based falloff for immersive feel
- ✅ Custom UI with dropdowns, spinboxes, checkboxes
- ✅ Settings persistence

**Location:** `Effects3D/ScreenMirror3D/`

---

## Future Enhancements

- Effect categories as filterable groups in UI
- Effect search/filter in dropdown
- Effect thumbnails/previews
- Effect templates for quick setup
- Community effect marketplace

---

**Last Updated:** 2025-10-24
**Author:** OpenRGB 3D Spatial Plugin Team
