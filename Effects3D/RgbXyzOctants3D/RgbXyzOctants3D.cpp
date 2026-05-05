// SPDX-License-Identifier: GPL-2.0-only

#include "RgbXyzOctants3D.h"

#include <algorithm>
#include <cmath>

REGISTER_EFFECT_3D(RgbXyzOctants3D);

namespace
{
inline RGBColor Rgb01(float r, float g, float b)
{
    const int ri = std::clamp((int)std::lround(r * 255.0f), 0, 255);
    const int gi = std::clamp((int)std::lround(g * 255.0f), 0, 255);
    const int bi = std::clamp((int)std::lround(b * 255.0f), 0, 255);
    return (RGBColor)((bi << 16) | (gi << 8) | ri);
}
} // namespace

RgbXyzOctants3D::RgbXyzOctants3D(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(false);
    SetSpeed(50);
}

RgbXyzOctants3D::~RgbXyzOctants3D() = default;

EffectInfo3D RgbXyzOctants3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 1;
    info.effect_name = "Axis Octants 3D";
    info.effect_description =
        "Static orientation test: each XYZ octant gets a unique RGB combination from axis high/low sides.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_RGB_XYZ_OCTANTS_3D;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = false;
    info.needs_3d_origin = false;
    info.needs_frequency = false;
    info.default_speed_scale = 50.0f;
    info.use_size_parameter = false;
    info.show_speed_control = false;
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = false;
    info.show_scale_control = false;
    info.show_color_controls = false;
    return info;
}

void RgbXyzOctants3D::SetupCustomUI(QWidget* parent)
{
    Q_UNUSED(parent);
}

void RgbXyzOctants3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_RGB_XYZ_OCTANTS_3D;
}

RGBColor RgbXyzOctants3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Q_UNUSED(time);

    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    const float nx = NormalizeGridAxis01(rot.x, grid.min_x, grid.max_x);
    const float ny = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
    const float nz = NormalizeGridAxis01(rot.z, grid.min_z, grid.max_z);

    const float hi = 1.0f;
    const float lo = 0.0f;
    const float r = (nx > 0.5f) ? hi : lo;
    const float g = (ny > 0.5f) ? hi : lo;
    const float b = (nz > 0.5f) ? hi : lo;
    return Rgb01(r, g, b);
}

nlohmann::json RgbXyzOctants3D::SaveSettings() const
{
    return SpatialEffect3D::SaveSettings();
}

void RgbXyzOctants3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
}
