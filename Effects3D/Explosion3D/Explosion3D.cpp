/*---------------------------------------------------------*\
| Explosion3D.cpp                                           |
|                                                           |
|   3D Explosion effect with shockwave animation           |
|                                                           |
|   Date: 2025-09-28                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "Explosion3D.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QColorDialog>
#include <algorithm>
#include <cmath>

// Helper function for smooth interpolation
static float smoothstep(float edge0, float edge1, float x)
{
    float t = fmax(0.0f, fmin(1.0f, (x - edge0) / (edge1 - edge0)));
    return t * t * (3.0f - 2.0f * t);
}

Explosion3D::Explosion3D(QWidget* parent) : SpatialEffect3D(parent)
{
    intensity_slider = nullptr;
    speed_slider = nullptr;
    brightness_slider = nullptr;
    frequency_slider = nullptr;
    rainbow_mode_check = nullptr;

    color_controls_widget = nullptr;
    color_controls_layout = nullptr;
    add_color_button = nullptr;
    remove_color_button = nullptr;

    explosion_intensity = 75;   // Default explosion intensity
    frequency = 50;             // Default shockwave frequency
    rainbow_mode = true;        // Default to rainbow mode
    progress = 0.0f;

    // Initialize with default colors
    colors.push_back(0x000000FF);  // Red
    colors.push_back(0x0000FFFF);  // Yellow
    colors.push_back(0x00FF0000);  // Blue
}

Explosion3D::~Explosion3D()
{
}

EffectInfo3D Explosion3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.effect_name = "3D Explosion";
    info.effect_description = "Expanding shockwave explosion with multiple wave layers";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_EXPLOSION;
    info.is_reversible = false;
    info.supports_random = true;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;
    return info;
}

void Explosion3D::SetupCustomUI(QWidget* parent)
{
    QWidget* explosion_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(explosion_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    // Row 0: Explosion Intensity
    layout->addWidget(new QLabel("Intensity:"), 0, 0);
    intensity_slider = new QSlider(Qt::Horizontal);
    intensity_slider->setRange(10, 200);
    intensity_slider->setValue(explosion_intensity);
    layout->addWidget(intensity_slider, 0, 1);

    // Row 1: Speed
    layout->addWidget(new QLabel("Speed:"), 1, 0);
    speed_slider = new QSlider(Qt::Horizontal);
    speed_slider->setRange(1, 200);
    speed_slider->setValue(effect_speed);
    layout->addWidget(speed_slider, 1, 1);

    // Row 2: Brightness
    layout->addWidget(new QLabel("Brightness:"), 2, 0);
    brightness_slider = new QSlider(Qt::Horizontal);
    brightness_slider->setRange(1, 100);
    brightness_slider->setValue(effect_brightness);
    layout->addWidget(brightness_slider, 2, 1);

    // Row 3: Frequency
    layout->addWidget(new QLabel("Frequency:"), 3, 0);
    frequency_slider = new QSlider(Qt::Horizontal);
    frequency_slider->setRange(1, 100);
    frequency_slider->setValue(frequency);
    layout->addWidget(frequency_slider, 3, 1);

    // Row 4: Rainbow Mode
    rainbow_mode_check = new QCheckBox("Rainbow Mode");
    rainbow_mode_check->setChecked(rainbow_mode);
    layout->addWidget(rainbow_mode_check, 4, 0, 1, 2);

    if(parent && parent->layout())
    {
        parent->layout()->addWidget(explosion_widget);
    }

    SetupColorControls(parent);

    // Connect signals
    connect(intensity_slider, &QSlider::valueChanged, this, &Explosion3D::OnExplosionParameterChanged);
    connect(speed_slider, &QSlider::valueChanged, this, &Explosion3D::OnExplosionParameterChanged);
    connect(brightness_slider, &QSlider::valueChanged, this, &Explosion3D::OnExplosionParameterChanged);
    connect(frequency_slider, &QSlider::valueChanged, this, &Explosion3D::OnExplosionParameterChanged);
    connect(rainbow_mode_check, &QCheckBox::toggled, this, &Explosion3D::OnRainbowModeChanged);
}

void Explosion3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_EXPLOSION;
}

void Explosion3D::OnExplosionParameterChanged()
{
    if(intensity_slider) explosion_intensity = intensity_slider->value();
    if(speed_slider) effect_speed = speed_slider->value();
    if(brightness_slider) effect_brightness = brightness_slider->value();
    if(frequency_slider) frequency = frequency_slider->value();
    emit ParametersChanged();
}

void Explosion3D::OnRainbowModeChanged()
{
    if(rainbow_mode_check) rainbow_mode = rainbow_mode_check->isChecked();
    if(color_controls_widget) color_controls_widget->setVisible(!rainbow_mode);
    emit ParametersChanged();
}

void Explosion3D::OnAddColorClicked()
{
    RGBColor new_color = 0x00FFFFFF; // White
    colors.push_back(new_color);
    CreateColorButton(new_color);
    emit ParametersChanged();
}

void Explosion3D::OnRemoveColorClicked()
{
    if(colors.size() > 1)
    {
        colors.pop_back();
        RemoveLastColorButton();
        emit ParametersChanged();
    }
}

void Explosion3D::OnColorButtonClicked()
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

RGBColor Explosion3D::CalculateColor(float x, float y, float z, float time)
{
    // Update progress for animation
    progress = time * (effect_speed * 0.1f);
    float freq_scale = frequency * 0.01f;

    // Calculate distance from center (3D sphere)
    float distance = sqrt(x*x + y*y + z*z);

    // Create expanding shockwave - main explosion wave
    float explosion_radius = progress * (explosion_intensity * 0.1f);
    float wave_thickness = 3.0f + explosion_intensity * 0.05f;

    // Primary shockwave
    float primary_wave = 1.0f - smoothstep(explosion_radius - wave_thickness, explosion_radius + wave_thickness, distance);
    primary_wave *= exp(-fabs(distance - explosion_radius) * 0.1f); // Exponential falloff

    // Secondary shockwave (following behind)
    float secondary_radius = explosion_radius * 0.7f;
    float secondary_wave = 1.0f - smoothstep(secondary_radius - wave_thickness * 0.5f, secondary_radius + wave_thickness * 0.5f, distance);
    secondary_wave *= exp(-fabs(distance - secondary_radius) * 0.15f) * 0.6f;

    // Add high-frequency shock details
    float shock_detail = 0.2f * sin(distance * freq_scale * 8.0f - progress * 4.0f);
    shock_detail *= exp(-distance * 0.1f); // Fade with distance

    // Combine all explosion layers
    float explosion_intensity_final = primary_wave + secondary_wave + shock_detail;
    explosion_intensity_final = fmax(0.0f, fmin(1.0f, explosion_intensity_final));

    // Add inner core explosion (bright center)
    if(distance < explosion_radius * 0.3f)
    {
        float core_intensity = 1.0f - (distance / (explosion_radius * 0.3f));
        explosion_intensity_final = fmax(explosion_intensity_final, core_intensity * 0.8f);
    }

    // Get color based on mode
    RGBColor final_color;

    if(rainbow_mode)
    {
        // Rainbow colors - hot to cool based on explosion distance and intensity
        float hue = 60.0f - (explosion_intensity_final * 60.0f) + progress * 10.0f; // Red-hot to blue-cool
        hue = fmax(0.0f, hue);
        final_color = GetRainbowColor(hue);
    }
    else
    {
        // Use custom colors based on explosion intensity
        final_color = GetColorAtPosition(explosion_intensity_final);
    }

    // Apply intensity and brightness
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;

    float brightness_factor = (effect_brightness / 100.0f) * explosion_intensity_final;
    r = (unsigned char)(r * brightness_factor);
    g = (unsigned char)(g * brightness_factor);
    b = (unsigned char)(b * brightness_factor);

    return (b << 16) | (g << 8) | r;
}

void Explosion3D::SetupColorControls(QWidget* parent)
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

    connect(add_color_button, &QPushButton::clicked, this, &Explosion3D::OnAddColorClicked);
    connect(remove_color_button, &QPushButton::clicked, this, &Explosion3D::OnRemoveColorClicked);
}

void Explosion3D::CreateColorButton(RGBColor color)
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
    connect(button, &QPushButton::clicked, this, &Explosion3D::OnColorButtonClicked);
}

void Explosion3D::RemoveLastColorButton()
{
    if(!color_buttons.empty())
    {
        QPushButton* button = color_buttons.back();
        color_buttons.pop_back();
        color_controls_layout->removeWidget(button);
        delete button;
    }
}

RGBColor Explosion3D::GetRainbowColor(float hue)
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

RGBColor Explosion3D::GetColorAtPosition(float position)
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