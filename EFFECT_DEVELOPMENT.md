# Effect Development Guide

**Create custom 3D spatial effects for the OpenRGB 3D Spatial Plugin**

This guide will walk you through creating new spatial effects using the plugin's standardized effect system. No need to reinvent the wheel - the base class handles UI, parameters, colors, and more!

## 📚 Table of Contents

1. [Prerequisites](#prerequisites)
2. [Effect System Overview](#effect-system-overview)
3. [Creating Your First Effect](#creating-your-first-effect)
4. [Effect Info Structure](#effect-info-structure)
5. [Custom UI Controls](#custom-ui-controls)
6. [Color Calculation](#color-calculation)
7. [Parameter System](#parameter-system)
8. [Testing Your Effect](#testing-your-effect)
9. [Best Practices](#best-practices)
10. [Example Effects](#example-effects)
11. [Troubleshooting](#troubleshooting)

---

## Prerequisites

**Required Knowledge:**
- C++ programming (basic OOP)
- Qt framework basics (widgets, layouts, signals/slots)
- Basic trigonometry (sin, cos, distance calculations)
- Understanding of RGB color format (0x00BBGGRR)

**Development Environment:**
- Qt 5.15+ development libraries
- C++17 compatible compiler
- OpenRGB source code (for headers)
- Text editor or IDE (Qt Creator recommended)

**Recommended Reading:**
- [OpenRGB Contributing Guide](../../CONTRIBUTING.md)
- [RGB Controller API](../../RGBControllerAPI.md)
- Existing effect implementations in `Effects3D/`

---

## Effect System Overview

### Architecture

The plugin uses a **modular effect system** with these components:

1. **`SpatialEffect3D`**: Base class providing common functionality
2. **Effect Registration**: Auto-discovery system for loading effects
3. **Effect Info**: Metadata describing effect capabilities
4. **Parameter System**: Standardized normalization and scaling
5. **Color Calculation**: Pure function taking (x, y, z, time) → RGB color

### What the Base Class Provides

**Automatic UI Controls:**
- Speed slider (1-100, quadratic curve)
- Brightness slider (1-100, linear)
- Frequency slider (1-100, quadratic curve)
- Size slider (1-100, 0.1x-2.0x multiplier)
- Scale slider (10-200%, coverage area)
- FPS limiter (1-60)
- Axis selection (X/Y/Z/Radial)
- Reverse direction checkbox
- Rainbow mode toggle
- Multi-color picker (add/remove colors)

**Helper Functions:**
- `GetNormalizedSpeed()` - Returns 0.0-1.0 with quadratic curve
- `GetScaledSpeed()` - Returns speed * effect_speed_scale
- `GetNormalizedFrequency()` - Returns 0.0-1.0 with quadratic curve
- `GetScaledFrequency()` - Returns frequency * effect_frequency_scale
- `GetNormalizedSize()` - Returns 0.1-2.0 linear scale
- `GetNormalizedScale()` - Returns 0.1-2.0 linear scale
- `CalculateProgress(time)` - Returns time * scaled_speed (handles reverse)
- `GetEffectOrigin()` - Returns origin based on reference mode
- `GetRainbowColor(hue)` - HSV to RGB conversion
- `GetColorAtPosition(position)` - Interpolate through color list

**Built-in Features:**
- Reference point integration (room center, user position, custom points)
- Color management (rainbow or user-defined colors)
- Parameter normalization (consistent behavior across effects)
- Axis selection and reversal
- Signal emission on parameter changes

### What You Implement

**4 Required Virtual Methods:**

1. **`GetEffectInfo()`**: Return effect metadata
2. **`SetupCustomUI(QWidget* parent)`**: Create effect-specific controls
3. **`UpdateParams(SpatialEffectParams& params)`**: Set effect type in params
4. **`CalculateColor(x, y, z, time)`**: Calculate LED color at position/time

That's it! The base class handles everything else.

---

## Creating Your First Effect

Let's create a simple **Pulse3D** effect that pulses brightness from a center point.

### Step 1: Create Effect Files

Create two files in `Effects3D/Pulse3D/`:

- `Pulse3D.h` - Header file
- `Pulse3D.cpp` - Implementation file

### Step 2: Header File (`Pulse3D.h`)

```cpp
/*---------------------------------------------------------*\
| Pulse3D.h                                                 |
|                                                           |
|   3D pulsing effect from origin point                    |
|                                                           |
|   Date: 2025-10-04                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef PULSE3D_H
#define PULSE3D_H

#include "SpatialEffect3D.h"

class Pulse3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    Pulse3D(QWidget* parent = nullptr);
    ~Pulse3D();

    /*---------------------------------------------------------*\
    | Required virtual methods                                 |
    \*---------------------------------------------------------*/
    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;

private:
    /*---------------------------------------------------------*\
    | Custom parameters (if needed)                            |
    \*---------------------------------------------------------*/
    // No custom parameters for this simple effect
};

/*---------------------------------------------------------*\
| Effect registration (in header for static init)          |
\*---------------------------------------------------------*/
EFFECT_REGISTERER_3D("Pulse3D", "3D Pulse", "3D Spatial",
                     [](){return new Pulse3D;})

#endif
```

### Step 3: Implementation File (`Pulse3D.cpp`)

```cpp
/*---------------------------------------------------------*\
| Pulse3D.cpp                                               |
|                                                           |
|   3D pulsing effect from origin point                    |
|                                                           |
|   Date: 2025-10-04                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "Pulse3D.h"
#include "../EffectHelpers.h"  // For smoothstep, lerp, clamp

/*---------------------------------------------------------*\
| Register effect (triggers static initializer)            |
\*---------------------------------------------------------*/
REGISTER_EFFECT_3D(Pulse3D);

Pulse3D::Pulse3D(QWidget* parent) : SpatialEffect3D(parent)
{
    /*---------------------------------------------------------*\
    | Initialize custom parameters if needed                   |
    \*---------------------------------------------------------*/

    /*---------------------------------------------------------*\
    | Set default base class parameters                        |
    \*---------------------------------------------------------*/
    SetSpeed(50);           // Medium speed
    SetBrightness(100);     // Full brightness
    SetFrequency(30);       // Medium frequency
    SetRainbowMode(true);   // Enable rainbow by default
}

Pulse3D::~Pulse3D()
{
    // Cleanup if needed (Qt parent-child handles most cases)
}

EffectInfo3D Pulse3D::GetEffectInfo()
{
    EffectInfo3D info;

    /*---------------------------------------------------------*\
    | Version and basic info                                   |
    \*---------------------------------------------------------*/
    info.info_version = 2;  // Always 2 for new effects
    info.effect_name = "3D Pulse";
    info.effect_description = "Pulsing waves emanating from origin point";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_PULSE;

    /*---------------------------------------------------------*\
    | Capabilities                                             |
    \*---------------------------------------------------------*/
    info.is_reversible = true;   // Can reverse direction
    info.supports_random = true; // Supports rainbow mode
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;        // 0 = unlimited colors
    info.has_custom_settings = false;  // No custom UI

    /*---------------------------------------------------------*\
    | Requirements                                             |
    \*---------------------------------------------------------*/
    info.needs_3d_origin = true;   // Uses origin point
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;

    /*---------------------------------------------------------*\
    | Parameter scaling                                        |
    \*---------------------------------------------------------*/
    info.default_speed_scale = 10.0f;      // Speed multiplier
    info.default_frequency_scale = 10.0f;  // Frequency multiplier
    info.use_size_parameter = true;

    /*---------------------------------------------------------*\
    | Control visibility (show all standard controls)         |
    \*---------------------------------------------------------*/
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_axis_control = true;
    info.show_color_controls = true;

    return info;
}

void Pulse3D::SetupCustomUI(QWidget* parent)
{
    /*---------------------------------------------------------*\
    | No custom UI for this simple effect                     |
    | Just use the standard controls from base class          |
    \*---------------------------------------------------------*/
    (void)parent;  // Suppress unused parameter warning
}

void Pulse3D::UpdateParams(SpatialEffectParams& params)
{
    /*---------------------------------------------------------*\
    | Set the effect type (used by parent tab)                |
    \*---------------------------------------------------------*/
    params.type = SPATIAL_EFFECT_PULSE;
}

RGBColor Pulse3D::CalculateColor(float x, float y, float z, float time)
{
    /*---------------------------------------------------------*\
    | Step 1: Get effect origin (room center or reference pt) |
    \*---------------------------------------------------------*/
    Vector3D origin = GetEffectOrigin();

    /*---------------------------------------------------------*\
    | Step 2: Calculate distance from origin                   |
    \*---------------------------------------------------------*/
    float dx = x - origin.x;
    float dy = y - origin.y;
    float dz = z - origin.z;
    float distance = sqrt(dx*dx + dy*dy + dz*dz);

    /*---------------------------------------------------------*\
    | Step 3: Check if within effect boundary (scale)         |
    \*---------------------------------------------------------*/
    float scale_radius = GetNormalizedScale() * 10.0f;
    if(distance > scale_radius)
    {
        return 0x00000000;  // Black - outside effect area
    }

    /*---------------------------------------------------------*\
    | Step 4: Calculate animation progress                     |
    \*---------------------------------------------------------*/
    float progress = CalculateProgress(time);  // Handles speed & reverse

    /*---------------------------------------------------------*\
    | Step 5: Calculate pulse value (0.0 to 1.0)              |
    \*---------------------------------------------------------*/
    float freq = GetScaledFrequency() * 0.1f;
    float pulse_value = sin(distance * freq - progress);
    pulse_value = (pulse_value + 1.0f) * 0.5f;  // Normalize to 0-1

    /*---------------------------------------------------------*\
    | Step 6: Get color (rainbow or user-defined)             |
    \*---------------------------------------------------------*/
    RGBColor color;
    if(GetRainbowMode())
    {
        float hue = (distance * freq + progress * 10.0f);
        hue = fmod(hue, 360.0f);
        color = GetRainbowColor(hue);
    }
    else
    {
        color = GetColorAtPosition(pulse_value);
    }

    /*---------------------------------------------------------*\
    | Step 7: Apply brightness and pulse intensity            |
    \*---------------------------------------------------------*/
    unsigned char r = color & 0xFF;
    unsigned char g = (color >> 8) & 0xFF;
    unsigned char b = (color >> 16) & 0xFF;

    float brightness_factor = (GetBrightness() / 100.0f) * pulse_value;
    r = (unsigned char)(r * brightness_factor);
    g = (unsigned char)(g * brightness_factor);
    b = (unsigned char)(b * brightness_factor);

    /*---------------------------------------------------------*\
    | Step 8: Return final RGB color                          |
    \*---------------------------------------------------------*/
    return (b << 16) | (g << 8) | r;  // 0x00BBGGRR format
}
```

### Step 4: Add to Build System

Edit `OpenRGB3DSpatialPlugin.pro` and add:

```pro
HEADERS += \
    # ... existing headers ...
    Effects3D/Pulse3D/Pulse3D.h

SOURCES += \
    # ... existing sources ...
    Effects3D/Pulse3D/Pulse3D.cpp
```

### Step 5: Add Effect Type Enum

Edit `SpatialEffectTypes.h` and add:

```cpp
enum SpatialEffectType
{
    // ... existing types ...
    SPATIAL_EFFECT_PULSE,
};
```

### Step 6: Build and Test

```bash
qmake OpenRGB3DSpatialPlugin.pro
make
```

Your effect should now appear in the Effects dropdown!

---

## Effect Info Structure

The `EffectInfo3D` structure tells the system about your effect's capabilities.

### Required Fields

```cpp
EffectInfo3D info;

// ALWAYS set to 2 for new effects
info.info_version = 2;

// Basic identification
info.effect_name = "Your Effect Name";        // Class name
info.effect_description = "What it does";     // User-friendly description
info.category = "3D Spatial";                 // Category (always this for now)
info.effect_type = SPATIAL_EFFECT_YOUR_TYPE;  // Enum value
```

### Capabilities

```cpp
// Can direction be reversed?
info.is_reversible = true/false;

// Supports random/rainbow colors?
info.supports_random = true/false;

// Speed range (1-100 typical)
info.max_speed = 100;
info.min_speed = 1;

// Number of user colors (0 = unlimited)
info.user_colors = 0;

// Has custom UI controls?
info.has_custom_settings = true/false;
```

### Requirements

These flags indicate what your effect needs:

```cpp
// Uses an origin point (GetEffectOrigin)?
info.needs_3d_origin = true/false;

// Uses directional axis?
info.needs_direction = true/false;

// Needs thickness parameter? (future)
info.needs_thickness = true/false;

// Needs arms parameter (spiral count)?
info.needs_arms = true/false;

// Uses frequency parameter?
info.needs_frequency = true/false;
```

### Parameter Scaling

Control how parameters are scaled:

```cpp
// Speed multiplier after normalization (default: 10.0)
info.default_speed_scale = 10.0f;

// Frequency multiplier after normalization (default: 10.0)
info.default_frequency_scale = 10.0f;

// Should size parameter be available? (default: true)
info.use_size_parameter = true;
```

**Speed Calculation:**
```cpp
// User slider: 50
// Normalized: (50/100)² = 0.25
// Scaled: 0.25 * default_speed_scale = 2.5
float speed = GetScaledSpeed();  // Returns 2.5
```

### Control Visibility

Hide standard controls if providing custom versions:

```cpp
// Set to false to hide standard control
info.show_speed_control = true;
info.show_brightness_control = true;
info.show_frequency_control = true;
info.show_size_control = true;
info.show_scale_control = true;
info.show_fps_control = true;
info.show_axis_control = true;
info.show_color_controls = true;
```

**Example - Custom Speed Control:**
```cpp
EffectInfo3D GetEffectInfo() {
    EffectInfo3D info;
    // ... other settings ...

    // Hide standard speed, we'll provide custom one
    info.show_speed_control = false;
    info.has_custom_settings = true;

    return info;
}

void SetupCustomUI(QWidget* parent) {
    // Create custom speed control
    QSlider* my_speed = new QSlider(Qt::Horizontal);
    my_speed->setRange(1, 200);  // Different range!
    // ... add to layout ...
}
```

---

## Custom UI Controls

Create effect-specific controls in `SetupCustomUI()`.

### Basic Template

```cpp
void YourEffect::SetupCustomUI(QWidget* parent)
{
    /*---------------------------------------------------------*\
    | Create container widget                                  |
    \*---------------------------------------------------------*/
    QWidget* controls_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(controls_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    /*---------------------------------------------------------*\
    | Add your controls                                        |
    \*---------------------------------------------------------*/
    layout->addWidget(new QLabel("My Parameter:"), 0, 0);

    my_slider = new QSlider(Qt::Horizontal);
    my_slider->setRange(1, 100);
    my_slider->setValue(my_parameter);
    layout->addWidget(my_slider, 0, 1);

    QLabel* value_label = new QLabel(QString::number(my_parameter));
    layout->addWidget(value_label, 0, 2);

    /*---------------------------------------------------------*\
    | Connect signals                                          |
    \*---------------------------------------------------------*/
    connect(my_slider, &QSlider::valueChanged, this,
            &YourEffect::OnParameterChanged);

    connect(my_slider, &QSlider::valueChanged, value_label,
            [value_label](int value) {
                value_label->setText(QString::number(value));
            });

    /*---------------------------------------------------------*\
    | Add to parent layout                                     |
    \*---------------------------------------------------------*/
    if(parent && parent->layout())
    {
        parent->layout()->addWidget(controls_widget);
    }
}
```

### Common Control Types

**Slider:**
```cpp
QSlider* slider = new QSlider(Qt::Horizontal);
slider->setRange(min, max);
slider->setValue(default_value);
```

**Spin Box:**
```cpp
QSpinBox* spinbox = new QSpinBox();
spinbox->setRange(min, max);
spinbox->setValue(default_value);
```

**Combo Box:**
```cpp
QComboBox* combo = new QComboBox();
combo->addItem("Option 1");
combo->addItem("Option 2");
combo->setCurrentIndex(0);
```

**Checkbox:**
```cpp
QCheckBox* checkbox = new QCheckBox("Enable Feature");
checkbox->setChecked(true);
```

### Signal Handling

**Update Parameter:**
```cpp
void YourEffect::OnParameterChanged()
{
    if(my_slider)
    {
        my_parameter = my_slider->value();
    }

    // Notify parent that parameters changed
    emit ParametersChanged();
}
```

**Update Value Label:**
```cpp
connect(slider, &QSlider::valueChanged, label,
        [label](int value) {
            label->setText(QString::number(value));
        });
```

### Example - Multiple Parameters

```cpp
void Spiral3D::SetupCustomUI(QWidget* parent)
{
    QWidget* spiral_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(spiral_widget);

    // Arms slider
    layout->addWidget(new QLabel("Arms:"), 0, 0);
    arms_slider = new QSlider(Qt::Horizontal);
    arms_slider->setRange(1, 12);
    arms_slider->setValue(spiral_arms);
    layout->addWidget(arms_slider, 0, 1);

    arms_value_label = new QLabel(QString::number(spiral_arms));
    layout->addWidget(arms_value_label, 0, 2);

    // Gap slider
    layout->addWidget(new QLabel("Gap:"), 1, 0);
    gap_slider = new QSlider(Qt::Horizontal);
    gap_slider->setRange(0, 100);
    gap_slider->setValue(spiral_gap);
    layout->addWidget(gap_slider, 1, 1);

    gap_value_label = new QLabel(QString::number(spiral_gap));
    layout->addWidget(gap_value_label, 1, 2);

    // Pattern combo
    layout->addWidget(new QLabel("Pattern:"), 2, 0);
    pattern_combo = new QComboBox();
    pattern_combo->addItem("Spiral");
    pattern_combo->addItem("Galaxy");
    pattern_combo->addItem("Tornado");
    layout->addWidget(pattern_combo, 2, 1, 1, 2);

    // Connect signals
    connect(arms_slider, &QSlider::valueChanged,
            this, &Spiral3D::OnSpiralParameterChanged);
    connect(gap_slider, &QSlider::valueChanged,
            this, &Spiral3D::OnSpiralParameterChanged);
    connect(pattern_combo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Spiral3D::OnSpiralParameterChanged);

    // Update labels
    connect(arms_slider, &QSlider::valueChanged, arms_value_label,
            [this](int val) { arms_value_label->setText(QString::number(val)); });
    connect(gap_slider, &QSlider::valueChanged, gap_value_label,
            [this](int val) { gap_value_label->setText(QString::number(val)); });

    if(parent && parent->layout())
    {
        parent->layout()->addWidget(spiral_widget);
    }
}

void Spiral3D::OnSpiralParameterChanged()
{
    if(arms_slider) spiral_arms = arms_slider->value();
    if(gap_slider) spiral_gap = gap_slider->value();
    if(pattern_combo) spiral_pattern = pattern_combo->currentIndex();

    emit ParametersChanged();
}
```

---

## Color Calculation

The heart of your effect - `CalculateColor(x, y, z, time)`.

### Function Signature

```cpp
RGBColor CalculateColor(float x, float y, float z, float time) override;
```

**Parameters:**
- `x, y, z`: LED world position in millimeters
- `time`: Elapsed time in seconds since effect started

**Returns:**
- `RGBColor`: 32-bit color in 0x00BBGGRR format

### Calculation Steps

**1. Get Origin (if needed):**
```cpp
Vector3D origin = GetEffectOrigin();
```

Returns origin based on reference mode:
- Room Center: (0, 0, 0)
- User Position: User reference point
- Custom: Selected reference point

**2. Calculate Relative Position:**
```cpp
float rel_x = x - origin.x;
float rel_y = y - origin.y;
float rel_z = z - origin.z;
```

**3. Calculate Distance:**
```cpp
float distance = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
```

Or for specific axis:
```cpp
float distance;
switch(effect_axis)
{
    case AXIS_X: distance = fabs(rel_x); break;
    case AXIS_Y: distance = fabs(rel_y); break;
    case AXIS_Z: distance = fabs(rel_z); break;
    case AXIS_RADIAL:
    default: distance = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
}
```

**4. Check Boundary (scale):**
```cpp
float scale_radius = GetNormalizedScale() * 10.0f;
if(distance > scale_radius)
{
    return 0x00000000;  // Black - outside effect
}
```

**5. Calculate Progress:**
```cpp
float progress = CalculateProgress(time);
```

**6. Calculate Effect Value:**
```cpp
// Example: Wave effect
float freq = GetScaledFrequency() * 0.1f;
float wave_value = sin(distance * freq - progress);

// Normalize to 0-1
wave_value = (wave_value + 1.0f) * 0.5f;
```

**7. Get Color:**
```cpp
RGBColor color;
if(GetRainbowMode())
{
    float hue = wave_value * 360.0f;
    color = GetRainbowColor(hue);
}
else
{
    color = GetColorAtPosition(wave_value);
}
```

**8. Apply Brightness:**
```cpp
unsigned char r = color & 0xFF;
unsigned char g = (color >> 8) & 0xFF;
unsigned char b = (color >> 16) & 0xFF;

float brightness_factor = (GetBrightness() / 100.0f) * wave_value;
r = (unsigned char)(r * brightness_factor);
g = (unsigned char)(g * brightness_factor);
b = (unsigned char)(b * brightness_factor);

return (b << 16) | (g << 8) | r;
```

### Helper Functions

**EffectHelpers.h:**
```cpp
#include "../EffectHelpers.h"

// Smooth interpolation (0-1)
float t = smoothstep(edge0, edge1, x);

// Linear interpolation
float result = lerp(a, b, t);

// Clamp value to range
float clamped = clamp(value, min_val, max_val);
```

**Color Helpers:**
```cpp
// Rainbow color (hue 0-360)
RGBColor color = GetRainbowColor(hue);

// Interpolate through user color list (position 0-1)
RGBColor color = GetColorAtPosition(position);
```

**Math Constants:**
```cpp
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static constexpr float TWO_PI = 6.28318530718f;
static constexpr float RAD_TO_DEG = 57.2957795131f;
static constexpr float DEG_TO_RAD = 0.01745329251f;
```

### Performance Tips

**Avoid Expensive Operations:**
```cpp
// BAD: Multiple sqrt per LED
float dist1 = sqrt(...);
float dist2 = sqrt(...);

// GOOD: Calculate once, reuse
float distance = sqrt(...);
```

**Pre-compute Constants:**
```cpp
// BAD: Recalculate each LED
float freq = GetScaledFrequency() * 0.1f;

// GOOD: Calculate once before loop
float freq = GetScaledFrequency() * 0.1f;
for each LED:
    color = CalculateColor(x, y, z, time);  // Uses pre-computed freq
```

**Use Lookup Tables for Complex Math:**
```cpp
// For very expensive operations
static std::vector<float> sin_table(360);
static bool initialized = false;
if(!initialized)
{
    for(int i = 0; i < 360; i++)
    {
        sin_table[i] = sin(i * DEG_TO_RAD);
    }
    initialized = true;
}

// Then use table instead of sin()
int index = (int)(angle) % 360;
float sin_value = sin_table[index];
```

**Early Exit:**
```cpp
// Skip expensive calculations if outside range
if(distance > max_radius) return 0x00000000;

// Skip if zero brightness
if(GetBrightness() == 0) return 0x00000000;
```

### Example Patterns

**Radial Pulse:**
```cpp
float pulse = sin(distance * frequency - progress);
pulse = (pulse + 1.0f) * 0.5f;  // 0-1
```

**Directional Wave:**
```cpp
float wave = sin(position_on_axis * frequency - progress);
```

**Spiral:**
```cpp
float angle = atan2(rel_y, rel_x);
float spiral = sin(angle * arms + distance * frequency - progress);
```

**Plasma:**
```cpp
float plasma = 0.0f;
plasma += sin(x * 0.1f + time);
plasma += sin(y * 0.1f + time * 1.3f);
plasma += sin(distance * 0.1f + time * 0.7f);
plasma = (plasma + 3.0f) / 6.0f;  // Normalize
```

---

## Parameter System

### Normalized Parameters

The base class provides normalized parameter getters with consistent curves:

**Speed (Quadratic Curve):**
```cpp
float norm_speed = GetNormalizedSpeed();
// User slider 50 → (50/100)² = 0.25
```

**Frequency (Quadratic Curve):**
```cpp
float norm_freq = GetNormalizedFrequency();
// User slider 75 → (75/100)² = 0.5625
```

**Size (Linear, 0.1-2.0):**
```cpp
float size_mult = GetNormalizedSize();
// User slider 1 → 0.1x
// User slider 50 → 1.0x
// User slider 100 → 2.0x
```

**Scale (Linear, 0.1-2.0):**
```cpp
float scale_mult = GetNormalizedScale();
// User slider 10 → 0.1x
// User slider 100 → 1.0x
// User slider 200 → 2.0x
```

### Scaled Parameters

Apply effect-specific multipliers:

```cpp
float speed = GetScaledSpeed();
// Returns GetNormalizedSpeed() * default_speed_scale

float freq = GetScaledFrequency();
// Returns GetNormalizedFrequency() * default_frequency_scale
```

**Setting Scales in EffectInfo:**
```cpp
info.default_speed_scale = 20.0f;      // Fast effect
info.default_frequency_scale = 5.0f;   // Sparse patterns
```

### Progress Calculator

Use this for animation:

```cpp
float progress = CalculateProgress(time);
// Returns time * GetScaledSpeed()
// Handles reverse direction automatically
```

**Usage:**
```cpp
float wave = sin(distance * frequency - progress);
```

### Custom Parameters

If you need custom parameters:

**1. Add Member Variables:**
```cpp
class YourEffect : public SpatialEffect3D {
private:
    unsigned int my_parameter;
    QSlider* my_parameter_slider;
};
```

**2. Initialize in Constructor:**
```cpp
YourEffect::YourEffect(QWidget* parent) : SpatialEffect3D(parent)
{
    my_parameter = 50;  // Default value
    my_parameter_slider = nullptr;
}
```

**3. Create UI:**
```cpp
void YourEffect::SetupCustomUI(QWidget* parent)
{
    my_parameter_slider = new QSlider(Qt::Horizontal);
    my_parameter_slider->setRange(1, 100);
    my_parameter_slider->setValue(my_parameter);

    connect(my_parameter_slider, &QSlider::valueChanged,
            this, &YourEffect::OnParameterChanged);

    // ... add to layout ...
}
```

**4. Update Parameter:**
```cpp
void YourEffect::OnParameterChanged()
{
    if(my_parameter_slider)
    {
        my_parameter = my_parameter_slider->value();
    }
    emit ParametersChanged();
}
```

**5. Use in CalculateColor:**
```cpp
RGBColor YourEffect::CalculateColor(float x, float y, float z, float time)
{
    float my_value = my_parameter * 0.01f;  // Normalize to 0-1
    // ... use my_value in calculations ...
}
```

---

## Testing Your Effect

### Manual Testing Checklist

**Basic Functionality:**
- [ ] Effect appears in dropdown
- [ ] Effect starts without crashing
- [ ] LEDs update correctly
- [ ] Effect stops cleanly

**Parameter Testing:**
- [ ] Speed slider affects animation speed
- [ ] Brightness slider affects intensity
- [ ] Frequency slider changes pattern density
- [ ] Size slider scales pattern elements
- [ ] Scale slider affects coverage area
- [ ] FPS limiter works (1 FPS should be slow)

**Axis Testing:**
- [ ] X-axis: Effect moves left-to-right
- [ ] Y-axis: Effect moves front-to-back
- [ ] Z-axis: Effect moves floor-to-ceiling
- [ ] Radial: Effect expands from origin
- [ ] Reverse checkbox inverts direction

**Color Testing:**
- [ ] Rainbow mode cycles colors
- [ ] Can add/remove user colors
- [ ] User colors interpolate smoothly
- [ ] Brightness affects all colors equally

**Reference Point Testing:**
- [ ] Room Center origin works
- [ ] User Position origin works
- [ ] Custom reference points work
- [ ] Effect recalculates when origin changes

**Zone Testing:**
- [ ] "All Controllers" applies to all
- [ ] Specific zones work correctly
- [ ] Zone filtering improves performance

**Performance Testing:**
- [ ] <100 LEDs: Smooth at 60 FPS
- [ ] 100-300 LEDs: Smooth at 30 FPS
- [ ] 300-500 LEDs: Acceptable at 30 FPS
- [ ] 500+ LEDs: Reduce FPS or optimize

### Debug Tips

**Check Effect Registration:**
```cpp
// Should see in OpenRGB logs:
// "Registered effect: YourEffect"
```

**Print Debug Info:**
```cpp
// ONLY during development, remove before commit!
#include "LogManager.h"

RGBColor YourEffect::CalculateColor(float x, float y, float z, float time)
{
    static int call_count = 0;
    if(call_count++ % 100 == 0)  // Print every 100 calls
    {
        LOG_DEBUG("Effect calculate: x=%.1f y=%.1f z=%.1f time=%.2f",
                  x, y, z, time);
    }
    // ... rest of function ...
}
```

**Verify Parameter Values:**
```cpp
LOG_DEBUG("Speed: %u, Normalized: %.2f, Scaled: %.2f",
          GetSpeed(), GetNormalizedSpeed(), GetScaledSpeed());
```

**Test with Single LED:**
Start with one LED to verify math before scaling up.

**Use Debugger:**
- Set breakpoint in `CalculateColor()`
- Inspect x, y, z, time values
- Step through calculations
- Watch color output

### Common Issues

**Effect doesn't appear:**
- Check `REGISTER_EFFECT_3D()` macro used
- Verify effect added to .pro file
- Check for compile errors
- Ensure effect type enum added

**Wrong colors:**
- Verify RGB format (0x00BBGGRR not 0x00RRGGBB)
- Check bit shifting: `(b << 16) | (g << 8) | r`
- Ensure values are in 0-255 range

**Effect is static (doesn't animate):**
- Check if using `time` parameter
- Verify `CalculateProgress()` called
- Ensure progress used in calculations

**Effect is too fast/slow:**
- Adjust `default_speed_scale` in EffectInfo
- Check speed calculation in CalculateColor

**Performance issues:**
- Remove expensive operations (sqrt, trig)
- Pre-compute constants
- Use lookup tables
- Early exit when possible

---

## Best Practices

### Code Style

**Follow OpenRGB Standards:**
- C-style conventions (no `auto`, indexed loops)
- snake_case for variables, CamelCase for functions/classes
- 4 spaces indentation (spaces, not tabs)
- Opening braces on own lines
- No `printf`/`cout` - use LogManager (only for debugging)
- No `QDebug` - use LogManager

**Example:**
```cpp
// GOOD
for(unsigned int i = 0; i < led_count; i++)
{
    float distance = CalculateDistance(leds[i]);
    if(distance > max_radius)
    {
        continue;
    }
    // ...
}

// BAD
for (auto& led : leds) {  // Don't use auto or range-for
  auto dist = calc(led);   // Don't use auto
  if (dist > max) continue; // Space after if, brace on same line
}
```

### Performance

**Optimize Hot Paths:**
- `CalculateColor()` is called for every LED every frame
- 500 LEDs * 30 FPS = 15,000 calls/second
- Each optimization matters!

**Pre-compute:**
```cpp
// BAD
RGBColor CalculateColor(float x, float y, float z, float time)
{
    float freq = GetScaledFrequency() * 0.1f;  // Recalculated every LED!
    // ...
}

// GOOD
class YourEffect {
private:
    float cached_freq;

    void OnParameterChanged() {
        cached_freq = GetScaledFrequency() * 0.1f;  // Calculate once
    }
};

RGBColor CalculateColor(float x, float y, float z, float time)
{
    // Use cached value
    float wave = sin(distance * cached_freq - progress);
}
```

**Avoid Divisions:**
```cpp
// BAD
float value = x / 10.0f;

// GOOD
float inv_10 = 0.1f;  // Pre-compute reciprocal
float value = x * inv_10;
```

**Minimize Branching:**
```cpp
// BAD: Branch every LED
if(some_condition)
    calculate_one_way();
else
    calculate_other_way();

// GOOD: Branch once, calculate many
if(some_condition)
{
    for_each_LED: calculate_one_way();
}
else
{
    for_each_LED: calculate_other_way();
}
```

### User Experience

**Sensible Defaults:**
```cpp
YourEffect::YourEffect(QWidget* parent) : SpatialEffect3D(parent)
{
    SetSpeed(50);           // Medium speed (not too fast/slow)
    SetBrightness(100);     // Full brightness
    SetFrequency(30);       // Balanced pattern density
    SetRainbowMode(true);   // Rainbow is visually appealing
}
```

**Clear Parameter Names:**
```cpp
// GOOD
layout->addWidget(new QLabel("Wave Thickness:"), 0, 0);
layout->addWidget(new QLabel("Spiral Arms:"), 1, 0);

// BAD
layout->addWidget(new QLabel("Param1:"), 0, 0);
layout->addWidget(new QLabel("Val:"), 1, 0);
```

**Descriptive Effect Info:**
```cpp
info.effect_name = "3D Wave";  // Not "Wave" (be specific)
info.effect_description = "Expanding circular waves from origin point";
// Not "Wave effect" (describe what it does)
```

**Responsive Parameters:**
- Changes should be visible immediately
- Extreme values should still look good
- No dead zones where parameter has no effect

### Documentation

**Header Comments:**
```cpp
/*---------------------------------------------------------*\
| YourEffect.h                                              |
|                                                           |
|   Brief description of what this effect does            |
|                                                           |
|   Date: YYYY-MM-DD                                        |
|   Author: Your Name                                       |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/
```

**Function Comments:**
```cpp
/*---------------------------------------------------------*\
| Calculate wave intensity at position                     |
| Returns value from 0.0 (no wave) to 1.0 (peak)          |
\*---------------------------------------------------------*/
float CalculateWaveIntensity(float distance, float time)
{
    // ...
}
```

**Complex Math:**
```cpp
// Calculate spiral angle using atan2
// Range: -π to +π, normalized to 0-1
float angle = atan2(rel_y, rel_x);
float normalized_angle = (angle + M_PI) / TWO_PI;
```

---

## Example Effects

### Simple Effect - Solid Color Fade

```cpp
RGBColor SolidFade::CalculateColor(float x, float y, float z, float time)
{
    (void)x; (void)y; (void)z;  // Unused - same color for all LEDs

    // Oscillate brightness 0-1
    float progress = CalculateProgress(time);
    float intensity = (sin(progress) + 1.0f) * 0.5f;

    // Apply to color
    RGBColor color = GetRainbowMode() ?
        GetRainbowColor(progress * 360.0f) :
        GetColorAtPosition(0.0f);  // First user color

    unsigned char r = (color & 0xFF) * intensity;
    unsigned char g = ((color >> 8) & 0xFF) * intensity;
    unsigned char b = ((color >> 16) & 0xFF) * intensity;

    return (b << 16) | (g << 8) | r;
}
```

### Medium Complexity - Directional Wave

```cpp
RGBColor DirectionalWave::CalculateColor(float x, float y, float z, float time)
{
    // Get position along selected axis
    float position;
    switch(effect_axis)
    {
        case AXIS_X: position = x; break;
        case AXIS_Y: position = y; break;
        case AXIS_Z: position = z; break;
        case AXIS_RADIAL:
        {
            Vector3D origin = GetEffectOrigin();
            float dx = x - origin.x;
            float dy = y - origin.y;
            float dz = z - origin.z;
            position = sqrt(dx*dx + dy*dy + dz*dz);
            break;
        }
    }

    // Calculate wave
    float progress = CalculateProgress(time);
    float freq = GetScaledFrequency() * 0.1f;
    float wave = sin(position * freq - progress);

    // Normalize to 0-1
    wave = (wave + 1.0f) * 0.5f;

    // Get color
    RGBColor color = GetRainbowMode() ?
        GetRainbowColor(wave * 360.0f) :
        GetColorAtPosition(wave);

    // Apply brightness
    float brightness = (GetBrightness() / 100.0f) * wave;
    unsigned char r = (color & 0xFF) * brightness;
    unsigned char g = ((color >> 8) & 0xFF) * brightness;
    unsigned char b = ((color >> 16) & 0xFF) * brightness;

    return (b << 16) | (g << 8) | r;
}
```

### Advanced - Spiral with Arms

```cpp
RGBColor Spiral3D::CalculateColor(float x, float y, float z, float time)
{
    Vector3D origin = GetEffectOrigin();

    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    // Calculate cylindrical coordinates
    float distance = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
    float angle = atan2(rel_y, rel_x);  // -π to +π

    // Boundary check
    float scale_radius = GetNormalizedScale() * 10.0f;
    if(distance > scale_radius) return 0x00000000;

    // Calculate spiral
    float progress = CalculateProgress(time);
    float freq = GetScaledFrequency() * 0.1f;

    // Spiral equation: combine angle and distance
    float spiral_value = sin(angle * spiral_arms + distance * freq - progress);

    // Add radial fade
    float fade = 1.0f - (distance / scale_radius);
    spiral_value *= fade;

    // Normalize
    spiral_value = (spiral_value + 1.0f) * 0.5f;

    // Color
    RGBColor color;
    if(GetRainbowMode())
    {
        float hue = (angle / TWO_PI) * 360.0f + progress * 10.0f;
        hue = fmod(hue, 360.0f);
        color = GetRainbowColor(hue);
    }
    else
    {
        color = GetColorAtPosition(spiral_value);
    }

    // Brightness
    float brightness = (GetBrightness() / 100.0f) * spiral_value;
    unsigned char r = (color & 0xFF) * brightness;
    unsigned char g = ((color >> 8) & 0xFF) * brightness;
    unsigned char b = ((color >> 16) & 0xFF) * brightness;

    return (b << 16) | (g << 8) | r;
}
```

---

## Troubleshooting

### Compilation Errors

**"undefined reference to vtable":**
- Missing `Q_OBJECT` macro in class definition
- Forgot to run `qmake` after adding new effect
- Solution: Run `qmake` then `make clean && make`

**"no matching function for call":**
- Check function signature matches base class
- Ensure `override` keyword used
- Verify parameter types exactly match

**"forward declaration of 'class QWidget'":**
- Missing include: `#include <QWidget>`
- Check all Qt includes in header

### Runtime Issues

**Effect doesn't appear in dropdown:**
```cpp
// Check:
1. EFFECT_REGISTERER_3D macro in header
2. REGISTER_EFFECT_3D(YourEffect) in .cpp
3. Effect added to .pro file
4. Recompiled successfully
```

**Crash on effect start:**
```cpp
// Common causes:
1. Uninitialized pointer (set to nullptr in constructor)
2. Division by zero
3. Invalid array access
4. Check OpenRGB logs for crash location
```

**Colors wrong/inverted:**
```cpp
// Check RGB order: 0x00BBGGRR
RGBColor color = (b << 16) | (g << 8) | r;  // Correct

// NOT:
RGBColor color = (r << 16) | (g << 8) | b;  // Wrong!
```

**Effect not animating:**
```cpp
// Must use time parameter:
float progress = CalculateProgress(time);
float wave = sin(position - progress);  // Correct

// NOT:
float wave = sin(position);  // Static, no animation
```

**Parameters don't work:**
```cpp
// Check:
1. Signal connected: connect(slider, SIGNAL, this, SLOT)
2. Parameter updated in slot
3. ParametersChanged() emitted
4. Parameter actually used in CalculateColor()
```

### Performance Issues

**Effect is choppy/laggy:**
```cpp
// Solutions:
1. Reduce FPS (try 15-20)
2. Pre-compute expensive operations
3. Use lookup tables for sin/cos
4. Early exit when possible
5. Profile with debugger to find bottleneck
```

**High CPU usage:**
```cpp
// Check for:
1. Expensive operations in tight loops
2. Unnecessary recalculations
3. Complex branching
4. Try reducing LED count to isolate issue
```

### Visual Issues

**Effect looks wrong:**
```cpp
// Debug steps:
1. Test with single LED
2. Print x, y, z, time values
3. Print intermediate calculations
4. Verify math with calculator
5. Compare to working effect
```

**Flickering:**
```cpp
// Common causes:
1. Value oscillating around threshold
2. Division by zero creating NaN
3. Values outside 0-255 range
4. Use smoothstep() for smooth transitions
```

**Discontinuities:**
```cpp
// Use fmod() for wrapping:
float angle = fmod(angle, TWO_PI);

// Use smoothstep() for smooth edges:
float fade = smoothstep(edge0, edge1, x);
```

---

## Advanced Topics

### Grid-Based Calculation (Optional)

For performance, implement `CalculateColorGrid()`:

```cpp
RGBColor CalculateColorGrid(float x, float y, float z, float time,
                           const GridContext3D& grid) override
{
    // grid.min_x, grid.max_x (etc) available
    // grid.width, grid.height, grid.depth

    // Can optimize based on grid bounds
    // Default implementation just calls CalculateColor()
}
```

### Multi-Point Effects (Future)

Planned support for effects using multiple reference points:

```cpp
// Future API (not yet implemented):
std::vector<Vector3D> points = GetReferencePoints();
for(auto& point : points)
{
    // Calculate influence from each point
}
```

### Effect Chaining (Future)

Planned support for combining effects:

```cpp
// Future API:
RGBColor base_color = PreviousEffect::CalculateColor(x, y, z, time);
// Modify base_color with this effect
```

---

## Submitting Your Effect

### Before Submitting

**Checklist:**
- [ ] Code follows OpenRGB style guidelines
- [ ] No debug printf/cout/qDebug statements
- [ ] Comments explain complex math
- [ ] Tested with 10, 100, 500 LEDs
- [ ] Tested all parameters (min, mid, max values)
- [ ] Tested with rainbow and custom colors
- [ ] Tested all axes (X, Y, Z, Radial)
- [ ] Tested reverse direction
- [ ] Tested with different reference points
- [ ] Tested with zone filtering
- [ ] No compilation warnings
- [ ] Effect has clear name and description
- [ ] Default parameters provide good visual

### Pull Request

1. Fork repository
2. Create feature branch: `git checkout -b feature/my-effect`
3. Add your effect files
4. Update .pro file
5. Commit with clear message
6. Test build on your platform
7. Push to your fork
8. Open pull request with:
   - Effect description
   - Screenshot/video (if possible)
   - Testing details
   - Any special notes

### Effect Showcase

Include in PR description:
- **Name**: What it's called
- **Description**: What it does
- **Inspiration**: What inspired the effect
- **Parameters**: Custom parameters (if any)
- **Performance**: Tested LED counts
- **Visual**: Screenshot or video link

**Example:**
```markdown
## 3D Tornado Effect

Rotating tornado funnel with configurable intensity and height.

**Inspiration:** Weather phenomena, particle vortex systems

**Custom Parameters:**
- Funnel Width (1-100): Controls tornado diameter
- Spin Speed (1-100): Rotation speed independent of height movement

**Performance:**
- Tested up to 800 LEDs at 30 FPS
- Uses cached sin/cos lookups for performance

**Visual:** [Link to video/GIF]
```

---

## Resources

**Documentation:**
- [OpenRGB Contributing Guide](../../CONTRIBUTING.md)
- [RGB Controller API](../../RGBControllerAPI.md)
- [Plugin README](README.md)

**Example Effects:**
- `Effects3D/Wave3D/` - Simple radial wave
- `Effects3D/Spiral3D/` - Complex spiral with arms
- `Effects3D/Plasma3D/` - Organic noise-based effect
- `Effects3D/Explosion3D/` - Multi-wave radial effect

**Qt Documentation:**
- [Qt Widgets](https://doc.qt.io/qt-5/qtwidgets-index.html)
- [Signals & Slots](https://doc.qt.io/qt-5/signalsandslots.html)
- [Layouts](https://doc.qt.io/qt-5/layout.html)

**Math References:**
- [Trigonometry Cheat Sheet](https://www.mathsisfun.com/algebra/trigonometry.html)
- [Smoothstep Function](https://en.wikipedia.org/wiki/Smoothstep)
- [HSV Color Model](https://en.wikipedia.org/wiki/HSL_and_HSV)

**Community:**
- [OpenRGB Discord](https://discord.gg/AQwjJPY)
- [OpenRGB GitLab](https://gitlab.com/CalcProgrammer1/OpenRGB)
- [Plugin Issues](https://github.com/Wolfieeewolf/OpenRGB3DSpatialPlugin/issues)

---

## Conclusion

You now have everything you need to create amazing 3D spatial effects! The base class handles the heavy lifting - you just focus on the creative color calculation.

**Remember:**
- Keep `CalculateColor()` fast (it's called 15,000+ times/second)
- Test with varying LED counts
- Follow OpenRGB coding standards
- Comment complex math
- Have fun! 🎨

**Questions?** Open an issue or ask in the OpenRGB Discord.

**Happy Effect Creating!** ✨

---

*This guide is part of the OpenRGB 3D Spatial Plugin project*

*Created with assistance from Claude Code (Anthropic AI)*
