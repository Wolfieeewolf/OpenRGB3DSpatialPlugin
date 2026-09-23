// SPDX-License-Identifier: GPL-2.0-only

#include "AudioPaintBrush.h"
#include "AudioPaintBrushVolumeFieldGlsl.h"
#include "AudioReactiveUi.h"
#include "PluginLog.h"
#include "SpatialLayerCore.h"
#include "EffectUiRows.h"
#include "EffectUiSync.h"
#include <QByteArray>
#include <algorithm>
#include <cmath>
#include <QVBoxLayout>

REGISTER_EFFECT_3D(AudioPaintBrush);

namespace
{
float SmoothToward(float cur, float target, float alpha)
{
    return alpha * cur + (1.0f - alpha) * target;
}
} // namespace

AudioPaintBrush::AudioPaintBrush(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(true);
    volume_assist_.setFragmentBody(QString::fromUtf8(AudioPaintBrushVolumeFieldGlsl()));
    volume_assist_.setResolution(20);
}

EffectInfo3D AudioPaintBrush::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Audio Paintbrush";
    info.effect_description =
        "Soft Lissajous ribbons in the room: bass fattens amp, mid shifts phase, treble thins the brush. "
        "Inspired by MoonLight PaintBrush (no raymarch).";
    info.category = "Audio";
    info.effect_type = SPATIAL_EFFECT_AUDIO_PAINTBRUSH;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 14.0f;
    info.needs_frequency = true;
    info.default_frequency_scale = 10.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_axis_control = false;
    info.show_color_controls = true;
    info.supports_height_bands = true;
    info.supports_strip_colormap = true;
    return info;
}

void AudioPaintBrush::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto on_changed = [this]() { emit ParametersChanged(); };
    const auto pct_format = [](int v) { return QString::number(v) + QStringLiteral("%"); };

    AudioReactiveUi::AppendStandardFrequencyBandSection(layout, audio_settings, this, on_changed);

    QVBoxLayout* effect_body = EffectUiRows::AppendCollapsibleSectionBody(layout, QStringLiteral("Effect"));
    QWidget* effect_section = EffectUiRows::NewEffectPanel("AudioPaintBrushEffectSettings");
    QVBoxLayout* effect_layout = EffectUiRows::PanelLayout(effect_section);

    EffectSliderRow* thick_row = EffectUiRows::AppendSliderRow(
        effect_layout,
        QStringLiteral("Brush thickness:"),
        2,
        100,
        (int)std::lround(thickness * 100.0f),
        QStringLiteral("Soft width of each ribbon."));
    thick_row->setObjectName(QStringLiteral("thicknessRow"));
    thick_row->bindValueChanged(
        this, [this](int v) { thickness = v / 100.0f; }, pct_format, on_changed);

    EffectSliderRow* chaos_row = EffectUiRows::AppendSliderRow(
        effect_layout,
        QStringLiteral("Phrase chaos:"),
        0,
        150,
        (int)std::lround(chaos * 100.0f),
        QStringLiteral("Phase offset spread between ribbons."));
    chaos_row->setObjectName(QStringLiteral("chaosRow"));
    chaos_row->bindValueChanged(
        this, [this](int v) { chaos = v / 100.0f; }, pct_format, on_changed);

    EffectSliderRow* ribbons_row = EffectUiRows::AppendSliderRow(
        effect_layout,
        QStringLiteral("Ribbons:"),
        1,
        4,
        ribbon_count,
        QStringLiteral("How many Lissajous brushes to draw."));
    ribbons_row->setObjectName(QStringLiteral("ribbonsRow"));
    ribbons_row->bindValueChanged(
        this, [this](int v) { ribbon_count = v; }, [](int v) { return QString::number(v); }, on_changed);

    if(effect_body)
        effect_body->addWidget(effect_section);
    else
        layout->addWidget(effect_section);

    AudioReactiveUi::AudioResponseUiOptions response_opts;
    response_opts.include_falloff = true;
    AudioReactiveUi::AppendStandardResponseSection(layout, audio_settings, this, on_changed, response_opts);
    AudioReactiveUi::AppendAudioSectionBody(layout, QStringLiteral("Color"));
    AudioReactiveUi::AppendAudioPulseColorModeRow(layout, audio_settings, this, on_changed);

    AddWidgetToParent(w, parent);
}

