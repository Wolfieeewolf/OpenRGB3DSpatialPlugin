// SPDX-License-Identifier: GPL-2.0-only

#include "AudioLevel3D.h"
#include "Colors.h"
#include <cmath>
#include <algorithm>

float AudioLevel3D::EvaluateIntensity(float amplitude, float time)
{
    float alpha = std::clamp(audio_settings.smoothing, 0.0f, 0.99f);
    if(std::fabs(time - last_intensity_time) > 1e-4f)
    {
        smoothed = alpha * smoothed + (1.0f - alpha) * amplitude;
        last_intensity_time = time;
    }
    else if(alpha <= 0.0f)
    {
        smoothed = amplitude;
    }
    return ApplyAudioIntensity(smoothed, audio_settings);
}

AudioLevel3D::AudioLevel3D(QWidget* parent)
    : SpatialEffect3D(parent)
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

RGBColor AudioLevel3D::CalculateColor(float x, float y, float z, float time)
{
    AudioInputManager* audio = AudioInputManager::instance();
    float amplitude = audio->getBandEnergyHz((float)audio_settings.low_hz, (float)audio_settings.high_hz);
    float intensity = EvaluateIntensity(amplitude, time);

    // Apply rotation transformation
    Vector3D origin = GetEffectOrigin();
    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    
    float radial_norm = std::clamp(std::sqrt(rotated_pos.x * rotated_pos.x + rotated_pos.y * rotated_pos.y + rotated_pos.z * rotated_pos.z) / 0.75f, 0.0f, 1.0f);

    // Use rotated X coordinate for axis position
    float axis_pos = std::clamp(0.5f + rotated_pos.x, 0.0f, 1.0f);

    float gradient_pos = std::clamp(0.65f * axis_pos + 0.35f * (1.0f - radial_norm), 0.0f, 1.0f);
    float spatial = 0.55f + 0.45f * (1.0f - radial_norm);
    intensity = std::clamp(intensity * spatial, 0.0f, 1.0f);

    RGBColor color = ComposeAudioGradientColor(audio_settings, gradient_pos, intensity);
    // Global brightness is applied by PostProcessColorGrid
    color = ScaleRGBColor(color, (0.35f + 0.65f * intensity));

    RGBColor user_color = GetRainbowMode()
        ? GetRainbowColor(CalculateProgress(time) * 360.0f)
        : GetColorAtPosition(0.0f);
    color = ModulateRGBColors(color, user_color);
    return color;
}

RGBColor AudioLevel3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    AudioInputManager* audio = AudioInputManager::instance();
    float amplitude = audio->getBandEnergyHz((float)audio_settings.low_hz, (float)audio_settings.high_hz);
    float intensity = EvaluateIntensity(amplitude, time);

    // Apply rotation transformation before calculating positions
    Vector3D origin = GetEffectOriginGrid(grid);
    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    
    float dx = rotated_pos.x - grid.center_x;
    float dy = rotated_pos.y - grid.center_y;
    float dz = rotated_pos.z - grid.center_z;
    float max_radius = 0.5f * std::max({grid.width, grid.height, grid.depth});
    float radial_norm = ComputeRadialNormalized(dx, dy, dz, max_radius);

    // Use rotated X coordinate for axis position
    float axis_pos = NormalizeRange(rotated_pos.x, grid.min_x, grid.max_x);

    float gradient_pos = std::clamp(0.65f * axis_pos + 0.35f * (1.0f - radial_norm), 0.0f, 1.0f);
    float spatial = 0.55f + 0.45f * (1.0f - radial_norm);
    intensity = std::clamp(intensity * spatial, 0.0f, 1.0f);

    RGBColor color = ComposeAudioGradientColor(audio_settings, gradient_pos, intensity);
    // Global brightness is applied by PostProcessColorGrid
    color = ScaleRGBColor(color, (0.35f + 0.65f * intensity));

    RGBColor user_color = GetRainbowMode()
        ? GetRainbowColor(CalculateProgress(time) * 360.0f)
        : GetColorAtPosition(0.0f);
    color = ModulateRGBColors(color, user_color);
    return color;
}

// Register effect
REGISTER_EFFECT_3D(AudioLevel3D)

nlohmann::json AudioLevel3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    return j;
}

void AudioLevel3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    smoothed = 0.0f;
    last_intensity_time = std::numeric_limits<float>::lowest();
}
