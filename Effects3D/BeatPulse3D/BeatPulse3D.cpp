/*---------------------------------------------------------*\
| BeatPulse3D.cpp                                           |
|                                                           |
|   Bass-driven global pulse                                |
|                                                           |
|   Date: 2025-10-14                                        |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "BeatPulse3D.h"
#include <algorithm>
#include <cmath>

BeatPulse3D::BeatPulse3D(QWidget* parent)
    : SpatialEffect3D(parent)
{
}

BeatPulse3D::~BeatPulse3D() {}

EffectInfo3D BeatPulse3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Beat Pulse";
    info.effect_description = "Global brightness pulses with bass";
    info.category = "Audio";
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 1;
    info.has_custom_settings = false;
    info.needs_3d_origin = false;
    info.default_speed_scale = 10.0f;
    info.default_frequency_scale = 1.0f;
    info.use_size_parameter = false;
    info.show_speed_control = false;
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = false;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_axis_control = false;
    info.show_color_controls = true;
    return info;
}

void BeatPulse3D::SetupCustomUI(QWidget* parent)
{
    /*---------------------------------------------------------*\
    | Beat Pulse has no effect-specific controls               |
    | All audio controls (Hz, Smoothing, Falloff) are handled |
    | by the standard Audio Controls panel in the Audio tab   |
    \*---------------------------------------------------------*/
    (void)parent;
    // No custom UI needed - all controls are standard
}

void BeatPulse3D::UpdateParams(SpatialEffectParams& /*params*/)
{
}

RGBColor BeatPulse3D::CalculateColor(float /*x*/, float /*y*/, float /*z*/, float /*time*/)
{
    // Use band-filtered energy to drive pulse
    float drive = AudioInputManager::instance()->getBandEnergyHz((float)low_hz, (float)high_hz);
    // Attack fast, decay controlled by smoothing (0..0.99)
    float decay = 0.5f + std::clamp(smoothing, 0.0f, 0.99f) * 0.49f; // 0.5..0.99
    envelope = std::max(envelope * decay, drive);
    float bright = GetBrightness() / 100.0f;
    float scaled = std::clamp(envelope * bright, 0.0f, 1.0f);
    float factor = std::pow(scaled, std::max(0.2f, std::min(5.0f, falloff)));

    RGBColor baseColor = GetColorAtPosition(0.0f);
    int r = (int)((baseColor & 0xFF) * factor);
    int g = (int)(((baseColor >> 8) & 0xFF) * factor);
    int b = (int)(((baseColor >> 16) & 0xFF) * factor);
    if(r>255) r=255; if(g>255) g=255; if(b>255) b=255;
    return (b << 16) | (g << 8) | r;
}

RGBColor BeatPulse3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    /*---------------------------------------------------------*\
    | Audio effects are typically global (not spatially aware) |
    | Simply delegate to CalculateColor                        |
    \*---------------------------------------------------------*/
    (void)grid;  // Unused parameter
    return CalculateColor(x, y, z, time);
}

REGISTER_EFFECT_3D(BeatPulse3D)

nlohmann::json BeatPulse3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["low_hz"] = low_hz;
    j["high_hz"] = high_hz;
    j["smoothing"] = smoothing;
    j["falloff"] = falloff;
    return j;
}

void BeatPulse3D::LoadSettings(const nlohmann::json& settings)
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
