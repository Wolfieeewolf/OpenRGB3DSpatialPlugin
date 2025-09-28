/*---------------------------------------------------------*\
| DNAHelix3D.cpp                                            |
|                                                           |
|   3D DNA Helix effect with enhanced controls             |
|                                                           |
|   Date: 2025-09-28                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "DNAHelix3D.h"
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

DNAHelix3D::DNAHelix3D(QWidget* parent) : SpatialEffect3D(parent)
{
    radius_slider = nullptr;
    speed_slider = nullptr;
    brightness_slider = nullptr;
    frequency_slider = nullptr;
    rainbow_mode_check = nullptr;

    color_controls_widget = nullptr;
    color_controls_layout = nullptr;
    add_color_button = nullptr;
    remove_color_button = nullptr;

    helix_radius = 75;       // Default helix radius
    frequency = 50;          // Default DNA pitch frequency
    rainbow_mode = true;     // Default to rainbow mode
    progress = 0.0f;

    // Initialize with default colors (DNA base pairs)
    colors.push_back(0x000000FF);  // Red (Adenine)
    colors.push_back(0x0000FFFF);  // Yellow (Thymine)
    colors.push_back(0x0000FF00);  // Green (Guanine)
    colors.push_back(0x00FF0000);  // Blue (Cytosine)
}

DNAHelix3D::~DNAHelix3D()
{
}

EffectInfo3D DNAHelix3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.effect_name = "3D DNA Helix";
    info.effect_description = "Double helix pattern with base pairs and rainbow colors";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_DNA_HELIX;
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

void DNAHelix3D::SetupCustomUI(QWidget* parent)
{
    QWidget* dna_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(dna_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    // Row 0: Helix Radius
    layout->addWidget(new QLabel("Radius:"), 0, 0);
    radius_slider = new QSlider(Qt::Horizontal);
    radius_slider->setRange(20, 150);
    radius_slider->setValue(helix_radius);
    layout->addWidget(radius_slider, 0, 1);

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
        parent->layout()->addWidget(dna_widget);
    }

    SetupColorControls(parent);

    // Connect signals
    connect(radius_slider, &QSlider::valueChanged, this, &DNAHelix3D::OnDNAParameterChanged);
    connect(speed_slider, &QSlider::valueChanged, this, &DNAHelix3D::OnDNAParameterChanged);
    connect(brightness_slider, &QSlider::valueChanged, this, &DNAHelix3D::OnDNAParameterChanged);
    connect(frequency_slider, &QSlider::valueChanged, this, &DNAHelix3D::OnDNAParameterChanged);
    connect(rainbow_mode_check, &QCheckBox::toggled, this, &DNAHelix3D::OnRainbowModeChanged);
}

void DNAHelix3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_DNA_HELIX;
}

void DNAHelix3D::OnDNAParameterChanged()
{
    if(radius_slider) helix_radius = radius_slider->value();
    if(speed_slider) effect_speed = speed_slider->value();
    if(brightness_slider) effect_brightness = brightness_slider->value();
    if(frequency_slider) frequency = frequency_slider->value();
    emit ParametersChanged();
}

void DNAHelix3D::OnRainbowModeChanged()
{
    if(rainbow_mode_check) rainbow_mode = rainbow_mode_check->isChecked();
    if(color_controls_widget) color_controls_widget->setVisible(!rainbow_mode);
    emit ParametersChanged();
}

void DNAHelix3D::OnAddColorClicked()
{
    RGBColor new_color = 0x00FFFFFF; // White
    colors.push_back(new_color);
    CreateColorButton(new_color);
    emit ParametersChanged();
}

void DNAHelix3D::OnRemoveColorClicked()
{
    if(colors.size() > 1)
    {
        colors.pop_back();
        RemoveLastColorButton();
        emit ParametersChanged();
    }
}

void DNAHelix3D::OnColorButtonClicked()
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

RGBColor DNAHelix3D::CalculateColor(float x, float y, float z, float time)
{
    // Update progress for animation
    progress = time * (effect_speed * 0.1f);
    float freq_scale = frequency * 0.01f;
    float radius_scale = helix_radius * 0.01f;

    // Calculate distance from Z-axis (center of DNA)
    float radial_distance = sqrt(x*x + y*y);
    float angle = atan2(y, x);

    // Create double helix - two intertwined spirals
    float helix_height = z * freq_scale + progress;

    // First helix (major groove)
    float helix1_angle = angle + helix_height;
    float helix1_x = radius_scale * cos(helix1_angle);
    float helix1_y = radius_scale * sin(helix1_angle);
    float helix1_distance = sqrt((x - helix1_x)*(x - helix1_x) + (y - helix1_y)*(y - helix1_y));

    // Second helix (180 degrees offset)
    float helix2_angle = angle + helix_height + 3.14159f;
    float helix2_x = radius_scale * cos(helix2_angle);
    float helix2_y = radius_scale * sin(helix2_angle);
    float helix2_distance = sqrt((x - helix2_x)*(x - helix2_x) + (y - helix2_y)*(y - helix2_y));

    // Calculate helix strand intensities with smooth falloff
    float strand_thickness = 0.3f + radius_scale * 0.2f;
    float helix1_intensity = 1.0f - smoothstep(0.0f, strand_thickness, helix1_distance);
    float helix2_intensity = 1.0f - smoothstep(0.0f, strand_thickness, helix2_distance);

    // Add base pair connections (rungs of the DNA ladder)
    float base_pair_frequency = freq_scale * 4.0f;
    float base_pair_phase = sin(z * base_pair_frequency + progress * 0.5f);
    float base_pair_connection = 0.0f;

    if(base_pair_phase > 0.7f && radial_distance < radius_scale * 1.2f)
    {
        float connection_strength = (base_pair_phase - 0.7f) / 0.3f;
        float radial_falloff = 1.0f - smoothstep(0.0f, radius_scale * 1.2f, radial_distance);
        base_pair_connection = connection_strength * radial_falloff * 0.6f;
    }

    // Combine all DNA elements
    float total_intensity = fmax(helix1_intensity, helix2_intensity) + base_pair_connection;
    total_intensity = fmax(0.0f, fmin(1.0f, total_intensity));

    // Add minor groove effects for realism
    float minor_groove = 0.1f * sin(helix_height * 2.0f) * exp(-radial_distance * 0.5f);
    total_intensity += minor_groove;

    // Get color based on mode
    RGBColor final_color;

    if(rainbow_mode)
    {
        // Rainbow colors that shift along the helix
        float hue = helix_height * 30.0f + radial_distance * 50.0f;
        final_color = GetRainbowColor(hue);
    }
    else
    {
        // Use different colors for different parts of the DNA
        float color_selector = helix_height * 0.5f + base_pair_phase;
        final_color = GetColorAtPosition(color_selector);
    }

    // Apply intensity and brightness
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;

    float brightness_factor = (effect_brightness / 100.0f) * total_intensity;
    r = (unsigned char)(r * brightness_factor);
    g = (unsigned char)(g * brightness_factor);
    b = (unsigned char)(b * brightness_factor);

    return (b << 16) | (g << 8) | r;
}

void DNAHelix3D::SetupColorControls(QWidget* parent)
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

    connect(add_color_button, &QPushButton::clicked, this, &DNAHelix3D::OnAddColorClicked);
    connect(remove_color_button, &QPushButton::clicked, this, &DNAHelix3D::OnRemoveColorClicked);
}

void DNAHelix3D::CreateColorButton(RGBColor color)
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
    connect(button, &QPushButton::clicked, this, &DNAHelix3D::OnColorButtonClicked);
}

void DNAHelix3D::RemoveLastColorButton()
{
    if(!color_buttons.empty())
    {
        QPushButton* button = color_buttons.back();
        color_buttons.pop_back();
        color_controls_layout->removeWidget(button);
        delete button;
    }
}

RGBColor DNAHelix3D::GetRainbowColor(float hue)
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

RGBColor DNAHelix3D::GetColorAtPosition(float position)
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
