// SPDX-License-Identifier: GPL-2.0-only
// SPDX-License-Identifier: GPL-2.0-only

#include "BeatPulse3D.h"
#include <algorithm>
#include <cmath>

float BeatPulse3D::EvaluateIntensity(float amplitude, float time)
{
    amplitude = std::clamp(amplitude, 0.0f, 1.0f);
    float alpha = std::clamp(audio_settings.smoothing, 0.0f, 0.99f);
    if(std::fabs(time - last_intensity_time) > 1e-4f)
    {
        smoothed = alpha * smoothed + (1.0f - alpha) * amplitude;
        last_intensity_time = time;
        float decay = 0.65f + alpha * 0.25f;
        envelope = std::max(envelope * decay, smoothed);
    }
    else if(alpha <= 0.0f)
    {
        smoothed = amplitude;
        envelope = amplitude;
    }
    return ApplyAudioIntensity(envelope, audio_settings);
}

BeatPulse3D::BeatPulse3D(QWidget* parent)
    : SpatialEffect3D(parent)
{
}

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

RGBColor BeatPulse3D::CalculateColor(float x, float y, float z, float time)
{
    AudioInputManager* audio = AudioInputManager::instance();
    float amplitude = audio->getBandEnergyHz((float)audio_settings.low_hz, (float)audio_settings.high_hz);
    float intensity = EvaluateIntensity(amplitude, time);

    float phase = CalculateProgress(time);
    float wave_front = std::fmod(phase, 1.0f);
    if(wave_front < 0.0f)
    {
        wave_front += 1.0f;
    }
    if(GetReverse())
    {
        wave_front = 1.0f - wave_front;
    }

    float radial_norm = std::clamp(std::sqrt(x * x + y * y + z * z) / 0.75f, 0.0f, 1.0f);
    float height_norm = std::clamp(0.5f + y, 0.0f, 1.0f);
    float distance = std::fabs(radial_norm - wave_front);
    float pulse = std::exp(-distance * distance * 36.0f);
    float tail = std::exp(-std::max(distance - 0.2f, 0.0f) * 6.0f);

    float energy = std::clamp(intensity * (0.55f + 0.45f * (1.0f - height_norm)) * (0.7f * pulse + 0.3f * tail), 0.0f, 1.0f);

    float gradient_pos = std::clamp(radial_norm, 0.0f, 1.0f);
    RGBColor color = ComposeAudioGradientColor(audio_settings, gradient_pos, energy);
    float brightness = std::clamp((float)GetBrightness() / 100.0f, 0.0f, 1.0f);
    color = ScaleRGBColor(color, brightness * (0.25f + 0.75f * energy));

    RGBColor user_color = GetRainbowMode()
        ? GetRainbowColor(wave_front * 360.0f)
        : GetColorAtPosition(0.0f);
    return ModulateRGBColors(color, user_color);
}

RGBColor BeatPulse3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    AudioInputManager* audio = AudioInputManager::instance();
    float amplitude = audio->getBandEnergyHz((float)audio_settings.low_hz, (float)audio_settings.high_hz);
    float intensity = EvaluateIntensity(amplitude, time);

    float phase = CalculateProgress(time);
    float wave_front = std::fmod(phase, 1.0f);
    if(wave_front < 0.0f)
    {
        wave_front += 1.0f;
    }
    if(GetReverse())
    {
        wave_front = 1.0f - wave_front;
    }

    float dx = x - grid.center_x;
    float dy = y - grid.center_y;
    float dz = z - grid.center_z;
    float max_radius = 0.5f * std::max({grid.width, grid.height, grid.depth});
    float radial_norm = ComputeRadialNormalized(dx, dy, dz, max_radius);
    float height_norm = NormalizeRange(y, grid.min_y, grid.max_y);
    float distance = std::fabs(radial_norm - wave_front);
    float pulse = std::exp(-distance * distance * 36.0f);
    float tail = std::exp(-std::max(distance - 0.2f, 0.0f) * 6.0f);

    float energy = std::clamp(intensity * (0.55f + 0.45f * (1.0f - height_norm)) * (0.7f * pulse + 0.3f * tail), 0.0f, 1.0f);

    float gradient_pos = std::clamp(radial_norm, 0.0f, 1.0f);
    RGBColor color = ComposeAudioGradientColor(audio_settings, gradient_pos, energy);
    float brightness = std::clamp((float)GetBrightness() / 100.0f, 0.0f, 1.0f);
    color = ScaleRGBColor(color, brightness * (0.25f + 0.75f * energy));

    RGBColor user_color = GetRainbowMode()
        ? GetRainbowColor(wave_front * 360.0f)
        : GetColorAtPosition(0.0f);
    return ModulateRGBColors(color, user_color);
}

REGISTER_EFFECT_3D(BeatPulse3D)

nlohmann::json BeatPulse3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    return j;
}

void BeatPulse3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    envelope = 0.0f;
    smoothed = 0.0f;
    last_intensity_time = std::numeric_limits<float>::lowest();
}
