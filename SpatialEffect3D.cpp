/*---------------------------------------------------------*\
| SpatialEffect3D.cpp                                       |
|                                                           |
|   Base class for 3D spatial effects with custom UI      |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "SpatialEffect3D.h"
#include "Colors.h"
#include <cmath>

SpatialEffect3D::SpatialEffect3D(QWidget* parent) : QWidget(parent)
{
    effect_enabled = false;
    effect_running = false;
    effect_speed = 1;
    effect_brightness = 100;
    effect_frequency = 1;
    effect_size = 50;
    effect_scale = 200;  // Default to 200 (100% of room - whole room coverage)
    effect_fps = 30;
    rainbow_mode = false;
    rainbow_progress = 0.0f;
    boundary_prevalidated = false;

    // Initialize axis parameters
    effect_axis = AXIS_RADIAL;
    effect_reverse = false;

    // Initialize reference point parameters
    reference_mode = REF_MODE_ROOM_CENTER;
    global_reference_point = {0.0f, 0.0f, 0.0f};
    custom_reference_point = {0.0f, 0.0f, 0.0f};
    use_custom_reference = false;

    // Initialize default colors
    colors.push_back(COLOR_RED);
    colors.push_back(COLOR_BLUE);

    effect_controls_group = nullptr;
    speed_slider = nullptr;
    brightness_slider = nullptr;
    frequency_slider = nullptr;
    size_slider = nullptr;
    scale_slider = nullptr;
    fps_slider = nullptr;
    speed_label = nullptr;
    brightness_label = nullptr;
    frequency_label = nullptr;
    size_label = nullptr;
    scale_label = nullptr;
    fps_label = nullptr;

    // Axis controls
    axis_combo = nullptr;
    reverse_check = nullptr;

    // Color controls
    color_controls_group = nullptr;
    rainbow_mode_check = nullptr;
    color_buttons_widget = nullptr;
    color_buttons_layout = nullptr;
    add_color_button = nullptr;
    remove_color_button = nullptr;

    // Effect control buttons
    start_effect_button = nullptr;
    stop_effect_button = nullptr;
}

SpatialEffect3D::~SpatialEffect3D()
{
}

void SpatialEffect3D::CreateCommonEffectControls(QWidget* parent)
{
    effect_controls_group = new QGroupBox("Effect Controls");
    QVBoxLayout* main_layout = new QVBoxLayout();

    /*---------------------------------------------------------*\
    | Effect Control Buttons                                   |
    \*---------------------------------------------------------*/
    QHBoxLayout* button_layout = new QHBoxLayout();
    start_effect_button = new QPushButton("Start Effect");
    start_effect_button->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");
    stop_effect_button = new QPushButton("Stop Effect");
    stop_effect_button->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; }");
    stop_effect_button->setEnabled(false);

    button_layout->addWidget(start_effect_button);
    button_layout->addWidget(stop_effect_button);
    button_layout->addStretch();
    main_layout->addLayout(button_layout);

    /*---------------------------------------------------------*\
    | Speed control (with logarithmic curve)                   |
    \*---------------------------------------------------------*/
    QHBoxLayout* speed_layout = new QHBoxLayout();
    speed_layout->addWidget(new QLabel("Speed:"));
    speed_slider = new QSlider(Qt::Horizontal);
    speed_slider->setRange(1, 100);
    speed_slider->setValue(effect_speed);
    speed_slider->setToolTip("Effect animation speed (uses logarithmic curve for smooth control)");
    speed_layout->addWidget(speed_slider);
    speed_label = new QLabel(QString::number(effect_speed));
    speed_label->setMinimumWidth(30);
    speed_layout->addWidget(speed_label);
    main_layout->addLayout(speed_layout);

    /*---------------------------------------------------------*\
    | Brightness control                                       |
    \*---------------------------------------------------------*/
    QHBoxLayout* brightness_layout = new QHBoxLayout();
    brightness_layout->addWidget(new QLabel("Brightness:"));
    brightness_slider = new QSlider(Qt::Horizontal);
    brightness_slider->setRange(1, 100);
    brightness_slider->setValue(effect_brightness);
    brightness_layout->addWidget(brightness_slider);
    brightness_label = new QLabel(QString::number(effect_brightness));
    brightness_label->setMinimumWidth(30);
    brightness_layout->addWidget(brightness_label);
    main_layout->addLayout(brightness_layout);

    /*---------------------------------------------------------*\
    | Frequency control                                        |
    \*---------------------------------------------------------*/
    QHBoxLayout* frequency_layout = new QHBoxLayout();
    frequency_layout->addWidget(new QLabel("Frequency:"));
    frequency_slider = new QSlider(Qt::Horizontal);
    frequency_slider->setRange(1, 100);
    frequency_slider->setValue(effect_frequency);
    frequency_slider->setToolTip("Pattern frequency - controls color wave/pattern density");
    frequency_layout->addWidget(frequency_slider);
    frequency_label = new QLabel(QString::number(effect_frequency));
    frequency_label->setMinimumWidth(30);
    frequency_layout->addWidget(frequency_label);
    main_layout->addLayout(frequency_layout);

    /*---------------------------------------------------------*\
    | Size control (pattern density)                          |
    \*---------------------------------------------------------*/
    QHBoxLayout* size_layout = new QHBoxLayout();
    size_layout->addWidget(new QLabel("Size:"));
    size_slider = new QSlider(Qt::Horizontal);
    size_slider->setRange(1, 100);
    size_slider->setValue(effect_size);
    size_slider->setToolTip("Pattern size - controls how tight/spread out the pattern is");
    size_layout->addWidget(size_slider);
    size_label = new QLabel(QString::number(effect_size));
    size_label->setMinimumWidth(30);
    size_layout->addWidget(size_label);
    main_layout->addLayout(size_layout);

    /*---------------------------------------------------------*\
    | Scale control (area coverage)                            |
    \*---------------------------------------------------------*/
    QHBoxLayout* scale_layout = new QHBoxLayout();
    scale_layout->addWidget(new QLabel("Scale:"));
    scale_slider = new QSlider(Qt::Horizontal);
    scale_slider->setRange(0, 250);
    scale_slider->setValue(effect_scale);
    scale_slider->setToolTip("Effect coverage: 0-200 = 0-100% of room (200=whole room), 201-250 = 101-150% (beyond room)");
    scale_layout->addWidget(scale_slider);
    scale_label = new QLabel(QString::number(effect_scale));
    scale_label->setMinimumWidth(30);
    scale_layout->addWidget(scale_label);
    main_layout->addLayout(scale_layout);

    /*---------------------------------------------------------*\
    | FPS Limiter control                                      |
    \*---------------------------------------------------------*/
    QHBoxLayout* fps_layout = new QHBoxLayout();
    fps_layout->addWidget(new QLabel("FPS:"));
    fps_slider = new QSlider(Qt::Horizontal);
    fps_slider->setRange(1, 60);
    fps_slider->setValue(effect_fps);
    fps_slider->setToolTip("Frames per second (1-60) - lower values reduce CPU usage");
    fps_layout->addWidget(fps_slider);
    fps_label = new QLabel(QString::number(effect_fps));
    fps_label->setMinimumWidth(30);
    fps_layout->addWidget(fps_label);
    main_layout->addLayout(fps_layout);

    /*---------------------------------------------------------*\
    | Axis control                                             |
    \*---------------------------------------------------------*/
    QHBoxLayout* axis_layout = new QHBoxLayout();
    axis_layout->addWidget(new QLabel("Axis:"));
    axis_combo = new QComboBox();
    axis_combo->addItem("None (Effect Default)");      // index 0 -> no override
    axis_combo->addItem("X-Axis (Left to Right)");    // AXIS_X = 0 (grid X)
    axis_combo->addItem("Y-Axis (Front to Back)");    // AXIS_Y = 1 (grid Y)
    axis_combo->addItem("Z-Axis (Bottom to Top)");    // AXIS_Z = 2 (grid Z)
    axis_combo->addItem("Radial (Outward from Center)"); // AXIS_RADIAL = 3
    axis_combo->setToolTip(
        "Axis mapping uses grid coordinates:\n"
        "X: Left→Right, Y: Front→Back, Z: Bottom→Top.\n"
        "Radial: distance from effect origin (room/user)."
    );
    axis_none = true; // default to no axis override
    axis_combo->setCurrentIndex(axis_none ? 0 : (int)effect_axis + 1);
    axis_layout->addWidget(axis_combo);

    reverse_check = new QCheckBox("Reverse");
    reverse_check->setToolTip("Reverse direction along the selected axis");
    reverse_check->setChecked(effect_reverse);
    axis_layout->addWidget(reverse_check);
    axis_layout->addStretch();
    main_layout->addLayout(axis_layout);

    /*---------------------------------------------------------*\
    | Create and add color controls                            |
    \*---------------------------------------------------------*/
    CreateColorControls();
    main_layout->addWidget(color_controls_group);

    effect_controls_group->setLayout(main_layout);

    /*---------------------------------------------------------*\
    | Connect signals                                          |
    \*---------------------------------------------------------*/
    connect(speed_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(brightness_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(frequency_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(size_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(scale_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(fps_slider, &QSlider::valueChanged, this, &SpatialEffect3D::OnParameterChanged);
    connect(axis_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpatialEffect3D::OnAxisChanged);
    connect(reverse_check, &QCheckBox::toggled, this, &SpatialEffect3D::OnReverseChanged);

    // Effect control buttons - NOT connected here!
    // The parent tab needs to connect these to its on_start_effect_clicked/on_stop_effect_clicked handlers
    // to actually start the effect timer

    /*---------------------------------------------------------*\
    | Apply control visibility based on effect info            |
    \*---------------------------------------------------------*/
    ApplyControlVisibility();

    /*---------------------------------------------------------*\
    | Update labels when sliders change                        |
    \*---------------------------------------------------------*/
    connect(speed_slider, &QSlider::valueChanged, speed_label, [this](int value) {
        speed_label->setText(QString::number(value));
        effect_speed = value;
    });
    connect(brightness_slider, &QSlider::valueChanged, brightness_label, [this](int value) {
        brightness_label->setText(QString::number(value));
        effect_brightness = value;
    });
    connect(frequency_slider, &QSlider::valueChanged, frequency_label, [this](int value) {
        frequency_label->setText(QString::number(value));
        effect_frequency = value;
    });
    connect(size_slider, &QSlider::valueChanged, size_label, [this](int value) {
        size_label->setText(QString::number(value));
        effect_size = value;
    });
    connect(scale_slider, &QSlider::valueChanged, scale_label, [this](int value) {
        scale_label->setText(QString::number(value));
        effect_scale = value;
    });
    connect(fps_slider, &QSlider::valueChanged, fps_label, [this](int value) {
        fps_label->setText(QString::number(value));
        effect_fps = value;
    });

    /*---------------------------------------------------------*\
    | Add to parent layout                                     |
    \*---------------------------------------------------------*/
    if(parent && parent->layout())
    {
        parent->layout()->addWidget(effect_controls_group);
    }
}

/*---------------------------------------------------------*\
| Create Color Controls                                    |
\*---------------------------------------------------------*/
void SpatialEffect3D::CreateColorControls()
{
    color_controls_group = new QGroupBox("Colors");
    QVBoxLayout* color_layout = new QVBoxLayout();

    /*---------------------------------------------------------*\
    | Rainbow Mode Toggle                                      |
    \*---------------------------------------------------------*/
    rainbow_mode_check = new QCheckBox("Rainbow Mode");
    rainbow_mode_check->setChecked(rainbow_mode);
    color_layout->addWidget(rainbow_mode_check);

    /*---------------------------------------------------------*\
    | Color Buttons Container                                  |
    \*---------------------------------------------------------*/
    color_buttons_widget = new QWidget();
    color_buttons_layout = new QHBoxLayout();
    color_buttons_widget->setLayout(color_buttons_layout);

    // Create initial color buttons
    for(unsigned int i = 0; i < colors.size(); i++)
    {
        CreateColorButton(colors[i]);
    }

    /*---------------------------------------------------------*\
    | Add/Remove Color Buttons                                 |
    \*---------------------------------------------------------*/
    add_color_button = new QPushButton("+");
    add_color_button->setMaximumSize(30, 30);
    add_color_button->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");

    remove_color_button = new QPushButton("-");
    remove_color_button->setMaximumSize(30, 30);
    remove_color_button->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; }");
    remove_color_button->setEnabled(colors.size() > 1);

    color_buttons_layout->addWidget(add_color_button);
    color_buttons_layout->addWidget(remove_color_button);
    color_buttons_layout->addStretch();

    color_layout->addWidget(color_buttons_widget);
    color_controls_group->setLayout(color_layout);

    // Hide color buttons when rainbow mode is enabled
    color_buttons_widget->setVisible(!rainbow_mode);

    /*---------------------------------------------------------*\
    | Connect signals                                          |
    \*---------------------------------------------------------*/
    connect(rainbow_mode_check, &QCheckBox::toggled, this, &SpatialEffect3D::OnRainbowModeChanged);
    connect(add_color_button, &QPushButton::clicked, this, &SpatialEffect3D::OnAddColorClicked);
    connect(remove_color_button, &QPushButton::clicked, this, &SpatialEffect3D::OnRemoveColorClicked);
}

void SpatialEffect3D::CreateColorButton(RGBColor color)
{
    QPushButton* color_button = new QPushButton();
    color_button->setMinimumSize(40, 30);
    color_button->setMaximumSize(40, 30);
    color_button->setStyleSheet(QString("background-color: rgb(%1, %2, %3); border: 1px solid #333;")
                              .arg((color >> 16) & 0xFF)
                              .arg((color >> 8) & 0xFF)
                              .arg(color & 0xFF));

    connect(color_button, &QPushButton::clicked, this, &SpatialEffect3D::OnColorButtonClicked);

    color_buttons.push_back(color_button);

    // Insert before the add/remove buttons
    int insert_pos = color_buttons_layout->count() - 3; // Before +, -, and stretch
    if(insert_pos < 0) insert_pos = 0;
    color_buttons_layout->insertWidget(insert_pos, color_button);
}

void SpatialEffect3D::RemoveLastColorButton()
{
    if(!color_buttons.empty())
    {
        QPushButton* last_button = color_buttons.back();
        color_buttons_layout->removeWidget(last_button);
        color_buttons.pop_back();
        last_button->deleteLater();
    }
}

RGBColor SpatialEffect3D::GetRainbowColor(float hue)
{
    // Convert HSV to RGB (Hue: 0-360, Saturation: 1.0, Value: 1.0)
    hue = std::fmod(hue, 360.0f);
    if(hue < 0) hue += 360.0f;

    float c = 1.0f; // Chroma (since saturation = 1, value = 1)
    float x = c * (1.0f - std::fabs(std::fmod(hue / 60.0f, 2.0f) - 1.0f));

    float r, g, b;
    if(hue < 60) { r = c; g = x; b = 0; }
    else if(hue < 120) { r = x; g = c; b = 0; }
    else if(hue < 180) { r = 0; g = c; b = x; }
    else if(hue < 240) { r = 0; g = x; b = c; }
    else if(hue < 300) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }

    // OpenRGB uses BGR format: 0x00BBGGRR
    return ((int)(b * 255) << 16) | ((int)(g * 255) << 8) | (int)(r * 255);
}

RGBColor SpatialEffect3D::GetColorAtPosition(float position)
{
    if(rainbow_mode)
    {
        return GetRainbowColor(position * 360.0f);
    }

    if(colors.empty())
    {
        return COLOR_WHITE;
    }

    if(colors.size() == 1)
    {
        return colors[0];
    }

    // Interpolate between colors
    float scaled_pos = position * (colors.size() - 1);
    int index = (int)scaled_pos;
    float frac = scaled_pos - index;

    if(index >= (int)colors.size() - 1)
    {
        return colors.back();
    }

    RGBColor color1 = colors[index];
    RGBColor color2 = colors[index + 1];

    // Linear interpolation - OpenRGB uses BGR format: 0x00BBGGRR
    int b1 = (color1 >> 16) & 0xFF;
    int g1 = (color1 >> 8) & 0xFF;
    int r1 = color1 & 0xFF;

    int b2 = (color2 >> 16) & 0xFF;
    int g2 = (color2 >> 8) & 0xFF;
    int r2 = color2 & 0xFF;

    int r = (int)(r1 + (r2 - r1) * frac);
    int g = (int)(g1 + (g2 - g1) * frac);
    int b = (int)(b1 + (b2 - b1) * frac);

    // Return in BGR format
    return (b << 16) | (g << 8) | r;
}

/*---------------------------------------------------------*\
| New Color Control Slots                                  |
\*---------------------------------------------------------*/
void SpatialEffect3D::OnRainbowModeChanged()
{
    rainbow_mode = rainbow_mode_check->isChecked();
    color_buttons_widget->setVisible(!rainbow_mode);
    emit ParametersChanged();
}

void SpatialEffect3D::OnAddColorClicked()
{
    // Add a new random color
    RGBColor new_color = GetRainbowColor(colors.size() * 60.0f); // Space colors around hue wheel
    colors.push_back(new_color);
    CreateColorButton(new_color);

    remove_color_button->setEnabled(colors.size() > 1);
    emit ParametersChanged();
}

void SpatialEffect3D::OnRemoveColorClicked()
{
    if(colors.size() > 1)
    {
        colors.pop_back();
        RemoveLastColorButton();
        remove_color_button->setEnabled(colors.size() > 1);
        emit ParametersChanged();
    }
}

void SpatialEffect3D::OnColorButtonClicked()
{
    QPushButton* clicked_button = qobject_cast<QPushButton*>(sender());
    if(!clicked_button) return;

    // Find which color button was clicked
    std::vector<QPushButton*>::iterator it = std::find(color_buttons.begin(), color_buttons.end(), clicked_button);
    if(it == color_buttons.end()) return;

    int index = std::distance(color_buttons.begin(), it);
    if(index >= (int)colors.size()) return;

    // Open color dialog
    QColorDialog color_dialog;
    QColor current_color = QColor((colors[index] >> 16) & 0xFF,
                                 (colors[index] >> 8) & 0xFF,
                                 colors[index] & 0xFF);
    color_dialog.setCurrentColor(current_color);

    if(color_dialog.exec() == QDialog::Accepted)
    {
        QColor new_color = color_dialog.currentColor();
        colors[index] = (new_color.red() << 16) | (new_color.green() << 8) | new_color.blue();

        // Update button color
        clicked_button->setStyleSheet(QString("background-color: rgb(%1, %2, %3); border: 1px solid #333;")
                                    .arg(new_color.red())
                                    .arg(new_color.green())
                                    .arg(new_color.blue()));

        emit ParametersChanged();
    }
}

void SpatialEffect3D::OnStartEffectClicked()
{
    effect_running = true;
    start_effect_button->setEnabled(false);
    stop_effect_button->setEnabled(true);
    emit ParametersChanged();
}

void SpatialEffect3D::OnStopEffectClicked()
{
    effect_running = false;
    start_effect_button->setEnabled(true);
    stop_effect_button->setEnabled(false);
    emit ParametersChanged();
}

/*---------------------------------------------------------*\
| New getter/setter methods                                |
\*---------------------------------------------------------*/
void SpatialEffect3D::SetColors(const std::vector<RGBColor>& new_colors)
{
    colors = new_colors;
    if(colors.empty())
    {
        colors.push_back(COLOR_RED);
    }
}

std::vector<RGBColor> SpatialEffect3D::GetColors() const
{
    return colors;
}

void SpatialEffect3D::SetRainbowMode(bool enabled)
{
    rainbow_mode = enabled;
    if(rainbow_mode_check)
    {
        rainbow_mode_check->setChecked(enabled);
    }
}

bool SpatialEffect3D::GetRainbowMode() const
{
    return rainbow_mode;
}

void SpatialEffect3D::SetFrequency(unsigned int frequency)
{
    effect_frequency = frequency;
    if(frequency_slider)
    {
        frequency_slider->setValue(frequency);
    }
}

unsigned int SpatialEffect3D::GetFrequency() const
{
    return effect_frequency;
}

/*---------------------------------------------------------*\
| Reference Point Methods                                  |
\*---------------------------------------------------------*/
void SpatialEffect3D::SetReferenceMode(ReferenceMode mode)
{
    reference_mode = mode;
}

ReferenceMode SpatialEffect3D::GetReferenceMode() const
{
    return reference_mode;
}

void SpatialEffect3D::SetGlobalReferencePoint(const Vector3D& point)
{
    global_reference_point = point;
}

Vector3D SpatialEffect3D::GetGlobalReferencePoint() const
{
    return global_reference_point;
}

void SpatialEffect3D::SetCustomReferencePoint(const Vector3D& point)
{
    custom_reference_point = point;
}

void SpatialEffect3D::SetUseCustomReference(bool use_custom)
{
    use_custom_reference = use_custom;
}

Vector3D SpatialEffect3D::GetEffectOrigin() const
{
    if(use_custom_reference)
    {
        return custom_reference_point;
    }

    switch(reference_mode)
    {
        case REF_MODE_USER_POSITION:
            return global_reference_point;
        case REF_MODE_CUSTOM_POINT:
            return custom_reference_point;
        case REF_MODE_ROOM_CENTER:
        default:
            // Legacy behavior: returns corner origin (0,0,0)
            // This is kept for backward compatibility with old effects
            // New effects should use GetEffectOriginGrid() instead
            return {0.0f, 0.0f, 0.0f};
    }
}

Vector3D SpatialEffect3D::GetEffectOriginGrid(const GridContext3D& grid) const
{
    if(use_custom_reference)
    {
        return custom_reference_point;
    }

    switch(reference_mode)
    {
        case REF_MODE_USER_POSITION:
            return global_reference_point;
        case REF_MODE_CUSTOM_POINT:
            return custom_reference_point;
        case REF_MODE_ROOM_CENTER:
        default:
            // Return actual room center from grid context
            return {grid.center_x, grid.center_y, grid.center_z};
    }
}

/*---------------------------------------------------------*\
| Standardized Parameter Calculation Helpers               |
| These provide consistent behavior across all effects     |
\*---------------------------------------------------------*/
/*---------------------------------------------------------*\
| Base normalized parameter getters                        |
| These return 0.0-1.0 or similar ranges, no effect bias   |
\*---------------------------------------------------------*/
float SpatialEffect3D::GetNormalizedSpeed() const
{
    // Apply quadratic curve for smooth acceleration
    // Range: 0.0 (stopped) to 1.0 (max speed)
    // This curve works well for ANY effect type
    float normalized = effect_speed / 100.0f;
    return normalized * normalized;
}

float SpatialEffect3D::GetNormalizedFrequency() const
{
    // Apply quadratic curve for smooth frequency scaling
    // Range: 0.0 (low frequency) to 1.0 (high frequency)
    float normalized = effect_frequency / 100.0f;
    return normalized * normalized;
}

float SpatialEffect3D::GetNormalizedSize() const
{
    // Linear scaling for size (pattern density)
    // Range: 0.1 (tiny) to 1.0 (normal) to 2.0 (double size)
    // Works for spatial effects, particle effects, wave effects, etc.
    return 0.1f + (effect_size / 100.0f) * 1.9f;
}

float SpatialEffect3D::GetNormalizedScale() const
{
    // New scale mapping: 0-250 slider range
    // 0-200: Maps to 0-100% of room (each 2 units = 1%)
    // 201-250: Maps to 101-150% beyond room (each unit = 1%)
    if(effect_scale <= 200)
    {
        // 0-200 range: 0% to 100% of room
        return effect_scale / 200.0f;  // 0.0 to 1.0
    }
    else
    {
        // 201-250 range: 101% to 150% beyond room
        return 1.0f + ((effect_scale - 200) / 100.0f);  // 1.01 to 1.5
    }
}

unsigned int SpatialEffect3D::GetTargetFPS() const
{
    return effect_fps;
}

/*---------------------------------------------------------*\
| Scaled parameter getters - use effect-specific info      |
\*---------------------------------------------------------*/
float SpatialEffect3D::GetScaledSpeed() const
{
    EffectInfo3D info = const_cast<SpatialEffect3D*>(this)->GetEffectInfo();
    float speed_scale = (info.default_speed_scale > 0.0f) ? info.default_speed_scale : 10.0f;
    return GetNormalizedSpeed() * speed_scale;
}

float SpatialEffect3D::GetScaledFrequency() const
{
    EffectInfo3D info = const_cast<SpatialEffect3D*>(this)->GetEffectInfo();
    float freq_scale = (info.default_frequency_scale > 0.0f) ? info.default_frequency_scale : 10.0f;
    return GetNormalizedFrequency() * freq_scale;
}

/*---------------------------------------------------------*\
| Universal progress calculator                            |
| Use this in CalculateColor() for time-based animation    |
\*---------------------------------------------------------*/
float SpatialEffect3D::CalculateProgress(float time) const
{
    float progress = time * GetScaledSpeed();
    return effect_reverse ? -progress : progress;
}

/*---------------------------------------------------------*\
| Boundary checking helper - LEGACY VERSION                |
| Returns false if LED is outside the effect radius        |
| Uses fixed radius - kept for backward compatibility      |
\*---------------------------------------------------------*/
bool SpatialEffect3D::IsWithinEffectBoundary(float rel_x, float rel_y, float rel_z) const
{
    // If grid-aware boundary has already been validated upstream, skip legacy check
    if(boundary_prevalidated)
    {
        return true;
    }

    // Legacy fixed radius calculation
    // This assumes a "standard" room size for effects that don't have grid context
    // Scale slider: 10 (10%) = 1mm radius, 100 (100%) = 10mm, 200 (200%) = 20mm
    float scale_radius = GetNormalizedScale() * 10.0f;
    float distance_from_origin = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
    return distance_from_origin <= scale_radius;
}

/*---------------------------------------------------------*\
| Boundary checking helper - ROOM-AWARE VERSION            |
| Returns false if LED is outside the effect radius        |
| Scale is relative to room size for universal compatibility|
| RECOMMENDED: Use this version in CalculateColorGrid()    |
\*---------------------------------------------------------*/
bool SpatialEffect3D::IsWithinEffectBoundary(float rel_x, float rel_y, float rel_z, const GridContext3D& grid) const
{
    // Calculate room's half-diagonal (center to corner distance)
    // This is the maximum distance from room center to any corner
    float half_width = grid.width / 2.0f;
    float half_depth = grid.depth / 2.0f;
    float half_height = grid.height / 2.0f;
    float max_distance_from_center = sqrt(half_width * half_width +
                                         half_depth * half_depth +
                                         half_height * half_height);

    // New scale mapping: 0-250 slider range
    // 0-200: Maps to 0-100% of room (each 2 units = 1%)
    // 201-250: Maps to 101-150% beyond room (each unit = 1%)
    float scale_percentage;
    if(effect_scale <= 200)
    {
        // 0-200 range: 0% to 100% of room
        scale_percentage = effect_scale / 200.0f;  // 0.0 to 1.0
    }
    else
    {
        // 201-250 range: 101% to 150% beyond room
        scale_percentage = 1.0f + ((effect_scale - 200) / 100.0f);  // 1.01 to 1.5
    }

    float scale_radius = max_distance_from_center * scale_percentage;

    float distance_from_origin = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
    return distance_from_origin <= scale_radius;
}

/*---------------------------------------------------------*\
| Parameter Update Methods                                 |
\*---------------------------------------------------------*/
void SpatialEffect3D::UpdateCommonEffectParams(SpatialEffectParams& /* params */)
{
    // Empty implementation - old 3D controls removed
}

/*---------------------------------------------------------*\
| Helper function to set visibility of a control group     |
\*---------------------------------------------------------*/
void SpatialEffect3D::SetControlGroupVisibility(QSlider* slider, QLabel* value_label, const QString& label_text, bool visible)
{
    if(slider && value_label)
    {
        slider->setVisible(visible);
        value_label->setVisible(visible);

        QWidget* parent = slider->parentWidget();
        if(parent)
        {
            QList<QLabel*> labels = parent->findChildren<QLabel*>();
            for(int i = 0; i < labels.size(); i++)
            {
                QLabel* label = labels[i];
                if(label->text() == label_text)
                {
                    label->setVisible(visible);
                    break;
                }
            }
        }
    }
}

/*---------------------------------------------------------*\
| Apply Control Visibility                                 |
| Hides base controls if effect provides custom versions   |
\*---------------------------------------------------------*/
void SpatialEffect3D::ApplyControlVisibility()
{
    EffectInfo3D info = GetEffectInfo();

    // Check version to determine if effect uses new visibility system
    // Version 2+ effects explicitly set visibility flags
    // Version 0/1 (old) effects default to showing all controls
    bool is_versioned_effect = (info.info_version >= 2);

    bool show_speed = is_versioned_effect ? info.show_speed_control : true;
    bool show_brightness = is_versioned_effect ? info.show_brightness_control : true;
    bool show_frequency = is_versioned_effect ? info.show_frequency_control : true;
    bool show_size = is_versioned_effect ? info.show_size_control : true;
    bool show_scale = is_versioned_effect ? info.show_scale_control : true;
    bool show_fps = is_versioned_effect ? info.show_fps_control : true;
    bool show_axis = is_versioned_effect ? info.show_axis_control : true;
    bool show_colors = is_versioned_effect ? info.show_color_controls : true;

    // Hide/show controls based on effect's declarations
    SetControlGroupVisibility(speed_slider, speed_label, "Speed:", show_speed);
    SetControlGroupVisibility(brightness_slider, brightness_label, "Brightness:", show_brightness);
    SetControlGroupVisibility(frequency_slider, frequency_label, "Frequency:", show_frequency);
    SetControlGroupVisibility(size_slider, size_label, "Size:", show_size);
    SetControlGroupVisibility(scale_slider, scale_label, "Scale:", show_scale);
    SetControlGroupVisibility(fps_slider, fps_label, "FPS:", show_fps);

    // Handle axis control separately as it uses combo box instead of slider
    if(axis_combo && reverse_check)
    {
        axis_combo->setVisible(show_axis);
        reverse_check->setVisible(show_axis);
        QWidget* parent = axis_combo->parentWidget();
        if(parent)
        {
            QList<QLabel*> labels = parent->findChildren<QLabel*>();
            for(int i = 0; i < labels.size(); i++)
            {
                QLabel* label = labels[i];
                if(label->text() == "Axis:")
                {
                    label->setVisible(show_axis);
                    break;
                }
            }
        }
    }

    if(color_controls_group)
    {
        color_controls_group->setVisible(show_colors);
    }
}

void SpatialEffect3D::OnParameterChanged()
{
    // Use linear speed mapping - effects handle their own speed curves
    if(speed_slider)
    {
        effect_speed = speed_slider->value();
        if(speed_label)
        {
            speed_label->setText(QString::number(effect_speed));
        }
    }

    if(brightness_slider && brightness_label)
    {
        effect_brightness = brightness_slider->value();
        brightness_label->setText(QString::number(effect_brightness));
    }

    if(frequency_slider && frequency_label)
    {
        effect_frequency = frequency_slider->value();
        frequency_label->setText(QString::number(effect_frequency));
    }

    if(size_slider && size_label)
    {
        effect_size = size_slider->value();
        size_label->setText(QString::number(effect_size));
    }

    if(scale_slider && scale_label)
    {
        effect_scale = scale_slider->value();
        scale_label->setText(QString::number(effect_scale));
    }

    if(fps_slider && fps_label)
    {
        effect_fps = fps_slider->value();
        fps_label->setText(QString::number(effect_fps));
    }

    emit ParametersChanged();
}

/*---------------------------------------------------------*\
| Axis Control Slots                                       |
\*---------------------------------------------------------*/
void SpatialEffect3D::OnAxisChanged()
{
    if(axis_combo)
    {
        int idx = axis_combo->currentIndex();
        if(idx == 0)
        {
            axis_none = true;
        }
        else
        {
            axis_none = false;
            effect_axis = (EffectAxis)(idx - 1);
        }
    }
    emit ParametersChanged();
}

void SpatialEffect3D::OnReverseChanged()
{
    if(reverse_check)
    {
        effect_reverse = reverse_check->isChecked();
    }
    emit ParametersChanged();
}

/*---------------------------------------------------------*\
| Serialization - save effect parameters to JSON           |
\*---------------------------------------------------------*/
nlohmann::json SpatialEffect3D::SaveSettings() const
{
    nlohmann::json j;

    // Save common parameters
    j["speed"] = effect_speed;
    j["brightness"] = effect_brightness;
    j["frequency"] = effect_frequency;
    j["rainbow_mode"] = rainbow_mode;
    j["reverse"] = effect_reverse;

    // Save colors
    nlohmann::json colors_array = nlohmann::json::array();
    for(size_t i = 0; i < colors.size(); i++)
    {
        RGBColor color = colors[i];
        colors_array.push_back({
            {"r", RGBGetRValue(color)},
            {"g", RGBGetGValue(color)},
            {"b", RGBGetBValue(color)}
        });
    }
    j["colors"] = colors_array;

    // Save reference point settings
    j["reference_mode"] = (int)reference_mode;
    j["global_ref_x"] = global_reference_point.x;
    j["global_ref_y"] = global_reference_point.y;
    j["global_ref_z"] = global_reference_point.z;
    j["custom_ref_x"] = custom_reference_point.x;
    j["custom_ref_y"] = custom_reference_point.y;
    j["custom_ref_z"] = custom_reference_point.z;
    j["use_custom_ref"] = use_custom_reference;

    return j;
}

/*---------------------------------------------------------*\
| Serialization - load effect parameters from JSON         |
\*---------------------------------------------------------*/
void SpatialEffect3D::LoadSettings(const nlohmann::json& settings)
{
    // Load common parameters
    if(settings.contains("speed"))
        SetSpeed(settings["speed"].get<unsigned int>());

    if(settings.contains("brightness"))
        SetBrightness(settings["brightness"].get<unsigned int>());

    if(settings.contains("frequency"))
        SetFrequency(settings["frequency"].get<unsigned int>());

    if(settings.contains("rainbow_mode"))
        SetRainbowMode(settings["rainbow_mode"].get<bool>());

    if(settings.contains("reverse"))
        effect_reverse = settings["reverse"].get<bool>();

    // Load colors
    if(settings.contains("colors"))
    {
        std::vector<RGBColor> loaded_colors;
        const nlohmann::json& colors_array = settings["colors"];
        for(size_t i = 0; i < colors_array.size(); i++)
        {
            const nlohmann::json& color_json = colors_array[i];
            unsigned char r = color_json["r"].get<unsigned char>();
            unsigned char g = color_json["g"].get<unsigned char>();
            unsigned char b = color_json["b"].get<unsigned char>();
            loaded_colors.push_back(ToRGBColor(r, g, b));
        }
        SetColors(loaded_colors);
    }

    // Load reference point settings
    if(settings.contains("reference_mode"))
        SetReferenceMode((ReferenceMode)settings["reference_mode"].get<int>());

    if(settings.contains("global_ref_x") && settings.contains("global_ref_y") && settings.contains("global_ref_z"))
    {
        Vector3D ref_point;
        ref_point.x = settings["global_ref_x"].get<float>();
        ref_point.y = settings["global_ref_y"].get<float>();
        ref_point.z = settings["global_ref_z"].get<float>();
        SetGlobalReferencePoint(ref_point);
    }

    if(settings.contains("custom_ref_x") && settings.contains("custom_ref_y") && settings.contains("custom_ref_z"))
    {
        Vector3D ref_point;
        ref_point.x = settings["custom_ref_x"].get<float>();
        ref_point.y = settings["custom_ref_y"].get<float>();
        ref_point.z = settings["custom_ref_z"].get<float>();
        SetCustomReferencePoint(ref_point);
    }

    if(settings.contains("use_custom_ref"))
        SetUseCustomReference(settings["use_custom_ref"].get<bool>());

    // Update UI controls if they exist
    if(speed_slider)
        speed_slider->setValue(effect_speed);
    if(brightness_slider)
        brightness_slider->setValue(effect_brightness);
    if(frequency_slider)
        frequency_slider->setValue(effect_frequency);
    if(rainbow_mode_check)
        rainbow_mode_check->setChecked(rainbow_mode);
    if(reverse_check)
        reverse_check->setChecked(effect_reverse);
}
