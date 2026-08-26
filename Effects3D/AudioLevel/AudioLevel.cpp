// SPDX-License-Identifier: GPL-2.0-only

#include "AudioLevel.h"
#include "AudioLevelVolumeFieldGlsl.h"
#include "AudioReactiveUi.h"
#include "PluginLog.h"
#include "SpatialLayerCore.h"
#include <cmath>
#include <algorithm>
#include <QByteArray>
#include <QVector3D>
#include <QVBoxLayout>
#include "EffectUiRows.h"
#include "EffectUiSync.h"

float AudioLevel::EvaluateIntensity(float amplitude, float time)
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

AudioLevel::AudioLevel(QWidget* parent)
    : SpatialEffect3D(parent)
{
    volume_assist_.setFragmentBody(QString::fromUtf8(AudioLevelVolumeFieldGlsl()));
    volume_assist_.setResolution(20);
}

EffectInfo3D AudioLevel::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Audio Level";
    info.effect_description =
        "Rising fill surface driven by your Listen Role / Hz band (GPU volume field). "
        "Turn on Rainbow for spatial spectrum color; Follow notes uses ColorChord HSV hues.";
    info.category = "Audio";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;

    info.default_speed_scale = 10.0f;
    info.default_frequency_scale = 20.0f;
    info.use_size_parameter = true;

    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_axis_control = false;
    info.show_color_controls = true;
    info.show_path_axis_control = true;
    info.supports_height_bands = true;
    info.supports_strip_colormap = true;

    return info;
}

void AudioLevel::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto on_changed = [this]() { emit ParametersChanged(); };

    AudioReactiveUi::AppendStandardFrequencyBandSection(layout, audio_settings, this, on_changed);

    QVBoxLayout* effect_body = EffectUiRows::AppendCollapsibleSectionBody(layout, QStringLiteral("Effect"));

    QWidget* effect_section = EffectUiRows::NewEffectPanel("AudioLevelEffectSettings");
    QVBoxLayout* effect_layout = EffectUiRows::PanelLayout(effect_section);
    const auto pct_format = [](int v) { return QString::number(v) + QStringLiteral("%"); };

    EffectSliderRow* boundary_wave_row = EffectUiRows::AppendSliderRow(
        effect_layout,
        QStringLiteral("Boundary wave:"),
        0,
        50,
        (int)std::lround(wave_amount * 100.0f),
        QStringLiteral("Wobble on the lit/dark boundary (Path axis in common controls)."));
    boundary_wave_row->setObjectName(QStringLiteral("boundaryWaveRow"));
    boundary_wave_row->bindValueChanged(
        this, [this](int v) { wave_amount = v / 100.0f; }, pct_format, on_changed);

    EffectSliderRow* edge_softness_row = EffectUiRows::AppendSliderRow(
        effect_layout,
        QStringLiteral("Edge softness:"),
        2,
        50,
        (int)std::lround(edge_soft * 100.0f),
        QStringLiteral("Thickness of the transition at the fill surface (higher = softer)."));
    edge_softness_row->setObjectName(QStringLiteral("edgeSoftnessRow"));
    edge_softness_row->bindValueChanged(
        this, [this](int v) { edge_soft = v / 100.0f; }, pct_format, on_changed);
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
    response_opts.falloff_label = QStringLiteral("Fill edge:");
    response_opts.falloff_slider_max = 500;
    response_opts.falloff_tooltip =
        QStringLiteral("Steepness of the lit region versus dark below the fill boundary.");
    AudioReactiveUi::AppendStandardResponseSection(layout, audio_settings, this, on_changed, response_opts);
    AudioReactiveUi::AppendAudioSectionBody(layout, QStringLiteral("Color"));
    AudioReactiveUi::AppendAudioPulseColorModeRow(layout, audio_settings, this, on_changed);

    AddWidgetToParent(w, parent);
}

void AudioLevel::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& /*grid*/)
{
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(0.5f, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());

    const float amplitude = SampleAudioVisualLevel(audio_settings);
    const float fill_level = EvaluateIntensity(amplitude, time_sec);
    const float wave = std::clamp(wave_amount, 0.0f, 0.5f);
    const float edge = std::clamp(edge_soft, 0.02f, 0.5f);
    const float size_m = std::max(0.35f, GetNormalizedSize());
    const float detail = std::max(0.05f, GetScaledDetail());
    const float wave_freq = std::max(0.2f, GetScaledFrequency() * 0.15f * bb.tight_mul);
    const float time_e = time_sec * bb.speed_mul;

    float vp[10] = {
        fill_level,
        wave,
        edge,
        (float)std::clamp(GetPathAxis(), 0, 2),
        size_m,
        wave_freq,
        detail,
        std::max(0.15f, bb.speed_mul),
        std::max(0.25f, bb.tight_mul),
        time_e
    };
    if(!volume_assist_.prepare(render_sequence, time_sec, vp, 10))
    {
        static bool logged_once = false;
        if(!logged_once)
        {
            logged_once = true;
            const QString err = volume_assist_.lastError();
            const QByteArray err_bytes = err.isEmpty() ? QByteArray("ensureReady failed") : err.toUtf8();
            LOG_WARNING("[OpenRGB3DSpatialPlugin] AudioLevel volume assist unavailable: %s",
                        err_bytes.constData());
        }
    }
}

RGBColor AudioLevel::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
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
    float intensity = samp.x();
    float gradient_pos = samp.y();
    if(GetStratumLayoutMode() == 1)
        intensity = EffectStratumBlend::ApplyMotionToUnit01(intensity, stratum_mot01, 0.18f);
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

nlohmann::json AudioLevel::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    j["wave_amount"] = wave_amount;
    j["edge_soft"] = edge_soft;
    return j;
}

void AudioLevel::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    if(settings.contains("wave_amount"))
    {
        float w = settings["wave_amount"].get<float>();
        /* Legacy UI wrote 0–100; normalize to 0–0.5. */
        if(w > 1.0f)
            w *= 0.01f;
        wave_amount = std::clamp(w, 0.0f, 0.5f);
    }
    if(settings.contains("edge_soft"))
    {
        float e = settings["edge_soft"].get<float>();
        if(e > 1.0f)
            e *= 0.01f;
        edge_soft = std::clamp(e, 0.02f, 0.5f);
    }
    smoothed = 0.0f;
    last_intensity_time = std::numeric_limits<float>::lowest();

    AudioReactiveUi::SyncSettingsToHost(GetCustomSettingsHost(), audio_settings);
    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "AudioLevelEffectSettings"))
        {
            const auto pct = [](int v) { return QString::number(v) + QStringLiteral("%"); };
            EffectUiSync::setSliderValue(fx, "boundaryWaveRow", (int)std::lround(wave_amount * 100.0f), pct);
            EffectUiSync::setSliderValue(fx, "edgeSoftnessRow", (int)std::lround(edge_soft * 100.0f), pct);
        }
    }
}

REGISTER_EFFECT_3D(AudioLevel)
