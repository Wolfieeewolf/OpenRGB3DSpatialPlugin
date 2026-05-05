// SPDX-License-Identifier: GPL-2.0-only

#include "Complements3D.h"

#include <algorithm>
#include <cmath>

REGISTER_EFFECT_3D(Complements3D);

namespace
{
inline float Phase01(float time_sec, float cycle_seconds, float speed_mul)
{
    if(cycle_seconds < 1e-4f)
        return 0.f;
    return std::fmod((time_sec * speed_mul) / cycle_seconds + 1000.f, 1.f);
}

inline RGBColor Hsv01ToBgr(float h, float s, float v)
{
    h = std::fmod(h, 1.0f);
    if(h < 0.0f)
        h += 1.0f;
    s = std::clamp(s, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    const float hf = h * 6.0f;
    const int i = (int)std::floor(hf) % 6;
    const float f = hf - std::floor(hf);
    const float p = v * (1.0f - s);
    const float q = v * (1.0f - f * s);
    const float t = v * (1.0f - (1.0f - f) * s);
    switch(i)
    {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }

    const int ri = std::clamp((int)std::lround(r * 255.0f), 0, 255);
    const int gi = std::clamp((int)std::lround(g * 255.0f), 0, 255);
    const int bi = std::clamp((int)std::lround(b * 255.0f), 0, 255);
    return (RGBColor)((bi << 16) | (gi << 8) | ri);
}
} // namespace

Complements3D::Complements3D(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(true);
    SetSpeed(45);
}

Complements3D::~Complements3D() = default;

EffectInfo3D Complements3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 1;
    info.effect_name = "Dual Tone Depth 3D";
    info.effect_description =
        "Blends two rotating complementary hues along room Z with center dimming for depth.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_COMPLEMENTS_3D;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = false;
    info.needs_3d_origin = false;
    info.needs_frequency = false;
    info.default_speed_scale = 45.0f;
    info.use_size_parameter = false;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = false;
    info.show_scale_control = true;
    info.show_color_controls = true;
    return info;
}

void Complements3D::SetupCustomUI(QWidget* parent)
{
    Q_UNUSED(parent);
}

void Complements3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_COMPLEMENTS_3D;
}

RGBColor Complements3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    float nz = NormalizeGridAxis01(rot.z, grid.min_z, grid.max_z);

    const float spd = std::max(0.08f, GetScaledSpeed() / 100.0f);
    const float pos = Phase01(time, 10.0f, spd); // speed=10000ms in source epe

    const float color_percent = 0.5f; // complementary split
    const float percent_dim = 0.7f;
    const float h1 = pos;
    const float h2 = pos + color_percent;
    const float hue = std::fmod(h1 + (h2 - h1) * nz + 1.0f, 1.0f);

    float v = 1.0f;
    if(nz < 0.5f)
        v *= (1.0f - (nz * 2.0f * percent_dim));
    else
        v *= ((1.0f - percent_dim) + ((nz - 0.5f) * (nz * 2.0f * percent_dim)));
    v = std::clamp(v, 0.0f, 1.0f);

    if(GetRainbowMode())
        return Hsv01ToBgr(hue, 1.0f, v);

    RGBColor c = GetColorAtPosition(hue);
    int r = (int)((float)(c & 0xFF) * v);
    int g = (int)((float)((c >> 8) & 0xFF) * v);
    int b = (int)((float)((c >> 16) & 0xFF) * v);
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json Complements3D::SaveSettings() const
{
    return SpatialEffect3D::SaveSettings();
}

void Complements3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
}
