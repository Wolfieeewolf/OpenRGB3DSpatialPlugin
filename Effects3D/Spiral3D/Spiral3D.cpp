/*---------------------------------------------------------*\
| Spiral3D.cpp                                              |
|                                                           |
|   3D Spiral effect with arm count control               |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "Spiral3D.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QColorDialog>
#include <algorithm>
#include <cmath>

Spiral3D::Spiral3D(QWidget* parent) : SpatialEffect3D(parent)
{
    arms_slider = nullptr;
    speed_slider = nullptr;
    brightness_slider = nullptr;
    frequency_slider = nullptr;
    rainbow_mode_check = nullptr;

    color_controls_widget = nullptr;
    color_controls_layout = nullptr;
    add_color_button = nullptr;
    remove_color_button = nullptr;

    num_arms = 3;            // Default arms
    frequency = 50;          // Default frequency
    rainbow_mode = true;     // Default to rainbow mode
    progress = 0.0f;

    // Initialize with default colors
    colors.push_back(0x000000FF);  // Blue
    colors.push_back(0x0000FF00);  // Green
    colors.push_back(0x00FF0000);  // Red
}

Spiral3D::~Spiral3D()
{
}

EffectInfo3D Spiral3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.effect_name = "3D Spiral";
    info.effect_description = "Animated spiral pattern with configurable arm count";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_SPIRAL;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = true;
    info.needs_frequency = false;
    return info;
}

void Spiral3D::SetupCustomUI(QWidget* parent)
{
    QWidget* spiral_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(spiral_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    /*---------------------------------------------------------*\
    | Row 0: Arms                                              |
    \*---------------------------------------------------------*/
    layout->addWidget(new QLabel("Arms:"), 0, 0);
    arms_slider = new QSlider(Qt::Horizontal);
    arms_slider->setRange(2, 8);
    arms_slider->setValue(num_arms);
    layout->addWidget(arms_slider, 0, 1);

    /*---------------------------------------------------------*\
    | Row 1: Speed                                             |
    \*---------------------------------------------------------*/
    layout->addWidget(new QLabel("Speed:"), 1, 0);
    speed_slider = new QSlider(Qt::Horizontal);
    speed_slider->setRange(1, 200);
    speed_slider->setValue(effect_speed);
    layout->addWidget(speed_slider, 1, 1);

    /*---------------------------------------------------------*\
    | Row 2: Brightness                                        |
    \*---------------------------------------------------------*/
    layout->addWidget(new QLabel("Brightness:"), 2, 0);
    brightness_slider = new QSlider(Qt::Horizontal);
    brightness_slider->setRange(1, 100);
    brightness_slider->setValue(effect_brightness);
    layout->addWidget(brightness_slider, 2, 1);

    /*---------------------------------------------------------*\
    | Row 3: Frequency                                         |
    \*---------------------------------------------------------*/
    layout->addWidget(new QLabel("Frequency:"), 3, 0);
    frequency_slider = new QSlider(Qt::Horizontal);
    frequency_slider->setRange(1, 100);
    frequency_slider->setValue(frequency);
    layout->addWidget(frequency_slider, 3, 1);

    /*---------------------------------------------------------*\
    | Row 4: Rainbow Mode                                      |
    \*---------------------------------------------------------*/
    rainbow_mode_check = new QCheckBox("Rainbow Mode");
    rainbow_mode_check->setChecked(rainbow_mode);
    layout->addWidget(rainbow_mode_check, 4, 0, 1, 2);

    /*---------------------------------------------------------*\
    | Add to parent layout                                     |
    \*---------------------------------------------------------*/
    if(parent && parent->layout())
    {
        parent->layout()->addWidget(spiral_widget);
    }

    SetupColorControls(parent);

    /*---------------------------------------------------------*\
    | Connect signals                                          |
    \*---------------------------------------------------------*/
    connect(arms_slider, &QSlider::valueChanged, this, &Spiral3D::OnSpiralParameterChanged);
    connect(speed_slider, &QSlider::valueChanged, this, &Spiral3D::OnSpiralParameterChanged);
    connect(brightness_slider, &QSlider::valueChanged, this, &Spiral3D::OnSpiralParameterChanged);
    connect(frequency_slider, &QSlider::valueChanged, this, &Spiral3D::OnSpiralParameterChanged);
    connect(rainbow_mode_check, &QCheckBox::toggled, this, &Spiral3D::OnRainbowModeChanged);
}

