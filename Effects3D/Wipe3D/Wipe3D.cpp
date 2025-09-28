/*---------------------------------------------------------*\
| Wipe3D.cpp                                                |
|                                                           |
|   3D Wipe effect with directional transitions           |
|                                                           |
|   Date: 2025-09-28                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "Wipe3D.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QColorDialog>
#include <algorithm>
#include <cmath>

/*---------------------------------------------------------*\
| Helper function for smooth interpolation                 |
\*---------------------------------------------------------*/
static float smoothstep(float edge0, float edge1, float x)
{
    float t = fmax(0.0f, fmin(1.0f, (x - edge0) / (edge1 - edge0)));
    return t * t * (3.0f - 2.0f * t);
}

Wipe3D::Wipe3D(QWidget* parent) : SpatialEffect3D(parent)
{
    direction_combo = nullptr;
    speed_slider = nullptr;
    brightness_slider = nullptr;
    thickness_slider = nullptr;
    shape_combo = nullptr;
    reverse_check = nullptr;

    color_controls_widget = nullptr;
    color_controls_layout = nullptr;
    add_color_button = nullptr;
    remove_color_button = nullptr;

    direction_type = 0;      // Default to X Axis
    thickness = 30;          // Default thickness
    shape_type = 0;          // Round edge
    reverse_mode = false;
    progress = 0.0f;

    // Initialize with default colors
    colors.push_back(0x000000FF);  // Red
    colors.push_back(0x0000FF00);  // Green
    colors.push_back(0x00FF0000);  // Blue
}

Wipe3D::~Wipe3D()
{
}

EffectInfo3D Wipe3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.effect_name = "3D Wipe";
    info.effect_description = "3D wipe effect with configurable direction and edge shapes";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_WIPE;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = true;
    info.needs_arms = false;
    info.needs_frequency = false;

    return info;
}

void Wipe3D::SetupCustomUI(QWidget* parent)
{
    /*---------------------------------------------------------*\
    | Create enhanced Wipe controls with sliders              |
    \*---------------------------------------------------------*/
    QWidget* wipe_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(wipe_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    /*---------------------------------------------------------*\
    | Row 0: Direction Presets                                 |
    \*---------------------------------------------------------*/
    layout->addWidget(new QLabel("Direction:"), 0, 0);
    direction_combo = new QComboBox();
    direction_combo->addItem("X Axis");
    direction_combo->addItem("Y Axis");
    direction_combo->addItem("Z Axis");
    direction_combo->addItem("Radial");
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
    | Row 3: Thickness                                         |
    \*---------------------------------------------------------*/
    layout->addWidget(new QLabel("Thickness:"), 3, 0);
    thickness_slider = new QSlider(Qt::Horizontal);
    thickness_slider->setRange(5, 100);
    thickness_slider->setValue(thickness);
    layout->addWidget(thickness_slider, 3, 1);

    /*---------------------------------------------------------*\
    | Row 4: Edge Shape                                        |
    \*---------------------------------------------------------*/
    layout->addWidget(new QLabel("Edge Shape:"), 4, 0);
    shape_combo = new QComboBox();
    shape_combo->addItem("Round");
    shape_combo->addItem("Point");
    shape_combo->addItem("Square");
    shape_combo->setCurrentIndex(shape_type);
    layout->addWidget(shape_combo, 4, 1);

    /*---------------------------------------------------------*\
    | Row 5: Reverse Direction                                 |
    \*---------------------------------------------------------*/
    reverse_check = new QCheckBox("Reverse Direction");
    reverse_check->setChecked(reverse_mode);
    layout->addWidget(reverse_check, 5, 0, 1, 2);

    /*---------------------------------------------------------*\
    | Add to parent layout                                     |
    \*---------------------------------------------------------*/
    if(parent && parent->layout())
    {
        parent->layout()->addWidget(wipe_widget);
    }

    SetupColorControls(parent);

    /*---------------------------------------------------------*\
    | Connect signals                                          |
    \*---------------------------------------------------------*/
    connect(direction_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Wipe3D::OnWipeParameterChanged);
    connect(speed_slider, &QSlider::valueChanged, this, &Wipe3D::OnWipeParameterChanged);
    connect(brightness_slider, &QSlider::valueChanged, this, &Wipe3D::OnWipeParameterChanged);
    connect(thickness_slider, &QSlider::valueChanged, this, &Wipe3D::OnWipeParameterChanged);
    connect(shape_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Wipe3D::OnWipeParameterChanged);
    connect(reverse_check, &QCheckBox::toggled, this, &Wipe3D::OnWipeParameterChanged);
}

void Wipe3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_WIPE;
}

