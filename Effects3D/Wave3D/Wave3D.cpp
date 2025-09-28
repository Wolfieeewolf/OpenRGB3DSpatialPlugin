/*---------------------------------------------------------*\
| Wave3D.cpp                                                |
|                                                           |
|   3D Wave effect with custom UI controls                 |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "Wave3D.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QColorDialog>
#include <algorithm>
#include <cmath>

Wave3D::Wave3D(QWidget* parent) : SpatialEffect3D(parent)
{
    direction_combo = nullptr;
    speed_slider = nullptr;
    brightness_slider = nullptr;
    frequency_slider = nullptr;
    shape_combo = nullptr;
    reverse_check = nullptr;
    rainbow_mode_check = nullptr;

    color_controls_widget = nullptr;
    color_controls_layout = nullptr;
    add_color_button = nullptr;
    remove_color_button = nullptr;

    direction_type = 3;  // Default to Radial (like RadialRainbow)
    frequency = 50;      // Default frequency like RadialRainbow slider2
    shape_type = 0;      // Circles
    reverse_mode = false;
    rainbow_mode = true; // Default to rainbow like RadialRainbow
    progress = 0.0f;

    // Initialize with default colors (will use rainbow mode by default)
    colors.push_back(0x000000FF);  // Red
    colors.push_back(0x0000FF00);  // Green
    colors.push_back(0x00FF0000);  // Blue
}

Wave3D::~Wave3D()
{
}

EffectInfo3D Wave3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.effect_name = "3D Wave";
    info.effect_description = "3D wave effect with configurable direction and properties";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_WAVE;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;

    return info;
}

void Wave3D::SetupCustomUI(QWidget* parent)
{
    /*---------------------------------------------------------*\
    | Create enhanced Wave controls with sliders              |
    \*---------------------------------------------------------*/
    QWidget* wave_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(wave_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    /*---------------------------------------------------------*\
    | Row 0: Direction Presets                                 |
    \*---------------------------------------------------------*/
    layout->addWidget(new QLabel("Direction:"), 0, 0);
    direction_combo = new QComboBox();
    direction_combo->addItem("X Axis");
    direction_combo->addItem("Y Axis");
    direction_combo->addItem("Z Axis");
    direction_combo->addItem("Radial (3D Sphere)");
    direction_combo->setCurrentIndex(direction_type);
    layout->addWidget(direction_combo, 0, 1);

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
    | Row 4: Shape (like RadialRainbow)                       |
    \*---------------------------------------------------------*/
    layout->addWidget(new QLabel("Shape:"), 4, 0);
    shape_combo = new QComboBox();
    shape_combo->addItem("Circles");
    shape_combo->addItem("Squares");
    shape_combo->setCurrentIndex(shape_type);
    layout->addWidget(shape_combo, 4, 1);

    /*---------------------------------------------------------*\
    | Row 5: Reverse Direction                                 |
    \*---------------------------------------------------------*/
    reverse_check = new QCheckBox("Reverse Direction");
    reverse_check->setChecked(reverse_mode);
    layout->addWidget(reverse_check, 5, 0, 1, 2);

    /*---------------------------------------------------------*\
    | Row 6: Rainbow Mode                                      |
    \*---------------------------------------------------------*/
    rainbow_mode_check = new QCheckBox("Rainbow Mode");
    rainbow_mode_check->setChecked(rainbow_mode);
    layout->addWidget(rainbow_mode_check, 6, 0, 1, 2);

    /*---------------------------------------------------------*\
    | Add to parent layout                                     |
    \*---------------------------------------------------------*/
    if(parent && parent->layout())
    {
        parent->layout()->addWidget(wave_widget);
    }

    SetupColorControls(parent);

    /*---------------------------------------------------------*\
    | Connect signals                                          |
    \*---------------------------------------------------------*/
    connect(direction_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Wave3D::OnWaveParameterChanged);
    connect(speed_slider, &QSlider::valueChanged, this, &Wave3D::OnWaveParameterChanged);
    connect(brightness_slider, &QSlider::valueChanged, this, &Wave3D::OnWaveParameterChanged);
    connect(frequency_slider, &QSlider::valueChanged, this, &Wave3D::OnWaveParameterChanged);
    connect(shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Wave3D::OnWaveParameterChanged);
    connect(reverse_check, &QCheckBox::toggled, this, &Wave3D::OnWaveParameterChanged);
    connect(rainbow_mode_check, &QCheckBox::toggled, this, &Wave3D::OnRainbowModeChanged);
}

