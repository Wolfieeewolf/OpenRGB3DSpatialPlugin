/*---------------------------------------------------------*\
| SpectrumBars3D.cpp                                        |
|                                                           |
|   Audio spectrum -> bars mapped across axis               |
|                                                           |
|   Date: 2025-10-14                                        |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "SpectrumBars3D.h"
#include <algorithm>
#include <cmath>

SpectrumBars3D::SpectrumBars3D(QWidget* parent)
    : SpatialEffect3D(parent)
{
}

SpectrumBars3D::~SpectrumBars3D() {}

EffectInfo3D SpectrumBars3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Spectrum Bars";
    info.effect_description = "Maps 16 audio bands across the axis";
    info.category = "Audio";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 2;
    info.has_custom_settings = false;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = false;
    info.default_speed_scale = 10.0f;
    info.default_frequency_scale = 1.0f;
    info.use_size_parameter = false;
    info.show_speed_control = false;
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = false;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_axis_control = true;
    info.show_color_controls = true;
    return info;
}

void SpectrumBars3D::SetupCustomUI(QWidget* parent)
{
    /*---------------------------------------------------------*\
    | Spectrum Bars has no effect-specific controls            |
    | All audio controls (Hz, Smoothing, Falloff) are handled |
    | by the standard Audio Controls panel in the Audio tab   |
    \*---------------------------------------------------------*/
    (void)parent;
    // No custom UI needed - all controls are standard
}

void SpectrumBars3D::UpdateParams(SpatialEffectParams& /*params*/)
{
}

RGBColor SpectrumBars3D::CalculateColor(float x, float y, float z, float /*time*/)
{
    // Axis mapping
    float coord = 0.0f;
    switch(GetAxis())
    {
        case AXIS_X: coord = x; break;
        case AXIS_Y: coord = y; break;
        case AXIS_Z: coord = z; break;
        case AXIS_RADIAL:
        default:
            coord = std::sqrt(x*x + y*y + z*z);
            break;
    }

    // Map coord to 0..1 using simple range assumption; in grid-aware path, PostProcess handles coverage
    float t = std::fmod(std::fabs(coord), 1.0f);
    std::vector<float> spec = AudioInputManager::instance()->getBands();
    int n = (int)spec.size();
    int start = std::clamp(band_start, 0, n>0?n-1:0);
    int end = (band_end < 0) ? (n-1) : std::clamp(band_end, start, n-1);
    int bands = std::max(1, end - start + 1);
    int idx_local = std::clamp((int)std::floor(t * bands), 0, bands-1);
    int idx = start + idx_local;
    float lvl = (idx >= 0 && idx < n) ? spec[idx] : 0.0f;

    // Color from gradient: start->end
    RGBColor c0 = GetColorAtPosition(0.0f);
    RGBColor c1 = GetColorAtPosition(1.0f);
    float u = (float)idx_local / (float)(bands - 1);
    int r0 = c0 & 0xFF, g0 = (c0>>8)&0xFF, b0 = (c0>>16)&0xFF;
    int r1 = c1 & 0xFF, g1 = (c1>>8)&0xFF, b1 = (c1>>16)&0xFF;
    int r = (int)(r0 + (r1 - r0) * u);
    int g = (int)(g0 + (g1 - g0) * u);
    int b = (int)(b0 + (b1 - b0) * u);

    // Apply per-effect smoothing to level
    smoothed = smoothing * smoothed + (1.0f - smoothing) * lvl;
    float bright = GetBrightness() / 100.0f;
    float base = std::clamp(smoothed * bright, 0.0f, 1.0f);
    float factor = std::pow(base, std::max(0.2f, std::min(5.0f, falloff)));
    r = std::min(255, (int)(r * factor));
    g = std::min(255, (int)(g * factor));
    b = std::min(255, (int)(b * factor));
    return (b << 16) | (g << 8) | r;
}

RGBColor SpectrumBars3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    /*---------------------------------------------------------*\
    | Audio effects are typically global (not spatially aware) |
    | Simply delegate to CalculateColor                        |
    \*---------------------------------------------------------*/
    (void)grid;  // Unused parameter
    return CalculateColor(x, y, z, time);
}

nlohmann::json SpectrumBars3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["low_hz"] = low_hz;
    j["high_hz"] = high_hz;
    j["smoothing"] = smoothing;
    j["falloff"] = falloff;
    return j;
}

void SpectrumBars3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("low_hz"))
    {
        low_hz = settings["low_hz"].get<int>();
    }
    if(settings.contains("high_hz"))
    {
        high_hz = settings["high_hz"].get<int>();
    }
    if(settings.contains("smoothing"))
    {
        smoothing = std::clamp(settings["smoothing"].get<float>(), 0.0f, 0.99f);
    }
    if(settings.contains("falloff"))
    {
        falloff = std::max(0.2f, std::min(5.0f, settings["falloff"].get<float>()));
    }
}

REGISTER_EFFECT_3D(SpectrumBars3D)
