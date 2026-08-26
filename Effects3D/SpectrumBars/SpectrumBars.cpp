// SPDX-License-Identifier: GPL-2.0-only

#include "SpectrumBars.h"
#include "SpectrumBarsVolumeFieldGlsl.h"
#include "AudioReactiveUi.h"
#include "PluginLog.h"
#include "SpatialLayerCore.h"
#include <algorithm>
#include <cmath>
#include <QVBoxLayout>
#include <QByteArray>
#include <QImage>
#include <QVector3D>
#include "EffectUiRows.h"
#include "EffectUiSync.h"

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

SpectrumBars::SpectrumBars(QWidget* parent)
    : SpatialEffect3D(parent)
{
    volume_assist_.setFragmentBody(QString::fromUtf8(SpectrumBarsVolumeFieldGlsl()));
    volume_assist_.setResolution(22);
    RefreshBandRange();
}

SpectrumBars::~SpectrumBars() = default;

EffectInfo3D SpectrumBars::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Spectrum Bars";
    info.effect_description =
        "Spectrum bar graph in the zone (GPU volume field). Role/Hz set which part of the mix feeds the bars; "
        "Rainbow or Hue-along-position for full color range.";
    info.category = "Audio";
    info.effect_type = SPATIAL_EFFECT_SPECTRUM_BARS;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;
    info.default_speed_scale = 10.0f;
    info.default_frequency_scale = 20.0f;
    info.use_size_parameter = true;
    info.show_speed_control = false;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = true;
    info.supports_height_bands = true;
    info.supports_strip_colormap = true;

    return info;
}

void SpectrumBars::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto on_changed = [this]() { emit ParametersChanged(); };

    AudioReactiveUi::AppendStandardFrequencyBandSection(layout, audio_settings, this, on_changed);

    QVBoxLayout* effect_body = EffectUiRows::AppendCollapsibleSectionBody(layout, QStringLiteral("Effect"));

    QWidget* effect_section = EffectUiRows::NewEffectPanel("SpectrumBarsEffectSettings");
    EffectSliderRow* roll_speed_row = EffectUiRows::AppendSliderRow(
        EffectUiRows::PanelLayout(effect_section),
        QStringLiteral("Roll speed:"),
        0,
        200,
        (int)(roll_speed * 100.0f),
        QStringLiteral("Scrolls the bar pattern along the spectrum axis over time."));
    roll_speed_row->setObjectName(QStringLiteral("rollSpeedRow"));
    roll_speed_row->bindValueChanged(
        this,
        [this](int v) { roll_speed = v / 100.0f; },
        [](int v) { return QString::number(v / 100.0f, 'f', 2); },
        on_changed);
    if(effect_body)
    {
        effect_body->addWidget(effect_section);
    }
    else
    {
        layout->addWidget(effect_section);
    }

    AudioReactiveUi::AudioResponseUiOptions response_opts;
    response_opts.include_falloff = true;
    response_opts.falloff_tooltip =
        QStringLiteral("How sharply each bar falls off above its peak height.");
    response_opts.peak_boost_tooltip =
        QStringLiteral("Raises quiet spectrum energy so bars still light the grid.");
    AudioReactiveUi::AppendStandardResponseSection(layout, audio_settings, this, on_changed, response_opts);
    AudioReactiveUi::AppendAudioSectionBody(layout, QStringLiteral("Color"));
    AudioReactiveUi::AppendAudioPulseColorModeRow(layout, audio_settings, this, on_changed);

    AddWidgetToParent(w, parent);
}

void SpectrumBars::UploadBandsMedia()
{
    const int count = std::max(1, (int)smoothed_bands.size());
    QImage img(count, 1, QImage::Format_RGBA8888);
    for(int i = 0; i < count; ++i)
    {
        const float v = (i < (int)smoothed_bands.size()) ? std::clamp(smoothed_bands[i], 0.0f, 1.0f) : 0.0f;
        const int g = (int)std::lround(v * 255.0f);
        img.setPixel(i, 0, qRgba(g, g, g, 255));
    }
    volume_assist_.setMediaTexture(img, false);
}

void SpectrumBars::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& /*grid*/)
{
    EnsureSpectrumCache(time_sec);

    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(0.5f, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());

    UploadBandsMedia();

    const float band_count = (float)std::max(1, (int)smoothed_bands.size());
    const float roll_phase =
        (roll_speed > 1e-6f) ? std::fmod(time_sec * roll_speed * bb.speed_mul + 1000.0f, 1.0f) : 0.0f;
    const float size_m = std::max(0.35f, GetNormalizedSize());
    const float detail = std::max(0.05f, GetScaledDetail());
    const float falloff_scale =
        std::clamp(0.35f + audio_settings.falloff * 0.002f, 0.25f, 2.5f);
    const float peak_boost = std::clamp(audio_settings.peak_boost, 0.0f, 4.0f);
    const float sweep_phase = CalculateProgress(time_sec);

    float vp[10] = {
        band_count,
        roll_speed,
        roll_phase,
        size_m,
        detail,
        std::max(0.15f, bb.speed_mul),
        std::max(0.25f, bb.tight_mul),
        falloff_scale,
        peak_boost,
        sweep_phase
    };
    if(!volume_assist_.prepare(render_sequence, time_sec, vp, 10))
    {
        static bool logged_once = false;
        if(!logged_once)
        {
            logged_once = true;
            const QString err = volume_assist_.lastError();
            const QByteArray err_bytes = err.isEmpty() ? QByteArray("ensureReady failed") : err.toUtf8();
            LOG_WARNING("[OpenRGB3DSpatialPlugin] SpectrumBars volume assist unavailable: %s",
                        err_bytes.constData());
        }
    }
}

