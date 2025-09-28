/*---------------------------------------------------------*\
| Plasma3D.cpp                                              |
|                                                           |
|   3D Plasma effect with custom UI controls               |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "Plasma3D.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QColorDialog>
#include <algorithm>
#include <cmath>

Plasma3D::Plasma3D(QWidget* parent) : SpatialEffect3D(parent)
{
    pattern_combo = nullptr;
    speed_slider = nullptr;
    brightness_slider = nullptr;
    frequency_slider = nullptr;
    rainbow_mode_check = nullptr;

    color_controls_widget = nullptr;
    color_controls_layout = nullptr;
    add_color_button = nullptr;
    remove_color_button = nullptr;

    pattern_type = 0;        // Default to Classic
    frequency = 50;          // Default frequency
    rainbow_mode = true;     // Default to rainbow mode
    progress = 0.0f;

    // Initialize with default colors
    colors.push_back(0x000000FF);  // Red
    colors.push_back(0x0000FF00);  // Green
    colors.push_back(0x00FF0000);  // Blue
}

Plasma3D::~Plasma3D()
{
}

EffectInfo3D Plasma3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.effect_name = "3D Plasma";
    info.effect_description = "Animated plasma effect with configurable patterns and complexity";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_PLASMA;
    info.is_reversible = false;
    info.supports_random = true;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = false;
    return info;
}

void Plasma3D::SetupCustomUI(QWidget* parent)
{
    QWidget* plasma_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(plasma_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    /*---------------------------------------------------------*\
    | Row 0: Pattern Type                                      |
    \*---------------------------------------------------------*/
    layout->addWidget(new QLabel("Pattern:"), 0, 0);
    pattern_combo = new QComboBox();
    pattern_combo->addItem("Classic");
    pattern_combo->addItem("Swirl");
    pattern_combo->addItem("Ripple");
    pattern_combo->addItem("Organic");
    pattern_combo->setCurrentIndex(pattern_type);
    layout->addWidget(pattern_combo, 0, 1);

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
        parent->layout()->addWidget(plasma_widget);
    }

    SetupColorControls(parent);

    /*---------------------------------------------------------*\
    | Connect signals                                          |
    \*---------------------------------------------------------*/
    connect(pattern_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Plasma3D::OnPlasmaParameterChanged);
    connect(speed_slider, &QSlider::valueChanged, this, &Plasma3D::OnPlasmaParameterChanged);
    connect(brightness_slider, &QSlider::valueChanged, this, &Plasma3D::OnPlasmaParameterChanged);
    connect(frequency_slider, &QSlider::valueChanged, this, &Plasma3D::OnPlasmaParameterChanged);
    connect(rainbow_mode_check, &QCheckBox::toggled, this, &Plasma3D::OnRainbowModeChanged);
}

void Plasma3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_PLASMA;
}

void Plasma3D::OnPlasmaParameterChanged()
{
    /*---------------------------------------------------------*\
    | Update internal parameters                               |
    \*---------------------------------------------------------*/
    if(pattern_combo) pattern_type = pattern_combo->currentIndex();
    if(speed_slider) effect_speed = speed_slider->value();
    if(brightness_slider) effect_brightness = brightness_slider->value();
    if(frequency_slider) frequency = frequency_slider->value();

    /*---------------------------------------------------------*\
    | Emit parameter change signal                             |
    \*---------------------------------------------------------*/
    emit ParametersChanged();
}

void Plasma3D::OnRainbowModeChanged()
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

void Plasma3D::OnAddColorClicked()
{
    RGBColor new_color = 0xFFFFFF; // White
    colors.push_back(new_color);
    CreateColorButton(new_color);
    emit ParametersChanged();
}

void Plasma3D::OnRemoveColorClicked()
{
    if(colors.size() > 1)
    {
        colors.pop_back();
        RemoveLastColorButton();
        emit ParametersChanged();
    }
}

void Plasma3D::OnColorButtonClicked()
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