void Spiral3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_SPIRAL;
}

void Spiral3D::OnSpiralParameterChanged()
{
    /*---------------------------------------------------------*\
    | Update internal parameters                               |
    \*---------------------------------------------------------*/
    if(arms_slider) num_arms = arms_slider->value();
    if(speed_slider) effect_speed = speed_slider->value();
    if(brightness_slider) effect_brightness = brightness_slider->value();
    if(frequency_slider) frequency = frequency_slider->value();

    /*---------------------------------------------------------*\
    | Emit parameter change signal                             |
    \*---------------------------------------------------------*/
    emit ParametersChanged();
}

void Spiral3D::OnRainbowModeChanged()
{
    if(rainbow_mode_check)
    {
        rainbow_mode = rainbow_mode_check->isChecked();
    }

    if(color_controls_widget)
    {
        color_controls_widget->setVisible(!rainbow_mode);
    }

    emit ParametersChanged();
}

void Spiral3D::OnAddColorClicked()
{
    RGBColor new_color = 0xFFFFFFFF; // White
    colors.push_back(new_color);
    CreateColorButton(new_color);
    emit ParametersChanged();
}

void Spiral3D::OnRemoveColorClicked()
{
    if(colors.size() > 1)
    {
        colors.pop_back();
        RemoveLastColorButton();
        emit ParametersChanged();
    }
}

void Spiral3D::OnColorButtonClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if(!button) return;

    int index = -1;
    for(unsigned int i = 0; i < color_buttons.size(); i++)
    {
        if(color_buttons[i] == button)
        {
            index = i;
            break;
        }
    }

    if(index >= 0 && index < (int)colors.size())
    {
        QColor initial_color;
        initial_color.setRgb(colors[index] & 0xFF, (colors[index] >> 8) & 0xFF, (colors[index] >> 16) & 0xFF);

        QColor new_color = QColorDialog::getColor(initial_color, this);
        if(new_color.isValid())
        {
            colors[index] = (new_color.blue() << 16) | (new_color.green() << 8) | new_color.red();
            QString style = QString("background-color: rgb(%1, %2, %3);").arg(new_color.red()).arg(new_color.green()).arg(new_color.blue());
            button->setStyleSheet(style);
            emit ParametersChanged();
        }
    }
}

RGBColor Spiral3D::CalculateColor(float x, float y, float z, float time)
{
    /*---------------------------------------------------------*\
    | Create smooth curves for speed and frequency            |
    \*---------------------------------------------------------*/
    float speed_curve = (effect_speed / 100.0f);
    speed_curve = speed_curve * speed_curve; // Quadratic curve for smoother control
    float actual_speed = speed_curve * 200.0f; // Map back to 0-200 range

    float freq_curve = (frequency / 100.0f);
    freq_curve = freq_curve * freq_curve; // Quadratic curve for smoother control
    float actual_frequency = freq_curve * 100.0f; // Map to 0-100 range

    /*---------------------------------------------------------*\
    | Update progress for animation                            |
    \*---------------------------------------------------------*/
    progress = time * (actual_speed * 0.1f);
    float freq_scale = actual_frequency * 0.01f;

    /*---------------------------------------------------------*\
    | Calculate enhanced 3D spiral pattern                    |
    \*---------------------------------------------------------*/
    // Primary spiral in XY plane
    float radius = sqrt(x*x + y*y);
    float angle = atan2(y, x);

    // Add Z-axis twist for true 3D spiral
    float z_twist = z * 0.3f;
    float spiral_angle = angle * num_arms + radius * freq_scale + z_twist - progress;

    // Create multiple spiral layers for depth
    float spiral_value = sin(spiral_angle) * (1.0f + 0.4f * cos(z * freq_scale + progress * 0.7f));

    // Add secondary spiral for complexity
    float secondary_spiral = cos(spiral_angle * 0.5f + z * freq_scale * 1.5f + progress * 1.2f) * 0.3f;
    spiral_value += secondary_spiral;

    /*---------------------------------------------------------*\
    | Normalize to 0.0 - 1.0                                  |
    \*---------------------------------------------------------*/
    spiral_value = (spiral_value + 1.5f) / 3.0f;
    spiral_value = fmax(0.0f, fmin(1.0f, spiral_value));

    /*---------------------------------------------------------*\
    | Get color based on mode                                  |
    \*---------------------------------------------------------*/
    RGBColor final_color;

    if(rainbow_mode)
    {
        float hue = spiral_value * 360.0f + progress * 20.0f;
        final_color = GetRainbowColor(hue);
    }
    else
    {
        final_color = GetColorAtPosition(spiral_value);
    }

    /*---------------------------------------------------------*\
    | Apply brightness                                         |
    \*---------------------------------------------------------*/
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;

    float brightness_factor = effect_brightness / 100.0f;
    r = (unsigned char)(r * brightness_factor);
    g = (unsigned char)(g * brightness_factor);
    b = (unsigned char)(b * brightness_factor);

    return (b << 16) | (g << 8) | r;
}

