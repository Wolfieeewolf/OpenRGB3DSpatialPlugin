// SPDX-License-Identifier: GPL-2.0-only

#include "BandScan.h"
#include "AudioReactiveUi.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <QVBoxLayout>

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

BandScan::BandScan(QWidget* parent)
    : SpatialEffect3D(parent)
{
    RefreshBandRange();
}

EffectInfo3D BandScan::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Band Scan";
    info.effect_description =
        "A lit band scans across the spectrum axis; energy comes from FFT bins in the Hz range you set";
    info.category = "Audio";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 10.0f;
    info.needs_frequency = true;
    info.default_frequency_scale = 20.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = true;
    info.supports_height_bands = true;
    info.supports_strip_colormap = true;

    return info;
}

void BandScan::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto on_changed = [this]() { emit ParametersChanged(); };

    AudioReactiveUi::AudioResponseUiOptions response_opts;
    response_opts.include_falloff = true;
    response_opts.falloff_tooltip = QStringLiteral(
        "How sharply the scan peak drops away from the lit band (higher = tighter highlight).");
    response_opts.peak_boost_tooltip =
        QStringLiteral("Amplifies quiet input so the scan reads on sparse LED layouts.");
    AudioReactiveUi::AppendStandardSpectrumAnalyzerSections(
        layout, audio_settings, this, on_changed, response_opts);

    AddWidgetToParent(w, parent);
}

void BandScan::UpdateParams(SpatialEffectParams& /*params*/)
{
    RefreshBandRange();
}

RGBColor BandScan::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
    {
        return 0x00000000;
    }

    EnsureSpectrumCache(time);
    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float coord2 = NormalizeGridAxis01(rotated_pos.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(sw, grid, x, y, z, origin, time);

    float axis_pos = ResolveCoordinateNormalized(&grid, rotated_pos.x, rotated_pos.y, rotated_pos.z);
    float height_norm = ResolveHeightNormalized(&grid, rotated_pos.x, rotated_pos.y, rotated_pos.z);
    float radial_norm = ResolveRadialNormalized(&grid, rotated_pos.x, rotated_pos.y, rotated_pos.z);
    float size_m = GetNormalizedSize();
    float detail = std::max(0.05f, GetScaledDetail());
    float center = 0.5f;
    axis_pos = std::clamp(center + (axis_pos - center) * (0.6f + 0.4f * size_m) * (0.7f + 0.3f * detail * bb.tight_mul),
                          0.0f, 1.0f);

    SpatialLayerCore::Basis basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
    SpatialLayerCore::MapperSettings map;
    SpatialLayerCore::InitAudioEffectMapperSettings(map, GetNormalizedScale(), detail);

    bool rainbow_mode = GetRainbowMode();
    float strip_phase_p01 = -1.0f;

    AudioReactiveColorParams color_params;
    color_params.gradient_pos01 = axis_pos;
    color_params.intensity = 1.0f;
    color_params.beat_color_slot = (uint32_t)std::floor(time * 2.5f);
    color_params.time = time;
    color_params.grid_x = x;
    color_params.grid_y = y;
    color_params.grid_z = z;
    color_params.grid = &grid;
    color_params.origin = origin;
    color_params.rotated_pos = rotated_pos;
    color_params.y_norm01 = coord2;
    color_params.stratum_mot01 = stratum_mot01;
    color_params.band_scalars = &bb;
    RGBColor axis_color = ResolveAudioReactiveColor(audio_settings, color_params);

    return ComposeColor(axis_pos,
                        height_norm,
                        radial_norm,
                        time,
                        1.0f,
                        axis_color,
                        rainbow_mode,
                        bb.speed_mul,
                        bb.tight_mul,
                        EffectStratumBlend::CombinedPhase01(bb, stratum_mot01),
                        strip_phase_p01);
}

nlohmann::json BandScan::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    return j;
}

void BandScan::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    RefreshBandRange();
    last_sample_time = std::numeric_limits<float>::lowest();

    AudioReactiveUi::SyncSettingsToHost(GetCustomSettingsHost(), audio_settings, this);
}

void BandScan::RefreshBandRange()
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

void BandScan::EnsureSpectrumCache(float time)
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
    AudioInputManager::instance()->getBands(bands_cache);
    UpdateSmoothedBands(bands_cache);
}

void BandScan::UpdateSmoothedBands(const std::vector<float>& spectrum)
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

static float normalize_linear(float value, float min, float max)
{
    return NormalizeGridAxis01(value, min, max);
}

float BandScan::ResolveCoordinateNormalized(const GridContext3D* grid, float x, float y, float z) const
{
    (void)y;
    (void)z;
    float normalized = 0.0f;
    if(grid)
    {
        normalized = normalize_linear(x, grid->min_x, grid->max_x);
    }
    else
    {
        normalized = std::fmod(std::fabs(x), 1.0f);
    }

    normalized = std::clamp(normalized, 0.0f, 1.0f);
    return normalized;
}

