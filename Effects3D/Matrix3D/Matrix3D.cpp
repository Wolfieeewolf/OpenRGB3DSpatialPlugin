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
    density_slider->setToolTip("Column density (higher = more columns)");
    layout->addWidget(density_slider, 0, 1);

    layout->addWidget(new QLabel("Trail Length:"), 1, 0);
    trail_slider = new QSlider(Qt::Horizontal);
    trail_slider->setRange(10, 100);
    trail_slider->setValue(trail);
    trail_slider->setToolTip("Trail length (higher = longer trails)");
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

float Matrix3D::ComputeFaceIntensity(int face,
                                     float x,
                                     float y,
                                     float z,
                                     float time,
                                     const GridContext3D& grid,
                                     float column_spacing,
                                     float size_normalized,
                                     float speed_scale) const
{
    float u = 0.0f;
    float v = 0.0f;
    float axis_value = 0.0f;
    float axis_min = 0.0f;
    float axis_max = 0.0f;
    float face_distance = 0.0f;

    switch(face)
    {
        case 0: // Left wall
            u = y;
            v = z;
            axis_value = z;
            axis_min = grid.min_z;
            axis_max = grid.max_z;
            face_distance = fabsf(x - grid.min_x);
            break;
        case 1: // Right wall
            u = y;
            v = z;
            axis_value = z;
            axis_min = grid.min_z;
            axis_max = grid.max_z;
            face_distance = fabsf(x - grid.max_x);
            break;
        case 2: // Front wall
            u = x;
            v = z;
            axis_value = z;
            axis_min = grid.min_z;
            axis_max = grid.max_z;
            face_distance = fabsf(y - grid.min_y);
            break;
        case 3: // Back wall
            u = x;
            v = z;
            axis_value = z;
            axis_min = grid.min_z;
            axis_max = grid.max_z;
            face_distance = fabsf(y - grid.max_y);
            break;
        case 4: // Floor
            u = x;
            v = y;
            axis_value = z;
            axis_min = grid.min_z;
            axis_max = grid.max_z;
            face_distance = fabsf(z - grid.min_z);
            break;
        case 5: // Ceiling
        default:
            u = x;
            v = y;
            axis_value = z;
            axis_min = grid.min_z;
            axis_max = grid.max_z;
            face_distance = fabsf(z - grid.max_z);
            break;
    }

    int column_u = (int)floorf(u / column_spacing);
    int column_v = (int)floorf(v / column_spacing);
    int column_id = column_u * 73856093 ^ column_v * 19349663;

    float offset = ((column_id & 255) / 255.0f) * 10.0f;
    float head = (axis_max - 0.5f) - fmodf(time * (0.5f + speed_scale * 0.3f) + offset,
                                           (axis_max - axis_min) + 5.0f);

    float distance = fabsf(axis_value - head);
    float trail_length = 1.0f + (trail * 0.05f) * size_normalized;
    float trail_intensity = fmax(0.0f, 1.0f - distance / trail_length);

    float gap = fmodf((float)((column_id >> 8) & 1023), 10.0f) / 10.0f;
    float gap_factor = 0.7f + 0.3f * (gap > 0.2f ? 1.0f : gap * 5.0f);

    float face_falloff = expf(-face_distance * 3.0f);

    float intensity = trail_intensity * gap_factor * face_falloff;
    if(intensity < 0.0f)
    {
        return 0.0f;
    }
    if(intensity > 1.0f)
    {
        return 1.0f;
    }
    return intensity;
}

RGBColor Matrix3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    // Matrix-style rain on selected room surfaces. We render vertical columns (along Z)
    // on walls and project downward onto floor/ceiling as well when selected.
    float speed = GetScaledSpeed();
    float size_m = GetNormalizedSize();

    // Column spacing: higher density -> smaller spacing
    float col_spacing = 1.0f + (100.0f - density) * 0.04f; // 1..5 units

    // Determine which faces to include based on coverage selection
    // effect_coverage indices: 0=Effect Default,1=Entire Room,2=Floor,3=Ceiling,4=Left,5=Right,6=Front,7=Back,8=Floor&Ceil,9=Left&Right,10=Front&Back,11=Origin
    int cov = (int)effect_coverage;
    float intensity = 0.0f;

    switch(cov)
    {
        case 0: // Effect Default -> treat as Entire Room for Matrix vibe
        case 1:
        {
            for(int face_index = 0; face_index < 6; ++face_index)
            {
                float face_value = ComputeFaceIntensity(face_index, x, y, z, time, grid, col_spacing, size_m, speed);
                intensity = fmax(intensity, face_value);
            }
            break;
        }
        case 2: intensity = ComputeFaceIntensity(4, x, y, z, time, grid, col_spacing, size_m, speed); break; // Floor
        case 3: intensity = ComputeFaceIntensity(5, x, y, z, time, grid, col_spacing, size_m, speed); break; // Ceiling
        case 4: intensity = ComputeFaceIntensity(0, x, y, z, time, grid, col_spacing, size_m, speed); break; // Left
        case 5: intensity = ComputeFaceIntensity(1, x, y, z, time, grid, col_spacing, size_m, speed); break; // Right
        case 6: intensity = ComputeFaceIntensity(2, x, y, z, time, grid, col_spacing, size_m, speed); break; // Front
        case 7: intensity = ComputeFaceIntensity(3, x, y, z, time, grid, col_spacing, size_m, speed); break; // Back
        case 8:
        {
            float floor_intensity = ComputeFaceIntensity(4, x, y, z, time, grid, col_spacing, size_m, speed);
            float ceiling_intensity = ComputeFaceIntensity(5, x, y, z, time, grid, col_spacing, size_m, speed);
            intensity = fmax(floor_intensity, ceiling_intensity);
            break;
        }
        case 9:
        {
            float left_intensity = ComputeFaceIntensity(0, x, y, z, time, grid, col_spacing, size_m, speed);
            float right_intensity = ComputeFaceIntensity(1, x, y, z, time, grid, col_spacing, size_m, speed);
            intensity = fmax(left_intensity, right_intensity);
            break;
        }
        case 10:
        {
            float front_intensity = ComputeFaceIntensity(2, x, y, z, time, grid, col_spacing, size_m, speed);
            float back_intensity = ComputeFaceIntensity(3, x, y, z, time, grid, col_spacing, size_m, speed);
            intensity = fmax(front_intensity, back_intensity);
            break;
        }
        case 11:
        {
            for(int face_index = 0; face_index < 6; ++face_index)
            {
                float face_value = ComputeFaceIntensity(face_index, x, y, z, time, grid, col_spacing, size_m, speed);
                intensity = fmax(intensity, face_value);
            }
            break;
        }
        default:
        {
            for(int face_index = 0; face_index < 6; ++face_index)
            {
                float face_value = ComputeFaceIntensity(face_index, x, y, z, time, grid, col_spacing, size_m, speed);
                intensity = fmax(intensity, face_value);
            }
            break;
        }
    }

    // Matrix-green color
    unsigned char r = 0;
    unsigned char g = (unsigned char)(255 * intensity * (GetBrightness() / 100.0f));
    unsigned char b = 0;
    return (b << 16) | (g << 8) | r;
}

nlohmann::json Matrix3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["density"] = density;
    j["trail"] = trail;
    return j;
}

void Matrix3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("density")) density = settings["density"];
    if(settings.contains("trail")) trail = settings["trail"];

    if(density_slider) density_slider->setValue(density);
    if(trail_slider) trail_slider->setValue(trail);
}
