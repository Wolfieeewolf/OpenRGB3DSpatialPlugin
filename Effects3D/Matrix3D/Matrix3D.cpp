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

RGBColor Matrix3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    // Matrix-style rain on selected room surfaces. We render vertical columns (along Z)
    // on walls and project downward onto floor/ceiling as well when selected.
    float speed = GetScaledSpeed();
    float size_m = GetNormalizedSize();

    // Column spacing: higher density -> smaller spacing
    float col_spacing = 1.0f + (100.0f - density) * 0.04f; // 1..5 units

    auto face_intensity = [&](int face) -> float {
        // face: 0=Left(X=min),1=Right(X=max),2=Front(Y=min),3=Back(Y=max),4=Floor(Z=min),5=Ceiling(Z=max)
        float u=0.0f, v=0.0f, a=0.0f, a_min=0.0f, a_max=0.0f, face_dist=0.0f;
        switch(face)
        {
            case 0: // Left wall
                u = y; v = z; a = z; a_min = grid.min_z; a_max = grid.max_z; face_dist = fabsf(x - grid.min_x); break;
            case 1: // Right wall
                u = y; v = z; a = z; a_min = grid.min_z; a_max = grid.max_z; face_dist = fabsf(x - grid.max_x); break;
            case 2: // Front wall
                u = x; v = z; a = z; a_min = grid.min_z; a_max = grid.max_z; face_dist = fabsf(y - grid.min_y); break;
            case 3: // Back wall
                u = x; v = z; a = z; a_min = grid.min_z; a_max = grid.max_z; face_dist = fabsf(y - grid.max_y); break;
            case 4: // Floor
                u = x; v = y; a = z; a_min = grid.min_z; a_max = grid.max_z; face_dist = fabsf(z - grid.min_z); break;
            case 5: // Ceiling
            default:
                u = x; v = y; a = z; a_min = grid.min_z; a_max = grid.max_z; face_dist = fabsf(z - grid.max_z); break;
        }

        int col_u = (int)floorf(u / col_spacing);
        int col_v = (int)floorf(v / col_spacing);
        int col_id = col_u * 73856093 ^ col_v * 19349663;

        float offset = ((col_id & 255) / 255.0f) * 10.0f;
        float head = (a_max - 0.5f) - fmodf(time * (0.5f + speed * 0.3f) + offset, (a_max - a_min) + 5.0f);

        float d = fabsf(a - head);
        float trail_len = 1.0f + (trail * 0.05f) * size_m; // ~1..6
        float trail_intensity = fmax(0.0f, 1.0f - d / trail_len);

        float gap = fmodf((float)((col_id >> 8) & 1023), 10.0f) / 10.0f;
        float gap_factor = 0.7f + 0.3f * (gap > 0.2f ? 1.0f : gap * 5.0f);

        // Fade from the surface into the room (keeps effect on the face)
        float face_falloff = expf(-face_dist * 3.0f);

        float intensity = trail_intensity * gap_factor * face_falloff;
        return fmax(0.0f, fmin(1.0f, intensity));
    };

    // Determine which faces to include based on coverage selection
    // effect_coverage indices: 0=Effect Default,1=Entire Room,2=Floor,3=Ceiling,4=Left,5=Right,6=Front,7=Back,8=Floor&Ceil,9=Left&Right,10=Front&Back,11=Origin
    int cov = (int)effect_coverage;
    float intensity = 0.0f;
    auto include_all = [&](){ for(int f=0; f<6; ++f) intensity = fmax(intensity, face_intensity(f)); };

    switch(cov)
    {
        case 0: // Effect Default -> treat as Entire Room for Matrix vibe
        case 1: include_all(); break;
        case 2: intensity = face_intensity(4); break; // Floor
        case 3: intensity = face_intensity(5); break; // Ceiling
        case 4: intensity = face_intensity(0); break; // Left
        case 5: intensity = face_intensity(1); break; // Right
        case 6: intensity = face_intensity(2); break; // Front
        case 7: intensity = face_intensity(3); break; // Back
        case 8: intensity = fmax(face_intensity(4), face_intensity(5)); break; // Floor & Ceiling
        case 9: intensity = fmax(face_intensity(0), face_intensity(1)); break; // Left & Right
        case 10: intensity = fmax(face_intensity(2), face_intensity(3)); break; // Front & Back
        case 11: include_all(); break; // Origin center -> render all faces
        default: include_all(); break;
    }

    // Matrix-green color
    unsigned char r = 0;
    unsigned char g = (unsigned char)(255 * intensity * (GetBrightness() / 100.0f));
    unsigned char b = 0;
    return (b << 16) | (g << 8) | r;
}