RGBColor Spiral3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    /*---------------------------------------------------------*\
    | Calculate normalized coordinates (0 to 1)               |
    \*---------------------------------------------------------*/
    float norm_x = (x - grid.min_x) / grid.width;
    float norm_y = (y - grid.min_y) / grid.height;
    float norm_z = (z - grid.min_z) / grid.depth;

    /*---------------------------------------------------------*\
    | Create smooth curves for speed and frequency            |
    \*---------------------------------------------------------*/
    float speed_curve = (effect_speed / 100.0f);
    speed_curve = speed_curve * speed_curve; // Quadratic curve for smoother control
    float actual_speed = speed_curve * 200.0f; // Map back to 0-200 range

    float freq_curve = (frequency / 100.0f);
    freq_curve = freq_curve * freq_curve; // Quadratic curve for smoother control
    float actual_frequency = freq_curve * 100.0f; // Map to 0-100 range

    /*---------------------------------------------------------*\
    | Update progress for animation                            |
    \*---------------------------------------------------------*/
    progress = time * (actual_speed * 0.1f);

    /*---------------------------------------------------------*\
    | Scale frequency based on grid dimensions                 |
    \*---------------------------------------------------------*/
    float freq_scale = actual_frequency * 0.01f;
    float grid_scale = sqrt(grid.width * grid.height) / 10.0f; // Use XY plane area for spiral scaling
    freq_scale *= grid_scale;

    /*---------------------------------------------------------*\
    | Convert to centered coordinates for spiral calculation  |
    \*---------------------------------------------------------*/
    float center_x = (norm_x - 0.5f) * grid.width;
    float center_y = (norm_y - 0.5f) * grid.height;
    float center_z = (norm_z - 0.5f) * grid.depth;

    /*---------------------------------------------------------*\
    | Calculate enhanced 3D spiral pattern (grid-aware)       |
    \*---------------------------------------------------------*/
    // Primary spiral in XY plane
    float radius = sqrt(center_x*center_x + center_y*center_y);
    float angle = atan2(center_y, center_x);

    // Scale radius to grid dimensions
    float max_radius = sqrt(grid.width*grid.width + grid.height*grid.height) * 0.5f;
    float normalized_radius = radius / max_radius;

    // Add Z-axis twist for true 3D spiral
    float z_twist = center_z * 0.3f;
    float spiral_angle = angle * num_arms + normalized_radius * freq_scale + z_twist - progress;

    // Create multiple spiral layers for depth
    float spiral_value = sin(spiral_angle) * (1.0f + 0.4f * cos(center_z * freq_scale + progress * 0.7f));

    // Add secondary spiral for complexity
    float secondary_spiral = cos(spiral_angle * 0.5f + center_z * freq_scale * 1.5f + progress * 1.2f) * 0.3f;
    spiral_value += secondary_spiral;

    /*---------------------------------------------------------*\
    | Normalize to 0.0 - 1.0                                  |
    \*---------------------------------------------------------*/
    spiral_value = (spiral_value + 1.5f) / 3.0f;
    spiral_value = fmax(0.0f, fmin(1.0f, spiral_value));

    /*---------------------------------------------------------*\
    | Get color based on mode                                  |
    \*---------------------------------------------------------*/
    RGBColor final_color;

    if(rainbow_mode)
    {
        float hue = spiral_value * 360.0f + progress * 20.0f;
        final_color = GetRainbowColor(hue);
    }
    else
    {
        final_color = GetColorAtPosition(spiral_value);
    }

    /*---------------------------------------------------------*\
    | Apply brightness                                         |
    \*---------------------------------------------------------*/
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;

    float brightness_factor = effect_brightness / 100.0f;
    r = (unsigned char)(r * brightness_factor);
    g = (unsigned char)(g * brightness_factor);
    b = (unsigned char)(b * brightness_factor);

    return (b << 16) | (g << 8) | r;
}

