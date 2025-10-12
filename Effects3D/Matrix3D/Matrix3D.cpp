/*---------------------------------------------------------*\
| Matrix3D.cpp                                             |
|                                                          |
|   3D Matrix code rain effect                             |
|                                                          |
|   SPDX-License-Identifier: GPL-2.0-only                  |
\*---------------------------------------------------------*/

#include "Matrix3D.h"
#include <QGridLayout>
#include <cmath>

REGISTER_EFFECT_3D(Matrix3D);

Matrix3D::Matrix3D(QWidget* parent) : SpatialEffect3D(parent)
{
    density_slider = nullptr;
    trail_slider = nullptr;
    density = 60;
    trail = 50;
    SetRainbowMode(false);
}

Matrix3D::~Matrix3D() {}

EffectInfo3D Matrix3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;
    info.effect_name = "3D Matrix";
    info.effect_description = "Matrix-style code rain columns";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_MATRIX;
    info.is_reversible = true;
    info.supports_random = true;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;

    info.default_speed_scale = 15.0f;
    info.default_frequency_scale = 8.0f;
    info.use_size_parameter = true;

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

void Matrix3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0,0,0,0);

    layout->addWidget(new QLabel("Density:"), 0, 0);
    density_slider = new QSlider(Qt::Horizontal);
    density_slider->setRange(10, 100);
    density_slider->setValue(density);
    layout->addWidget(density_slider, 0, 1);

    layout->addWidget(new QLabel("Trail Length:"), 1, 0);
    trail_slider = new QSlider(Qt::Horizontal);
    trail_slider->setRange(10, 100);
    trail_slider->setValue(trail);
    layout->addWidget(trail_slider, 1, 1);

    if(parent && parent->layout()) parent->layout()->addWidget(w);

    connect(density_slider, &QSlider::valueChanged, this, &Matrix3D::OnMatrixParameterChanged);
    connect(trail_slider, &QSlider::valueChanged, this, &Matrix3D::OnMatrixParameterChanged);
}

void Matrix3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_MATRIX;
}

void Matrix3D::OnMatrixParameterChanged()
{
    if(density_slider) density = density_slider->value();
    if(trail_slider) trail = trail_slider->value();
    emit ParametersChanged();
}

RGBColor Matrix3D::CalculateColor(float, float, float, float)
{
    return 0x00000000;
}

RGBColor Matrix3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    // Columns run along selected axis (default -Y); compute column index from orthogonal plane
    float speed = GetScaledSpeed();
    float size_m = GetNormalizedSize();

    float col_spacing = 1.0f + (100.0f - density) * 0.04f; // 1..5 units
    // Map to plane coords based on axis
    float a, u, v, a_min, a_max, a_span;
    EffectAxis use_axis = axis_none ? AXIS_Y : effect_axis;
    switch(use_axis)
    {
        case AXIS_X: a = x; u = y; v = z; a_min = grid.min_x; a_max = grid.max_x; a_span = grid.width; break;
        case AXIS_Y: a = y; u = x; v = z; a_min = grid.min_y; a_max = grid.max_y; a_span = grid.depth; break;
        case AXIS_Z: a = z; u = x; v = y; a_min = grid.min_z; a_max = grid.max_z; a_span = grid.height; break;
        case AXIS_RADIAL:
        default:
            a = y; u = x; v = z; a_min = grid.min_y; a_max = grid.max_y; a_span = grid.depth; break;
    }

    int col_u = (int)floorf(u / col_spacing);
    int col_v = (int)floorf(v / col_spacing);
    int col_id = col_u * 73856093 ^ col_v * 19349663;

    // Head position per column along axis a
    float offset = ((col_id & 255) / 255.0f) * 10.0f;
    float head = (a_max - 0.5f) - fmodf(time * (0.5f + speed * 0.3f) + offset, (a_max - a_min) + 5.0f);

    // Distance from head gives trail
    float d = fabsf(a - head);
    float trail_len = 1.0f + (trail * 0.05f) * size_m; // ~1..6
    float trail_intensity = fmax(0.0f, 1.0f - d / trail_len);

    // Fade columns slightly based on modulus to create gaps
    float gap = fmodf((float)((col_id >> 8) & 1023), 10.0f) / 10.0f;
    float gap_factor = 0.7f + 0.3f * (gap > 0.2f ? 1.0f : gap * 5.0f);

    float intensity = trail_intensity * gap_factor;
    intensity = fmax(0.0f, fmin(1.0f, intensity));

    // Matrix green
    unsigned char r = 0;
    unsigned char g = (unsigned char)(255 * intensity * (GetBrightness() / 100.0f));
    unsigned char b = 0;
    return (b << 16) | (g << 8) | r;
}
