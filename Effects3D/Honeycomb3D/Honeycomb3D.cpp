// SPDX-License-Identifier: GPL-2.0-only

#include "Honeycomb3D.h"

#include <algorithm>
#include <cmath>

REGISTER_EFFECT_3D(Honeycomb3D);

namespace
{
constexpr float kTwoPi = 6.28318530717958647692f;

inline float Phase01(float time_sec, float cycle_seconds, float speed_mul)
{
    if(cycle_seconds < 1e-4f)
        return 0.f;
    return std::fmod((time_sec * speed_mul) / cycle_seconds + 1000.f, 1.f);
}

inline float Wave01(float x01)
{
    return 0.5f + 0.5f * std::sin(kTwoPi * x01);
}

inline float Triangle01(float x01)
{
    const float f = x01 - std::floor(x01);
    return 1.0f - std::fabs(2.0f * f - 1.0f);
}
} // namespace

RGBColor Honeycomb3D::Hsv01ToBgr(float h, float s, float v)
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

Honeycomb3D::Honeycomb3D(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(true);
    SetSpeed(35);
    SetFrequency(12);
}

Honeycomb3D::~Honeycomb3D() = default;

EffectInfo3D Honeycomb3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 1;
    info.effect_name = "Hex Lattice 3D";
    info.effect_description =
        "Hex lattice 3D field. Blends sin/cos in XYZ with animated zoom and triangular hue shaping.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_HONEYCOMB_3D;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = false;
    info.needs_3d_origin = false;
    info.needs_frequency = true;
    info.default_speed_scale = 35.0f;
    info.default_frequency_scale = 12.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = true;
    return info;
}

void Honeycomb3D::SetupCustomUI(QWidget* parent)
{
    Q_UNUSED(parent);
}

void Honeycomb3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_HONEYCOMB_3D;
}

RGBColor Honeycomb3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
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

    const float spd = std::max(0.08f, GetScaledSpeed() / 100.0f);
    const float rate = std::max(0.08f, GetScaledFrequency() / 50.0f);
    const float motion = spd * rate;

    const float t1 = Wave01(Phase01(time, 1.333333f, motion)) * kTwoPi; // time(.75)
    const float t2 = Wave01(Phase01(time, 1.0526316f, motion)) * kTwoPi; // time(.95)
    const float zoom = 2.0f + Wave01(Phase01(time, 2.0f, motion)) * 5.0f; // time(.5)
    const float t3 = Wave01(Phase01(time, 1.5384615f, motion)); // time(.65)
    const float t4 = Phase01(time, 20.0f, motion); // time(.05)

    const float size_mul = std::max(0.2f, GetNormalizedSize());
    float h = (1.0f + std::sin(nx * zoom * size_mul + t1) + std::cos(ny * zoom * size_mul + t2) +
               std::cos(nz * zoom * size_mul + t2 + 0.3f)) * 0.5f;
    float v = Wave01(h + t4);
    v = v * v * v;
    h = Triangle01(h) * 0.5f + t3;

    if(GetRainbowMode())
        return Hsv01ToBgr(h, 1.0f, v);

    RGBColor c = GetColorAtPosition(h - std::floor(h));
    int r = (int)((float)(c & 0xFF) * v);
    int g = (int)((float)((c >> 8) & 0xFF) * v);
    int b = (int)((float)((c >> 16) & 0xFF) * v);
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json Honeycomb3D::SaveSettings() const
{
    return SpatialEffect3D::SaveSettings();
}

void Honeycomb3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
}
