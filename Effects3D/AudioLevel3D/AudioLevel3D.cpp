/*---------------------------------------------------------*\
| AudioLevel3D.cpp                                          |
|                                                           |
|   Basic audio-reactive effect                             |
|                                                           |
|   Date: 2025-10-14                                        |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "AudioLevel3D.h"
#include "Colors.h"
#include <cmath>

AudioLevel3D::AudioLevel3D(QWidget* parent)
    : SpatialEffect3D(parent)
{
}

AudioLevel3D::~AudioLevel3D()
{
}

EffectInfo3D AudioLevel3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Audio Level";
    info.effect_description = "Scales brightness by audio RMS level";
    info.category = "Audio";
    info.effect_type = (SpatialEffectType)0; // Not used in new system
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200; // speed can be used for hue cycle if rainbow enabled
    info.min_speed = 0;
    info.user_colors = 1;
    info.has_custom_settings = false;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = false;

    info.default_speed_scale = 10.0f;
    info.default_frequency_scale = 1.0f;
    info.use_size_parameter = false;

    // Hide unused base controls
    info.show_speed_control = true;         // allow hue cycle when rainbow
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = false;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_axis_control = false;
    info.show_color_controls = true;
    return info;
}

void AudioLevel3D::SetupCustomUI(QWidget* parent)
{
    /*---------------------------------------------------------*\
    | Audio Level has no effect-specific controls              |
    | All audio controls (Hz, Smoothing, Falloff) are handled |
    | by the standard Audio Controls panel in the Audio tab   |
    \*---------------------------------------------------------*/
    (void)parent;
    // No custom UI needed - all controls are standard
}

void AudioLevel3D::UpdateParams(SpatialEffectParams& /*params*/)
{
    // No special param mapping required
}

RGBColor AudioLevel3D::CalculateColor(float /*x*/, float /*y*/, float /*z*/, float time)
{
    // Get filtered band energy 0..1 and apply per-effect smoothing
    float lvl_raw = AudioInputManager::instance()->getBandEnergyHz((float)low_hz, (float)high_hz);
    smoothed = smoothing * smoothed + (1.0f - smoothing) * lvl_raw;
    float lvl = smoothed;

    // If rainbow mode, cycle hue by speed; else use first color
    RGBColor baseColor;
    if(GetRainbowMode())
    {
        float hue = std::fmod(CalculateProgress(time) * 60.0f, 360.0f);
        baseColor = GetRainbowColor(hue);
    }
    else
    {
        baseColor = GetColorAtPosition(0.0f);
    }

    // Multiply by brightness control (0..100)
    float bright = GetBrightness() / 100.0f; // using inherited getter

    // Apply audio level
    float scaled = std::max(0.0f, std::min(1.0f, lvl)) * std::max(0.0f, std::min(1.0f, bright));
    float factor = std::pow(scaled, std::max(0.2f, std::min(5.0f, falloff)));

    unsigned char r = baseColor & 0xFF;
    unsigned char g = (baseColor >> 8) & 0xFF;
    unsigned char b = (baseColor >> 16) & 0xFF;

    int rr = (int)(r * factor);
    int gg = (int)(g * factor);
    int bb = (int)(b * factor);
    if(rr > 255) rr = 255; if(gg > 255) gg = 255; if(bb > 255) bb = 255;
    return (bb << 16) | (gg << 8) | rr;
}

RGBColor AudioLevel3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    /*---------------------------------------------------------*\
    | Audio effects are typically global (not spatially aware) |
    | but we still need grid-aware implementation for          |
    | consistency with the rendering pipeline                  |
    \*---------------------------------------------------------*/
    (void)grid;  // Unused parameter

    // Audio effects don't use spatial coordinates, so we can
    // simply call the base CalculateColor implementation
    // (coordinates are ignored in audio effects anyway)
    return CalculateColor(x, y, z, time);
}

// Register effect
REGISTER_EFFECT_3D(AudioLevel3D)

nlohmann::json AudioLevel3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["low_hz"] = low_hz;
    j["high_hz"] = high_hz;
    j["smoothing"] = smoothing;
    j["falloff"] = falloff;
    return j;
}

void AudioLevel3D::LoadSettings(const nlohmann::json& settings)
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