void Wipe3D::OnWipeParameterChanged()
{
    /*---------------------------------------------------------*\
    | Update internal parameters                               |
    \*---------------------------------------------------------*/
    if(direction_combo)  direction_type = direction_combo->currentIndex();
    if(speed_slider)     effect_speed = speed_slider->value();
    if(brightness_slider) effect_brightness = brightness_slider->value();
    if(thickness_slider) thickness = thickness_slider->value();
    if(shape_combo)      shape_type = shape_combo->currentIndex();
    if(reverse_check)    reverse_mode = reverse_check->isChecked();

    /*---------------------------------------------------------*\
    | Emit parameter change signal                             |
    \*---------------------------------------------------------*/
    emit ParametersChanged();
}

void Wipe3D::OnAddColorClicked()
{
    RGBColor new_color = 0xFFFFFF; // White
    colors.push_back(new_color);
    CreateColorButton(new_color);
    emit ParametersChanged();
}

void Wipe3D::OnRemoveColorClicked()
{
    if(colors.size() > 1)
    {
        colors.pop_back();
        RemoveLastColorButton();
        emit ParametersChanged();
    }
}

void Wipe3D::OnColorButtonClicked()
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

void Wipe3D::SetupColorControls(QWidget* parent)
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

    /*---------------------------------------------------------*\
    | Connect signals                                          |
    \*---------------------------------------------------------*/
    connect(add_color_button, &QPushButton::clicked, this, &Wipe3D::OnAddColorClicked);
    connect(remove_color_button, &QPushButton::clicked, this, &Wipe3D::OnRemoveColorClicked);
}

void Wipe3D::CreateColorButton(RGBColor color)
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

    connect(button, &QPushButton::clicked, this, &Wipe3D::OnColorButtonClicked);
}

void Wipe3D::RemoveLastColorButton()
{
    if(!color_buttons.empty())
    {
        QPushButton* button = color_buttons.back();
        color_buttons.pop_back();
        color_controls_layout->removeWidget(button);
        delete button;
    }
}

RGBColor Wipe3D::GetColorAtPosition(float position)
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

float Wipe3D::CalculateWipeDistance(float x, float y, float z)
{
    switch(direction_type)
    {
        case 0: // X Axis
            return reverse_mode ? -x : x;
        case 1: // Y Axis
            return reverse_mode ? -y : y;
        case 2: // Z Axis
            return reverse_mode ? -z : z;
        case 3: // Radial
        default:
            return sqrt(x*x + y*y + z*z) * (reverse_mode ? -1.0f : 1.0f);
    }
}

float Wipe3D::ApplyEdgeShape(float /*distance*/, float edge_distance)
{
    float thickness_factor = thickness * 0.1f;

    switch(shape_type)
    {
        case 0: // Round
            return 1.0f - smoothstep(0.0f, thickness_factor, edge_distance);
        case 1: // Point (sharp triangle)
            return fmax(0.0f, 1.0f - (edge_distance / thickness_factor));
        case 2: // Square (hard edge)
        default:
            return (edge_distance < thickness_factor) ? 1.0f : 0.0f;
    }
}

RGBColor Wipe3D::CalculateColor(float x, float y, float z, float time)
{
    /*---------------------------------------------------------*\
    | Create smooth curves for speed                           |
    \*---------------------------------------------------------*/
    float speed_curve = (effect_speed / 100.0f);
    speed_curve = speed_curve * speed_curve; // Quadratic curve for smoother control
    float actual_speed = speed_curve * 200.0f; // Map back to 0-200 range

    /*---------------------------------------------------------*\
    | Update progress for animation                            |
    \*---------------------------------------------------------*/
    progress = time * (actual_speed * 0.5f);

    /*---------------------------------------------------------*\
    | Calculate distance along wipe direction                  |
    \*---------------------------------------------------------*/
    float distance = CalculateWipeDistance(x, y, z);

    /*---------------------------------------------------------*\
    | Calculate wipe position (-20 to +20 range)              |
    \*---------------------------------------------------------*/
    float wipe_position = progress - 20.0f;
    float edge_distance = fabs(distance - wipe_position);

    /*---------------------------------------------------------*\
    | Apply edge shape and get intensity                       |
    \*---------------------------------------------------------*/
    float intensity = ApplyEdgeShape(distance, edge_distance);

    if(intensity <= 0.0f)
    {
        return 0x000000; // Black (no color)
    }

    /*---------------------------------------------------------*\
    | Get color based on position in wipe                     |
    \*---------------------------------------------------------*/
    float color_position = fmod(distance * 0.1f + progress * 0.05f, 1.0f);
    RGBColor base_color = GetColorAtPosition(color_position);

    /*---------------------------------------------------------*\
    | Apply intensity and brightness                           |
    \*---------------------------------------------------------*/
    unsigned char r = (base_color >> 16) & 0xFF;
    unsigned char g = (base_color >> 8) & 0xFF;
    unsigned char b = base_color & 0xFF;

    float brightness_factor = (effect_brightness / 100.0f) * intensity;
    r = (unsigned char)(r * brightness_factor);
    g = (unsigned char)(g * brightness_factor);
    b = (unsigned char)(b * brightness_factor);

    return (b << 16) | (g << 8) | r;
}