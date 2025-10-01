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
    origin_combo = nullptr;
    intensity_slider = nullptr;
    frequency_slider = nullptr;
    rainbow_mode_check = nullptr;

    color_controls_widget = nullptr;
    color_controls_layout = nullptr;
    add_color_button = nullptr;
    remove_color_button = nullptr;

    origin_preset = ORIGIN_FLOOR_CENTER;  // Default to floor center for explosions
    explosion_intensity = 75;   // Default explosion intensity
    frequency = 50;             // Default shockwave frequency
    rainbow_mode = true;        // Default to rainbow mode
    progress = 0.0f;

    // Initialize with default colors
    colors.push_back(0x000000FF);  // Blue
    colors.push_back(0x0000FFFF);  // Yellow
    colors.push_back(0x00FF0000);  // Red
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

    // Row 0: Origin Preset
    layout->addWidget(new QLabel("Origin:"), 0, 0);
    origin_combo = new QComboBox();
    origin_combo->addItem("Room Center");
    origin_combo->addItem("Floor Center");
    origin_combo->addItem("Ceiling Center");
    origin_combo->addItem("Front Wall");
    origin_combo->addItem("Back Wall");
    origin_combo->addItem("Left Wall");
    origin_combo->addItem("Right Wall");
    origin_combo->addItem("Floor Front");
    origin_combo->addItem("Floor Back");
    origin_combo->setCurrentIndex(origin_preset);
    layout->addWidget(origin_combo, 0, 1);

    // Row 1: Explosion Intensity
    layout->addWidget(new QLabel("Intensity:"), 1, 0);
    intensity_slider = new QSlider(Qt::Horizontal);
    intensity_slider->setRange(10, 200);
    intensity_slider->setValue(explosion_intensity);
    layout->addWidget(intensity_slider, 1, 1);

    // Row 2: Frequency
    layout->addWidget(new QLabel("Frequency:"), 2, 0);
    frequency_slider = new QSlider(Qt::Horizontal);
    frequency_slider->setRange(1, 100);
    frequency_slider->setValue(frequency);
    layout->addWidget(frequency_slider, 2, 1);

    // Row 3: Rainbow Mode
    rainbow_mode_check = new QCheckBox("Rainbow Mode");
    rainbow_mode_check->setChecked(rainbow_mode);
    layout->addWidget(rainbow_mode_check, 3, 0, 1, 2);

    if(parent && parent->layout())
    {
        parent->layout()->addWidget(explosion_widget);
    }

    SetupColorControls(parent);

    // Connect signals
    connect(origin_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Explosion3D::OnExplosionParameterChanged);
    connect(intensity_slider, &QSlider::valueChanged, this, &Explosion3D::OnExplosionParameterChanged);
    connect(frequency_slider, &QSlider::valueChanged, this, &Explosion3D::OnExplosionParameterChanged);
    connect(rainbow_mode_check, &QCheckBox::toggled, this, &Explosion3D::OnRainbowModeChanged);
}

void Explosion3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_EXPLOSION;
}

void Explosion3D::OnExplosionParameterChanged()
{
    if(origin_combo) origin_preset = (OriginPreset)origin_combo->currentIndex();
    if(intensity_slider) explosion_intensity = intensity_slider->value();
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
    // Create smooth curves for speed and frequency
    float speed_curve = (effect_speed / 100.0f);
    speed_curve = speed_curve * speed_curve; // Quadratic curve for smoother control
    float actual_speed = speed_curve * 200.0f; // Map back to 0-200 range

    float freq_curve = (frequency / 100.0f);
    freq_curve = freq_curve * freq_curve; // Quadratic curve for smoother control
    float actual_frequency = freq_curve * 100.0f; // Map to 0-100 range

    // Update progress for animation
    progress = time * (actual_speed * 0.1f);
    float freq_scale = actual_frequency * 0.01f;

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

RGBColor Explosion3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    (void)grid;
    return CalculateColor(x, y, z, time);
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