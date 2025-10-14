/*---------------------------------------------------------*\
| BandScan3D.cpp                                            |
|                                                           |
|   Scans through spectrum bands across space               |
|                                                           |
|   Date: 2025-10-14                                        |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "BandScan3D.h"
#include <algorithm>
#include <cmath>

BandScan3D::BandScan3D(QWidget* parent)
    : SpatialEffect3D(parent)
{
}

BandScan3D::~BandScan3D() {}

EffectInfo3D BandScan3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Band Scan";
    info.effect_description = "Moves a band across axis with level";
    info.category = "Audio";
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 2;
    info.has_custom_settings = false;
    info.needs_3d_origin = false;
    info.default_speed_scale = 10.0f;
    info.default_frequency_scale = 1.0f;
    info.use_size_parameter = false;
    info.show_speed_control = true;  // scan speed
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = false;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_axis_control = true;
    info.show_color_controls = true;
    return info;
}

void BandScan3D::SetupCustomUI(QWidget* /*parent*/)
{
    // No extra per-effect UI; use standard Audio Controls panel
}

void BandScan3D::UpdateParams(SpatialEffectParams& /*params*/)
{
}

RGBColor BandScan3D::CalculateColor(float x, float y, float z, float time)
{
    // Determine position along axis
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
    float t = std::fmod(std::fabs(coord), 1.0f);
    // Scan band by time
    float progress = CalculateProgress(time);
    std::vector<float> spec = AudioInputManager::instance()->getBands();
    int n = (int)spec.size();
    int start = std::clamp(band_start, 0, n>0?n-1:0);
    int end = (band_end < 0) ? (n-1) : std::clamp(band_end, start, n-1);
    int bands = std::max(1, end - start + 1);
    int current_local = (int)std::floor(std::fmod(std::fabs(progress), 1.0f) * bands);
    int idx_local = std::clamp((int)std::floor(t * bands), 0, bands-1);
    int current = start + current_local;

    float lvl_raw = (current >= 0 && current < n) ? spec[current] : 0.0f;
    smoothed = smoothing * smoothed + (1.0f - smoothing) * lvl_raw;
    float lvl = smoothed;
    float dist = std::fabs((float)idx_local - (float)current_local);
    float local_fall = std::exp(-dist * 1.2f);
    float bright = GetBrightness() / 100.0f;
    float base = std::clamp(lvl * local_fall * bright, 0.0f, 1.0f);
    float factor = std::pow(base, std::max(0.2f, std::min(5.0f, falloff)));

    RGBColor c0 = GetColorAtPosition(0.0f);
    RGBColor c1 = GetColorAtPosition(1.0f);
    float u = (float)current_local / (float)(bands - 1);
    int r0 = c0 & 0xFF, g0 = (c0>>8)&0xFF, b0 = (c0>>16)&0xFF;
    int r1 = c1 & 0xFF, g1 = (c1>>8)&0xFF, b1 = (c1>>16)&0xFF;
    int r = (int)(r0 + (r1 - r0) * u);
    int g = (int)(g0 + (g1 - g0) * u);
    int b = (int)(b0 + (b1 - b0) * u);
    r = std::min(255, (int)(r * factor));
    g = std::min(255, (int)(g * factor));
    b = std::min(255, (int)(b * factor));
    return (b << 16) | (g << 8) | r;
}

nlohmann::json BandScan3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["band_start"] = band_start;
    j["band_end"] = band_end;
    j["smoothing"] = smoothing;
    j["falloff"] = falloff;
    return j;
}

void BandScan3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("band_start")) band_start = settings["band_start"].get<int>();
    if(settings.contains("band_end")) band_end = settings["band_end"].get<int>();
    if(settings.contains("smoothing")) smoothing = std::clamp(settings["smoothing"].get<float>(), 0.0f, 0.99f);
    if(settings.contains("falloff")) falloff = std::max(0.2f, std::min(5.0f, settings["falloff"].get<float>()));
}

REGISTER_EFFECT_3D(BandScan3D)
