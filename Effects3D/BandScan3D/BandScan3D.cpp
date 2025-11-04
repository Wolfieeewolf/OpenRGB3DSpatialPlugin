/*---------------------------------------------------------*\
| BandScan3D.cpp                                            |
|                                                           |
|   Scans through spectrum bands across space               |
|                                                           |
|   Date: 2025-11-03                                        |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "BandScan3D.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace
{
    inline int MapHzToBandIndex(float hz, int bands, float f_min, float f_max)
    {
        float clamped = std::clamp(hz, f_min, f_max);
        float denom = std::log(f_max / f_min);
        if(std::abs(denom) < 1e-6f)
        {
            return 0;
        }
        float t = std::log(clamped / f_min) / denom;
        int idx = static_cast<int>(std::floor(t * bands));
        return std::clamp(idx, 0, bands - 1);
    }
}

BandScan3D::BandScan3D(QWidget* parent)
    : SpatialEffect3D(parent)
{
    RefreshBandRange();
}

BandScan3D::~BandScan3D() {}

EffectInfo3D BandScan3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Band Scan";
    info.effect_description = "Moves a spectrum band highlight across the axis";
    info.category = "Audio";
    info.effect_type = (SpatialEffectType)0;
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
    info.show_speed_control = true; // scan speed
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = false;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_axis_control = true;
    info.show_color_controls = true;
    return info;
}

void BandScan3D::SetupCustomUI(QWidget* parent)
{
    Q_UNUSED(parent);
    // All controls live in the shared Audio panel.
}

void BandScan3D::UpdateParams(SpatialEffectParams& /*params*/)
{
    RefreshBandRange();
}

RGBColor BandScan3D::CalculateColor(float x, float y, float z, float time)
{
    EnsureSpectrumCache(time);
    float axis_pos = ResolveCoordinateNormalized(nullptr, x, y, z);
    float height_norm = ResolveHeightNormalized(nullptr, x, y, z);
    float radial_norm = ResolveRadialNormalized(nullptr, x, y, z);
    float brightness = std::clamp(static_cast<float>(GetBrightness()) / 100.0f, 0.0f, 1.0f);
    bool rainbow_mode = GetRainbowMode();
    RGBColor axis_color = rainbow_mode
        ? GetRainbowColor(axis_pos * 360.0f)
        : GetColorAtPosition(std::clamp(axis_pos, 0.0f, 1.0f));
    return ComposeColor(axis_pos, height_norm, radial_norm, time, brightness, axis_color, rainbow_mode);
}

RGBColor BandScan3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    EnsureSpectrumCache(time);
    float axis_pos = ResolveCoordinateNormalized(&grid, x, y, z);
    float height_norm = ResolveHeightNormalized(&grid, x, y, z);
    float radial_norm = ResolveRadialNormalized(&grid, x, y, z);
    float brightness = std::clamp(static_cast<float>(GetBrightness()) / 100.0f, 0.0f, 1.0f);
    bool rainbow_mode = GetRainbowMode();
    RGBColor axis_color = rainbow_mode
        ? GetRainbowColor(axis_pos * 360.0f)
        : GetColorAtPosition(std::clamp(axis_pos, 0.0f, 1.0f));
    return ComposeColor(axis_pos, height_norm, radial_norm, time, brightness, axis_color, rainbow_mode);
}

nlohmann::json BandScan3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    return j;
}

void BandScan3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);

    RefreshBandRange();
    last_sample_time = std::numeric_limits<float>::lowest();
}