float BandScan::ResolveHeightNormalized(const GridContext3D* grid, float x, float y, float z) const
{
    (void)x;
    (void)z;
    if(grid)
    {
        return NormalizeGridAxis01(y, grid->min_y, grid->max_y);
    }
    else
    {
        return std::clamp(0.5f + y, 0.0f, 1.0f);
    }
}

float BandScan::ResolveRadialNormalized(const GridContext3D* grid, float x, float y, float z) const
{
    if(grid)
    {
        Vector3D o = GetEffectOriginGrid(*grid);
        float dx = x - o.x;
        float dy = y - o.y;
        float dz = z - o.z;
        float max_radius = EffectGridMedianHalfExtent(*grid, GetNormalizedScale()) * 1.7320508f;
        return ComputeRadialNormalized(dx, dy, dz, max_radius);
    }
    return std::clamp(std::sqrt(x * x + y * y + z * z) / 0.75f, 0.0f, 1.0f);
}

float BandScan::WrapDistance(float a, float b, int modulo) const
{
    float diff = std::fabs(a - b);
    if(modulo <= 0)
    {
        return diff;
    }
    float wrapped = std::fmod(diff, static_cast<float>(modulo));
    return std::min(wrapped, static_cast<float>(modulo) - wrapped);
}

RGBColor BandScan::ComposeColor(float axis_pos,
                                float height_norm,
                                float radial_norm,
                                float time,
                                float brightness,
                                const RGBColor& axis_color,
                                bool rainbow_mode,
                                float stratum_speed_mul,
                                float stratum_tight_mul,
                                float stratum_phase01,
                                float strip_cmap_p01) const
{
    float ap = std::fmod(axis_pos + stratum_phase01 + 1.0f, 1.0f);
    if(smoothed_bands.empty())
    {
        (void)brightness;
        RGBColor base = ComposeAudioGradientColor(audio_settings, ap, 0.0f);
        return ModulateRGBColors(base, axis_color);
    }

    int count = static_cast<int>(smoothed_bands.size());
    float scaled = ap * count;
    int idx_local = std::clamp(static_cast<int>(std::floor(scaled)), 0, count - 1);
    float frac = scaled - std::floor(scaled);
    int idx_next = std::min(idx_local + 1, count - 1);
    float band_value = smoothed_bands[idx_local] + (smoothed_bands[idx_next] - smoothed_bands[idx_local]) * frac;
    band_value = std::clamp(band_value, 0.0f, 1.0f);

    float phase = CalculateProgress(time) * stratum_speed_mul;
    float scan_phase = std::fmod(phase, 1.0f);
    if(scan_phase < 0.0f)
    {
        scan_phase += 1.0f;
    }
    float scan_index = scan_phase * count;
    float distance = WrapDistance(scaled, scan_index, count);
    float tm = std::clamp(stratum_tight_mul, 0.25f, 4.0f);
    float highlight = std::exp(-distance * 1.35f * tm);
    float trail = std::exp(-std::max(distance - 0.6f, 0.0f) * 2.5f * tm);

    float height_strata = 1.0f;
    if(UseSpatialRoomTint())
    {
        SpatialLayerCore::MapperSettings smap;
        SpatialLayerCore::InitAudioEffectMapperSettings(smap, GetNormalizedScale(), std::max(0.05f, GetScaledDetail()));
        float lw[SpatialLayerCore::kMaxLayerCount]{};
        SpatialLayerCore::ComputeVerticalStratumWeights(height_norm, smap, 3, lw);
        height_strata = 0.55f + 0.45f * (lw[0] * 0.68f + lw[1] * 0.92f + lw[2] * 1.0f);
    }
    float height_profile = std::pow(std::clamp(height_norm, 0.0f, 1.0f), 1.3f) * height_strata;
    float radial_profile = std::clamp(1.0f - radial_norm, 0.0f, 1.0f);
    float energy = band_value * (0.55f + 0.45f * height_profile) * (0.45f + 0.55f * radial_profile);
    energy *= (0.65f * highlight + 0.35f * trail);
    energy = std::clamp(energy, 0.0f, 1.0f);
    float intensity = ApplyAudioPulseIntensity(energy, audio_settings);

    float gradient_pos = (count > 1) ? (float)idx_local / (float)(count - 1) : ap;
    RGBColor color = ComposeAudioGradientColor(audio_settings, gradient_pos, intensity);
    color = BrightenAudioEffectColor(color, intensity);

    RGBColor modulation = axis_color;
    if(rainbow_mode)
    {
        BandScan* mutable_self = const_cast<BandScan*>(this);
        if(strip_cmap_p01 >= 0.0f)
        {
            float p_mod = std::fmod(strip_cmap_p01 + scan_phase * (100.0f / 360.0f) + 1.0f, 1.0f);
            modulation = ResolveStripKernelFinalColor(*mutable_self, GetEffectStripColormapKernel(), p_mod,
                                                        GetEffectStripColormapColorStyle(), time,
                                                        mutable_self->GetScaledFrequency() * 12.0f * stratum_speed_mul);
        }
        else
        {
            modulation = mutable_self->GetRainbowColor(scan_phase * 360.0f);
        }
    }

    color = ModulateRGBColors(color, modulation);
    return color;
}

REGISTER_EFFECT_3D(BandScan)