RGBColor SpectrumBars::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    if(!volume_assist_.isAvailable())
        return 0x00000000;

    Vector3D rotated_pos{x, y, z};
    float coord2 = SampleStratumYNorm01(rotated_pos.y, grid, origin);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(sw, grid, x, y, z, origin, time);

    const float nx = NormalizeGridAxis01(rotated_pos.x, grid.min_x, grid.max_x);
    const float ny = NormalizeGridAxis01(rotated_pos.y, grid.min_y, grid.max_y);
    const float nz = NormalizeGridAxis01(rotated_pos.z, grid.min_z, grid.max_z);
    const QVector3D samp = volume_assist_.sample01(nx, ny, nz);
    float energy = samp.x();
    float gradient_pos = samp.y();
    if(GetStratumLayoutMode() == 1)
        energy = EffectStratumBlend::ApplyMotionToUnit01(energy, stratum_mot01, 0.18f);

    float intensity = ApplyAudioVisualIntensity(energy, audio_settings);
    if(intensity <= 0.001f)
        return 0x00000000;

    AudioReactiveColorParams color_params;
    color_params.gradient_pos01 = gradient_pos;
    color_params.intensity = intensity;
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
    RGBColor color = ResolveAudioReactiveColor(audio_settings, color_params);
    return BrightenAudioEffectColor(color, intensity);
}

nlohmann::json SpectrumBars::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    j["roll_speed"] = roll_speed;
    return j;
}

void SpectrumBars::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    if(settings.contains("roll_speed"))
        roll_speed = settings["roll_speed"].get<float>();

    RefreshBandRange();
    last_sample_time = std::numeric_limits<float>::lowest();

    AudioReactiveUi::SyncSettingsToHost(GetCustomSettingsHost(), audio_settings);
    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "SpectrumBarsEffectSettings"))
        {
            EffectUiSync::setSliderValue(fx, "rollSpeedRow", (int)(roll_speed * 100.0f),
                                          [](int v) { return QString::number(v / 100.0f, 'f', 2); });
        }
    }
}

void SpectrumBars::RefreshBandRange()
{
    AudioInputManager* audio = AudioInputManager::instance();
    int total_bands = audio->getBandsCount();
    if(total_bands <= 0)
        total_bands = 16;

    float sample_rate = static_cast<float>(audio->getSampleRate());
    if(sample_rate <= 0.0f)
        sample_rate = 48000.0f;
    int fft_size = audio->getFFTSize();
    if(fft_size <= 0)
        fft_size = 1024;

    float f_min = std::max(1.0f, sample_rate / std::max(1, fft_size));
    float f_max = sample_rate * 0.5f;
    if(f_max <= f_min)
        f_max = f_min + 1.0f;

    int start = MapHzToBandIndex((float)audio_settings.low_hz, total_bands, f_min, f_max);
    int end = MapHzToBandIndex((float)audio_settings.high_hz, total_bands, f_min, f_max);
    if(end < start)
        std::swap(end, start);

    band_start = std::clamp(start, 0, total_bands - 1);
    band_end = std::clamp(end, band_start, total_bands - 1);

    int count = std::max(1, band_end - band_start + 1);
    if(static_cast<int>(smoothed_bands.size()) != count)
        smoothed_bands.assign(count, 0.0f);
}

void SpectrumBars::EnsureSpectrumCache(float time)
{
    const float epsilon = 1e-4f;
    if(last_sample_time != std::numeric_limits<float>::lowest())
    {
        if(std::fabs(time - last_sample_time) <= epsilon)
            return;
    }

    float delta_time = 0.0f;
    if(last_sample_time != std::numeric_limits<float>::lowest())
        delta_time = std::max(0.0f, time - last_sample_time);
    last_sample_time = time;
    AudioInputManager::instance()->getBands(bands_cache);
    UpdateSmoothedBands(bands_cache, delta_time);
}

void SpectrumBars::UpdateSmoothedBands(const std::vector<float>& spectrum, float /*delta_time*/)
{
    RefreshBandRange();
    int count = band_end - band_start + 1;
    if(count <= 0)
    {
        smoothed_bands.clear();
        return;
    }
    if(static_cast<int>(smoothed_bands.size()) != count)
        smoothed_bands.assign(count, 0.0f);

    float smooth = std::clamp(audio_settings.smoothing, 0.0f, 0.99f);
    for(int i = 0; i < count; ++i)
    {
        int idx = band_start + i;
        float sample = 0.0f;
        if(idx >= 0 && idx < static_cast<int>(spectrum.size()))
            sample = std::clamp(spectrum[idx], 0.0f, 1.0f);
        smoothed_bands[i] = smooth * smoothed_bands[i] + (1.0f - smooth) * sample;
    }
}

REGISTER_EFFECT_3D(SpectrumBars);