void BandScan3D::RefreshBandRange()
{
    AudioInputManager* audio = AudioInputManager::instance();
    int total_bands = audio->getBandsCount();
    if(total_bands <= 0)
    {
        total_bands = static_cast<int>(audio->getBands().size());
        if(total_bands <= 0)
        {
            total_bands = 16;
        }
    }

    float sample_rate = static_cast<float>(audio->getSampleRate());
    if(sample_rate <= 0.0f)
    {
        sample_rate = 48000.0f;
    }
    int fft_size = audio->getFFTSize();
    if(fft_size <= 0)
    {
        fft_size = 1024;
    }

    float f_min = std::max(1.0f, sample_rate / std::max(1, fft_size));
    float f_max = sample_rate * 0.5f;
    if(f_max <= f_min)
    {
        f_max = f_min + 1.0f;
    }

    int start = MapHzToBandIndex((float)audio_settings.low_hz, total_bands, f_min, f_max);
    int end   = MapHzToBandIndex((float)audio_settings.high_hz, total_bands, f_min, f_max);
    if(end < start)
    {
        std::swap(end, start);
    }

    band_start = std::clamp(start, 0, total_bands - 1);
    band_end   = std::clamp(end, band_start, total_bands - 1);

    int count = std::max(1, band_end - band_start + 1);
    if(static_cast<int>(smoothed_bands.size()) != count)
    {
        smoothed_bands.assign(count, 0.0f);
    }
}

void BandScan3D::EnsureSpectrumCache(float time)
{
    const float epsilon = 1e-4f;
    if(last_sample_time != std::numeric_limits<float>::lowest())
    {
        if(std::fabs(time - last_sample_time) <= epsilon)
        {
            return;
        }
    }

    last_sample_time = time;
    UpdateSmoothedBands(AudioInputManager::instance()->getBands());
}

void BandScan3D::UpdateSmoothedBands(const std::vector<float>& spectrum)
{
    RefreshBandRange();
    int count = band_end - band_start + 1;
    if(count <= 0)
    {
        smoothed_bands.clear();
        return;
    }
    if(static_cast<int>(smoothed_bands.size()) != count)
    {
        smoothed_bands.assign(count, 0.0f);
    }

    float smooth = std::clamp(audio_settings.smoothing, 0.0f, 0.99f);
    for(int i = 0; i < count; ++i)
    {
        int idx = band_start + i;
        float sample = 0.0f;
        if(idx >= 0 && idx < static_cast<int>(spectrum.size()))
        {
            sample = std::clamp(spectrum[idx], 0.0f, 1.0f);
        }
        smoothed_bands[i] = smooth * smoothed_bands[i] + (1.0f - smooth) * sample;
    }
}

float BandScan3D::ResolveCoordinateNormalized(const GridContext3D* grid, float x, float y, float z) const
{
    EffectAxis axis = GetAxis();
    bool reverse = GetReverse();
    float normalized = 0.0f;

    auto normalize_linear = [](float value, float min, float max) -> float
    {
        float range = max - min;
        if(range <= 1e-5f)
        {
            return 0.0f;
        }
        return (value - min) / range;
    };

    if(grid)
    {
        switch(axis)
        {
            case AXIS_X:
                normalized = normalize_linear(x, grid->min_x, grid->max_x);
                break;
            case AXIS_Y:
                normalized = normalize_linear(y, grid->min_y, grid->max_y);
                break;
            case AXIS_Z:
                normalized = normalize_linear(z, grid->min_z, grid->max_z);
                break;
            case AXIS_RADIAL:
            default:
            {
                float dx = x - grid->center_x;
                float dy = y - grid->center_y;
                float dz = z - grid->center_z;
                float radius = std::sqrt(dx * dx + dy * dy + dz * dz);
                float max_radius = 0.5f * std::max({grid->width, grid->height, grid->depth});
                if(max_radius <= 1e-5f)
                {
                    normalized = 0.0f;
                }
                else
                {
                    normalized = radius / max_radius;
                }
            }
                break;
        }
    }
    else
    {
        float value = 0.0f;
        switch(axis)
        {
            case AXIS_X: value = x; break;
            case AXIS_Y: value = y; break;
            case AXIS_Z: value = z; break;
            case AXIS_RADIAL:
            default:
                value = std::sqrt(x * x + y * y + z * z);
                break;
        }
        normalized = std::fmod(std::fabs(value), 1.0f);
    }

    normalized = std::clamp(normalized, 0.0f, 1.0f);
    if(reverse)
    {
        normalized = 1.0f - normalized;
    }
    return normalized;
}

