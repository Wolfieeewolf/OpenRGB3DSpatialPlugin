// SPDX-License-Identifier: GPL-2.0-only

#include "FastPulse3D.h"

#include <algorithm>
#include <cmath>

REGISTER_EFFECT_3D(FastPulse3D);

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

FastPulse3D::FastPulse3D(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(true);
    SetSpeed(55);
    SetFrequency(20);
}

FastPulse3D::~FastPulse3D() = default;

EffectInfo3D FastPulse3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 1;
    info.effect_name = "Rapid Pulse 3D";
    info.effect_description =
        "Fast high-contrast 3D pulse field from moving XYZ wave sums.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_FAST_PULSE_3D;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = false;
    info.needs_3d_origin = false;
    info.needs_frequency = true;
    info.default_speed_scale = 55.0f;
    info.default_frequency_scale = 20.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = true;
    return info;
}

void FastPulse3D::SetupCustomUI(QWidget* parent)
{
    Q_UNUSED(parent);
}

void FastPulse3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_FAST_PULSE_3D;
}

RGBColor FastPulse3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
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

    const float t1 = Phase01(time, 10.0f, motion); // time(.1)
    const float r1 = std::sin(kTwoPi * Phase01(time, 10.0f, motion));
    const float r2 = std::sin(kTwoPi * Phase01(time, 20.0f, motion));
    const float r3 = std::sin(kTwoPi * Phase01(time, 14.285714f, motion));

    const float size_mul = std::max(0.2f, GetNormalizedSize());
    float v = Triangle01(std::fmod(3.0f * Wave01(t1) + (nx * r1 + ny * r2 + nz * r3) * size_mul + 10.0f, 1.0f));
    v = v * v * v * v * v;
    const float s = (v < 0.8f) ? 1.0f : 0.0f;

    if(GetRainbowMode())
        return Hsv01ToBgr(t1, s, v);

    RGBColor c = GetColorAtPosition(t1);
    int r = (int)((float)(c & 0xFF) * v);
    int g = (int)((float)((c >> 8) & 0xFF) * v);
    int b = (int)((float)((c >> 16) & 0xFF) * v);
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json FastPulse3D::SaveSettings() const
{
    return SpatialEffect3D::SaveSettings();
}

void FastPulse3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
}