void Wave3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_WAVE;
}

void Wave3D::OnWaveParameterChanged()
{
    /*---------------------------------------------------------*\
    | Update internal parameters                               |
    \*---------------------------------------------------------*/
    if(direction_combo)  direction_type = direction_combo->currentIndex();
    if(speed_slider)     effect_speed = speed_slider->value();
    if(brightness_slider) effect_brightness = brightness_slider->value();
    if(frequency_slider) frequency = frequency_slider->value();
    if(shape_combo)      shape_type = shape_combo->currentIndex();
    if(reverse_check)    reverse_mode = reverse_check->isChecked();

    /*---------------------------------------------------------*\
    | Emit parameter change signal                             |
    \*---------------------------------------------------------*/
    emit ParametersChanged();
}

void Wave3D::OnRainbowModeChanged()
{
    if(rainbow_mode_check)
    {
        rainbow_mode = rainbow_mode_check->isChecked();
    }

    /*---------------------------------------------------------*\
    | Show/hide color controls based on rainbow mode          |
    \*---------------------------------------------------------*/
    if(color_controls_widget)
    {
        color_controls_widget->setVisible(!rainbow_mode);
    }

    /*---------------------------------------------------------*\
    | Emit parameter change signal                             |
    \*---------------------------------------------------------*/
    emit ParametersChanged();
}

void Wave3D::OnAddColorClicked()
{
    RGBColor new_color = 0xFFFFFF; // White
    colors.push_back(new_color);
    CreateColorButton(new_color);
    emit ParametersChanged();
}

void Wave3D::OnRemoveColorClicked()
{
    if(colors.size() > 1)
    {
        colors.pop_back();
        RemoveLastColorButton();
        emit ParametersChanged();
    }
}

void Wave3D::OnColorButtonClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if(!button) return;

    // Find which color button was clicked
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
        // Open color dialog
        QColor initial_color;
        initial_color.setRgb(colors[index] & 0xFF, (colors[index] >> 8) & 0xFF, (colors[index] >> 16) & 0xFF);

        QColor new_color = QColorDialog::getColor(initial_color, this);
        if(new_color.isValid())
        {
            colors[index] = (new_color.blue() << 16) | (new_color.green() << 8) | new_color.red();

            // Update button color
            QString style = QString("background-color: rgb(%1, %2, %3);").arg(new_color.red()).arg(new_color.green()).arg(new_color.blue());
            button->setStyleSheet(style);

            emit ParametersChanged();
        }
    }
}

void Wave3D::SetupColorControls(QWidget* parent)
{
    if(!parent || !parent->layout()) return;

    color_controls_widget = new QWidget();
    color_controls_layout = new QHBoxLayout(color_controls_widget);
    color_controls_layout->setContentsMargins(0, 0, 0, 0);

    /*---------------------------------------------------------*\
    | Color + / - buttons                                      |
    \*---------------------------------------------------------*/
    add_color_button = new QPushButton("+");
    add_color_button->setMaximumWidth(30);
    remove_color_button = new QPushButton("-");
    remove_color_button->setMaximumWidth(30);

    color_controls_layout->addWidget(new QLabel("Colors:"));
    color_controls_layout->addWidget(add_color_button);
    color_controls_layout->addWidget(remove_color_button);

    /*---------------------------------------------------------*\
    | Create initial color buttons                             |
    \*---------------------------------------------------------*/
    for(unsigned int i = 0; i < colors.size(); i++)
    {
        CreateColorButton(colors[i]);
    }

    color_controls_layout->addStretch();
    parent->layout()->addWidget(color_controls_widget);

    // Hide by default since rainbow mode is on
    color_controls_widget->setVisible(!rainbow_mode);

    /*---------------------------------------------------------*\
    | Connect signals                                          |
    \*---------------------------------------------------------*/
    connect(add_color_button, &QPushButton::clicked, this, &Wave3D::OnAddColorClicked);
    connect(remove_color_button, &QPushButton::clicked, this, &Wave3D::OnRemoveColorClicked);
}