void Spiral3D::SetupColorControls(QWidget* parent)
{
    if(!parent || !parent->layout()) return;

    color_controls_widget = new QWidget();
    color_controls_layout = new QHBoxLayout(color_controls_widget);
    color_controls_layout->setContentsMargins(0, 0, 0, 0);

    add_color_button = new QPushButton("+");
    add_color_button->setMaximumWidth(30);
    remove_color_button = new QPushButton("-");
    remove_color_button->setMaximumWidth(30);

    color_controls_layout->addWidget(new QLabel("Colors:"));
    color_controls_layout->addWidget(add_color_button);
    color_controls_layout->addWidget(remove_color_button);

    for(unsigned int i = 0; i < colors.size(); i++)
    {
        CreateColorButton(colors[i]);
    }

    color_controls_layout->addStretch();
    parent->layout()->addWidget(color_controls_widget);
    color_controls_widget->setVisible(!rainbow_mode);

    connect(add_color_button, &QPushButton::clicked, this, &Spiral3D::OnAddColorClicked);
    connect(remove_color_button, &QPushButton::clicked, this, &Spiral3D::OnRemoveColorClicked);
}

void Spiral3D::CreateColorButton(RGBColor color)
{
    QPushButton* button = new QPushButton();
    button->setMaximumWidth(30);
    button->setMaximumHeight(30);

    unsigned char r = color & 0xFF;
    unsigned char g = (color >> 8) & 0xFF;
    unsigned char b = (color >> 16) & 0xFF;
    QString style = QString("background-color: rgb(%1, %2, %3);").arg(r).arg(g).arg(b);
    button->setStyleSheet(style);

    color_buttons.push_back(button);
    color_controls_layout->insertWidget(color_controls_layout->count() - 1, button);
    connect(button, &QPushButton::clicked, this, &Spiral3D::OnColorButtonClicked);
}

void Spiral3D::RemoveLastColorButton()
{
    if(!color_buttons.empty())
    {
        QPushButton* button = color_buttons.back();
        color_buttons.pop_back();
        color_controls_layout->removeWidget(button);
        delete button;
    }
}

RGBColor Spiral3D::GetRainbowColor(float hue)
{
    float h = fmod(hue, 360.0f);
    if(h < 0.0f) h += 360.0f;

    float saturation = 1.0f;
    float value = 1.0f;

    float c = value * saturation;
    float x = c * (1.0f - fabs(fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = value - c;

    float r, g, b;
    if(h >= 0 && h < 60) {
        r = c; g = x; b = 0;
    } else if(h >= 60 && h < 120) {
        r = x; g = c; b = 0;
    } else if(h >= 120 && h < 180) {
        r = 0; g = c; b = x;
    } else if(h >= 180 && h < 240) {
        r = 0; g = x; b = c;
    } else if(h >= 240 && h < 300) {
        r = x; g = 0; b = c;
    } else {
        r = c; g = 0; b = x;
    }

    unsigned char red = (unsigned char)((r + m) * 255);
    unsigned char green = (unsigned char)((g + m) * 255);
    unsigned char blue = (unsigned char)((b + m) * 255);

    return (blue << 16) | (green << 8) | red;
}

RGBColor Spiral3D::GetColorAtPosition(float position)
{
    if(colors.empty()) return 0x000000;
    if(colors.size() == 1) return colors[0];

    position = fmod(position, 1.0f);
    if(position < 0.0f) position += 1.0f;

    float color_pos = position * (colors.size() - 1);
    int color_idx = (int)color_pos;
    float blend = color_pos - color_idx;

    if(color_idx >= (int)colors.size() - 1)
    {
        return colors[colors.size() - 1];
    }

    RGBColor color1 = colors[color_idx];
    RGBColor color2 = colors[color_idx + 1];

    unsigned char r1 = color1 & 0xFF;
    unsigned char g1 = (color1 >> 8) & 0xFF;
    unsigned char b1 = (color1 >> 16) & 0xFF;

    unsigned char r2 = color2 & 0xFF;
    unsigned char g2 = (color2 >> 8) & 0xFF;
    unsigned char b2 = (color2 >> 16) & 0xFF;

    unsigned char r = (unsigned char)(r1 + (r2 - r1) * blend);
    unsigned char g = (unsigned char)(g1 + (g2 - g1) * blend);
    unsigned char b = (unsigned char)(b1 + (b2 - b1) * blend);

    return (b << 16) | (g << 8) | r;
}