void AudioPaintBrush::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& /*grid*/)
{
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(0.5f, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());

    AudioInputManager* audio = AudioInputManager::instance();
    float bass_t = 0.0f, mid_t = 0.0f, high_t = 0.0f;
    if(audio)
    {
        bass_t = std::clamp(audio->getBandSlowEnergyHz(40.0f, 180.0f), 0.0f, 1.0f);
        mid_t = std::clamp(audio->getBandSlowEnergyHz(200.0f, 2000.0f), 0.0f, 1.0f);
        high_t = std::clamp(audio->getBandSlowEnergyHz(2500.0f, 12000.0f), 0.0f, 1.0f);
    }

    const float alpha = std::clamp(audio_settings.smoothing, 0.0f, 0.95f);
    if(last_tick == std::numeric_limits<float>::lowest() || std::fabs(time_sec - last_tick) > 1e-4f)
    {
        bass_s = SmoothToward(bass_s, bass_t, alpha);
        mid_s = SmoothToward(mid_s, mid_t, alpha);
        high_s = SmoothToward(high_s, high_t, alpha);
        last_tick = time_sec;
        /* Bass pushes the brush “into the future” (MoonLight PaintBrush idea). */
        const float drive = ApplyAudioVisualIntensity(SampleAudioVisualLevel(audio_settings), audio_settings);
        time_boost += (0.35f + 1.8f * bass_s + 0.4f * drive) * std::max(0.0f, GetNormalizedSpeed()) * bb.speed_mul * 0.016f;
        /* Keep phase bounded — unbounded sin args lose precision and look broken. */
        time_boost = std::fmod(time_boost + 1000.0f, 1000.0f);
    }

    const float hue_scroll = std::fmod(time_sec * GetColorCycleHz() * bb.speed_mul + 1000.0f, 1.0f);
    const float vp[10] = {
        ApplyAudioIntensity(bass_s, audio_settings),
        ApplyAudioIntensity(mid_s, audio_settings),
        ApplyAudioIntensity(high_s, audio_settings),
        time_boost,
        std::max(0.015f, thickness),
        std::clamp(GetNormalizedSize(), 0.35f, 2.5f),
        std::clamp(GetNormalizedDetail(), 0.05f, 1.0f),
        hue_scroll,
        std::clamp(chaos, 0.0f, 1.5f),
        (float)std::clamp(ribbon_count, 1, 4)
    };
    if(!volume_assist_.prepare(render_sequence, time_sec, vp, 10))
    {
        static bool logged_once = false;
        if(!logged_once)
        {
            logged_once = true;
            const QString err = volume_assist_.lastError();
            const QByteArray err_bytes = err.isEmpty() ? QByteArray("ensureReady failed") : err.toUtf8();
            LOG_WARNING("[OpenRGB3DSpatialPlugin] AudioPaintBrush volume assist unavailable: %s",
                        err_bytes.constData());
        }
    }
}

RGBColor AudioPaintBrush::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
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

    float c1 = 0.5f, c2 = 0.5f, c3 = 0.5f;
    if(!SampleGpuVolumeOriginLocal01(rotated_pos.x, rotated_pos.y, rotated_pos.z, grid, origin,
                                    GetNormalizedScale(), &c1, &c2, &c3))
        return 0x00000000;

    const QVector3D samp = volume_assist_.sample01(c1, c2, c3);
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

nlohmann::json AudioPaintBrush::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    j["thickness"] = thickness;
    j["chaos"] = chaos;
    j["ribbon_count"] = ribbon_count;
    return j;
}

void AudioPaintBrush::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    if(settings.contains("thickness") && settings["thickness"].is_number())
        thickness = std::clamp(settings["thickness"].get<float>(), 0.02f, 1.0f);
    if(settings.contains("chaos") && settings["chaos"].is_number())
        chaos = std::clamp(settings["chaos"].get<float>(), 0.0f, 1.5f);
    if(settings.contains("ribbon_count") && settings["ribbon_count"].is_number_integer())
        ribbon_count = std::clamp(settings["ribbon_count"].get<int>(), 1, 4);

    AudioReactiveUi::SyncSettingsToHost(GetCustomSettingsHost(), audio_settings);
    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "AudioPaintBrushEffectSettings"))
        {
            const auto pct = [](int v) { return QString::number(v) + QStringLiteral("%"); };
            EffectUiSync::setSliderValue(fx, "thicknessRow", (int)std::lround(thickness * 100.0f), pct);
            EffectUiSync::setSliderValue(fx, "chaosRow", (int)std::lround(chaos * 100.0f), pct);
            EffectUiSync::setSliderValue(fx, "ribbonsRow", ribbon_count, [](int v) { return QString::number(v); });
        }
    }
}