float BandScan3D::ResolveHeightNormalized(const GridContext3D* grid, float x, float y, float z) const
{
    (void)x;
    EffectAxis axis = GetAxis();
    if(grid)
    {
        switch(axis)
        {
            case AXIS_Y:
                return NormalizeRange(z, grid->min_z, grid->max_z);
            case AXIS_X:
            case AXIS_Z:
            case AXIS_RADIAL:
            default:
                return NormalizeRange(y, grid->min_y, grid->max_y);
        }
    }
    else
    {
        switch(axis)
        {
            case AXIS_Y:
                return std::clamp(0.5f + z, 0.0f, 1.0f);
            case AXIS_X:
            case AXIS_Z:
            case AXIS_RADIAL:
            default:
                return std::clamp(0.5f + y, 0.0f, 1.0f);
        }
    }
}

float BandScan3D::ResolveRadialNormalized(const GridContext3D* grid, float x, float y, float z) const
{
    if(grid)
    {
        float dx = x - grid->center_x;
        float dy = y - grid->center_y;
        float dz = z - grid->center_z;
        float max_radius = 0.5f * std::max({grid->width, grid->height, grid->depth});
        return ComputeRadialNormalized(dx, dy, dz, max_radius);
    }
    return std::clamp(std::sqrt(x * x + y * y + z * z) / 0.75f, 0.0f, 1.0f);
}

float BandScan3D::WrapDistance(float a, float b, int modulo) const
{
    float diff = std::fabs(a - b);
    if(modulo <= 0)
    {
        return diff;
    }
    float wrapped = std::fmod(diff, static_cast<float>(modulo));
    return std::min(wrapped, static_cast<float>(modulo) - wrapped);
}

RGBColor BandScan3D::ComposeColor(float axis_pos, float height_norm, float radial_norm, float time, float brightness, const RGBColor& axis_color, bool rainbow_mode) const
{
    if(smoothed_bands.empty())
    {
        RGBColor base = ComposeAudioGradientColor(audio_settings, axis_pos, 0.0f);
        base = ScaleRGBColor(base, brightness);
        return ModulateRGBColors(base, axis_color);
    }

    int count = static_cast<int>(smoothed_bands.size());
    float scaled = axis_pos * count;
    int idx_local = std::clamp(static_cast<int>(std::floor(scaled)), 0, count - 1);
    float frac = scaled - std::floor(scaled);
    int idx_next = std::min(idx_local + 1, count - 1);
    float band_value = smoothed_bands[idx_local] + (smoothed_bands[idx_next] - smoothed_bands[idx_local]) * frac;
    band_value = std::clamp(band_value, 0.0f, 1.0f);

    float phase = CalculateProgress(time);
    float scan_phase = std::fmod(phase, 1.0f);
    if(scan_phase < 0.0f)
    {
        scan_phase += 1.0f;
    }
    if(GetReverse())
    {
        scan_phase = 1.0f - scan_phase;
    }
    float scan_index = scan_phase * count;
    float distance = WrapDistance(scaled, scan_index, count);
    float highlight = std::exp(-distance * 1.35f);
    float trail = std::exp(-std::max(distance - 0.6f, 0.0f) * 2.5f);

    float height_profile = std::pow(std::clamp(height_norm, 0.0f, 1.0f), 1.3f);
    float radial_profile = std::clamp(1.0f - radial_norm, 0.0f, 1.0f);
    float energy = band_value * (0.55f + 0.45f * height_profile) * (0.45f + 0.55f * radial_profile);
    energy *= (0.65f * highlight + 0.35f * trail);
    energy = std::clamp(energy, 0.0f, 1.0f);
    float intensity = ApplyAudioIntensity(energy, audio_settings);

    float gradient_pos = (count > 1) ? (float)idx_local / (float)(count - 1) : axis_pos;
    RGBColor color = ComposeAudioGradientColor(audio_settings, gradient_pos, intensity);
    color = ScaleRGBColor(color, brightness * (0.35f + 0.65f * intensity));

    RGBColor modulation = axis_color;
    if(rainbow_mode)
    {
        // Base class helpers are non-const, so cast is safe for palette queries.
        auto* mutable_self = const_cast<BandScan3D*>(this);
        modulation = mutable_self->GetRainbowColor(scan_phase * 360.0f);
    }

    color = ModulateRGBColors(color, modulation);
    return color;
}

REGISTER_EFFECT_3D(BandScan3D)