RGBColor Plasma3D::CalculateColor(float x, float y, float z, float time)
{
    /*---------------------------------------------------------*\
    | Update progress for animation                            |
    \*---------------------------------------------------------*/
    progress = time * (effect_speed * 0.1f);
    float freq_scale = frequency * 0.01f;

    float plasma_value = 0.0f;

    /*---------------------------------------------------------*\
    | Calculate plasma pattern based on type (improved 3D)    |
    \*---------------------------------------------------------*/
    switch(pattern_type)
    {
        case 0: // Classic plasma
            plasma_value = sin(x * freq_scale + progress) +
                          sin(y * freq_scale + progress * 1.1f) +
                          sin(z * freq_scale + progress * 0.9f);
            break;
        case 1: // Swirl
            {
                float angle = atan2(y, x) + progress * 0.5f;
                float radius = sqrt(x*x + y*y + z*z);
                plasma_value = sin(angle * 3.0f + radius * freq_scale + progress) +
                              cos(z * freq_scale * 0.5f + progress * 0.8f);
            }
            break;
        case 2: // Ripple
            {
                float distance = sqrt(x*x + y*y + z*z);
                plasma_value = sin(distance * freq_scale - progress * 2.0f) +
                              cos(x * freq_scale + y * freq_scale + progress);
            }
            break;
        case 3: // Organic
            plasma_value = sin(x * freq_scale * 0.7f + sin(y * freq_scale * 1.3f + progress) + progress * 0.7f) +
                          cos(y * freq_scale * 0.9f + cos(z * freq_scale * 1.1f + progress * 1.2f) + progress * 0.8f);
            break;
    }

    /*---------------------------------------------------------*\
    | Normalize to 0.0 - 1.0                                  |
    \*---------------------------------------------------------*/
    plasma_value = (plasma_value + 2.0f) * 0.25f;
    plasma_value = fmax(0.0f, fmin(1.0f, plasma_value));

    /*---------------------------------------------------------*\
    | Get color based on mode                                  |
    \*---------------------------------------------------------*/
    RGBColor final_color;

    if(rainbow_mode)
    {
        float hue = plasma_value * 360.0f;
        final_color = GetRainbowColor(hue);
    }
    else
    {
        final_color = GetColorAtPosition(plasma_value);
    }

    /*---------------------------------------------------------*\
    | Apply brightness                                         |
    \*---------------------------------------------------------*/
    unsigned char r = (final_color >> 16) & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = final_color & 0xFF;

    float brightness_factor = effect_brightness / 100.0f;
    r = (unsigned char)(r * brightness_factor);
    g = (unsigned char)(g * brightness_factor);
    b = (unsigned char)(b * brightness_factor);

    return (b << 16) | (g << 8) | r;
}

void Plasma3D::SetupColorControls(QWidget* parent)
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
    connect(add_color_button, &QPushButton::clicked, this, &Plasma3D::OnAddColorClicked);
    connect(remove_color_button, &QPushButton::clicked, this, &Plasma3D::OnRemoveColorClicked);
}

void Plasma3D::CreateColorButton(RGBColor color)
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

    connect(button, &QPushButton::clicked, this, &Plasma3D::OnColorButtonClicked);
}

void Plasma3D::RemoveLastColorButton()
{
    if(!color_buttons.empty())
    {
        QPushButton* button = color_buttons.back();
        color_buttons.pop_back();
        color_controls_layout->removeWidget(button);
        delete button;
    }
}

RGBColor Plasma3D::GetRainbowColor(float hue)
{
    /*---------------------------------------------------------*\
    | Convert HSV to RGB                                       |
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

    return (red << 16) | (green << 8) | blue;
}

RGBColor Plasma3D::GetColorAtPosition(float position)
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

    unsigned char r1 = (color1 >> 16) & 0xFF;
    unsigned char g1 = (color1 >> 8) & 0xFF;
    unsigned char b1 = color1 & 0xFF;

    unsigned char r2 = (color2 >> 16) & 0xFF;
    unsigned char g2 = (color2 >> 8) & 0xFF;
    unsigned char b2 = color2 & 0xFF;

    unsigned char r = (unsigned char)(r1 + (r2 - r1) * blend);
    unsigned char g = (unsigned char)(g1 + (g2 - g1) * blend);
    unsigned char b = (unsigned char)(b1 + (b2 - b1) * blend);

    return (b << 16) | (g << 8) | r;
}