void Wave3D::CreateColorButton(RGBColor color)
{
    QPushButton* button = new QPushButton();
    button->setMaximumWidth(30);
    button->setMaximumHeight(30);

    // Set button color
    unsigned char r = color & 0xFF;
    unsigned char g = (color >> 8) & 0xFF;
    unsigned char b = (color >> 16) & 0xFF;
    QString style = QString("background-color: rgb(%1, %2, %3);").arg(r).arg(g).arg(b);
    button->setStyleSheet(style);

    color_buttons.push_back(button);
    color_controls_layout->insertWidget(color_controls_layout->count() - 1, button);

    connect(button, &QPushButton::clicked, this, &Wave3D::OnColorButtonClicked);
}

void Wave3D::RemoveLastColorButton()
{
    if(!color_buttons.empty())
    {
        QPushButton* button = color_buttons.back();
        color_buttons.pop_back();
        color_controls_layout->removeWidget(button);
        delete button;
    }
}

RGBColor Wave3D::GetRainbowColor(float hue)
{
    /*---------------------------------------------------------*\
    | Convert HSV to RGB (like RadialRainbow)                 |
    \*---------------------------------------------------------*/
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

RGBColor Wave3D::GetColorAtPosition(float position)
{
    if(colors.empty()) return 0x000000;
    if(colors.size() == 1) return colors[0];

    /*---------------------------------------------------------*\
    | Interpolate between colors                               |
    \*---------------------------------------------------------*/
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

RGBColor Wave3D::CalculateColor(float x, float y, float z, float time)
{
    /*---------------------------------------------------------*\
    | Update progress for animation (like RadialRainbow)      |
    \*---------------------------------------------------------*/
    progress = time * (effect_speed * 2.0f);

    /*---------------------------------------------------------*\
    | Calculate wave based on direction type (like LED cube) |
    \*---------------------------------------------------------*/
    float wave_value = 0.0f;
    float freq_scale = frequency * 0.1f;

    switch(direction_type)
    {
        case 0: // X Axis Wave (left-to-right with Y/Z amplitude)
            wave_value = sin(x * freq_scale + (reverse_mode ? -progress : progress)) *
                        (1.0f + 0.3f * sin(y * 0.5f) + 0.2f * sin(z * 0.3f));
            break;
        case 1: // Y Axis Wave (up-down with X/Z amplitude)
            wave_value = sin(y * freq_scale + (reverse_mode ? -progress : progress)) *
                        (1.0f + 0.3f * sin(x * 0.5f) + 0.2f * sin(z * 0.3f));
            break;
        case 2: // Z Axis Wave (front-back with X/Y amplitude)
            wave_value = sin(z * freq_scale + (reverse_mode ? -progress : progress)) *
                        (1.0f + 0.3f * sin(x * 0.5f) + 0.2f * sin(y * 0.3f));
            break;
        case 3: // Radial (3D Sphere)
        default:
            if(shape_type == 0) // Circles
            {
                float distance = sqrt(x*x + y*y + z*z);
                wave_value = sin(distance * freq_scale + (reverse_mode ? progress : -progress));
            }
            else // Squares (cube distance)
            {
                float distance = std::max({fabs(x), fabs(y), fabs(z)});
                wave_value = sin(distance * freq_scale + (reverse_mode ? progress : -progress));
            }
            break;
    }

    /*---------------------------------------------------------*\
    | Convert wave to hue (0-360 degrees)                     |
    \*---------------------------------------------------------*/
    float hue = (wave_value + 1.0f) * 180.0f;
    hue = fmod(hue, 360.0f);
    if(hue < 0.0f) hue += 360.0f;

    /*---------------------------------------------------------*\
    | Get color based on mode                                  |
    \*---------------------------------------------------------*/
    RGBColor final_color;

    if(rainbow_mode)
    {
        final_color = GetRainbowColor(hue);
    }
    else
    {
        // Use custom colors with position-based selection
        float position = hue / 360.0f;
        final_color = GetColorAtPosition(position);
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