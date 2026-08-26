// SPDX-License-Identifier: GPL-2.0-only

#include "NoteSparkle.h"
#include "NoteSparkleVolumeFieldGlsl.h"
#include "AudioReactiveUi.h"
#include "EffectSliderRow.h"
#include "EffectUiRows.h"
#include "EffectUiSync.h"
#include "PluginLog.h"
#include "SpatialLayerCore.h"
#include <QVBoxLayout>
#include <QByteArray>
#include <QVector3D>
#include <algorithm>
#include <cmath>

namespace
{
constexpr int kMaxPackedNotes = 4;
}

float NoteSparkle::EvaluateDrive(float amplitude, float time)
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
    return ApplyAudioVisualIntensity(smoothed, audio_settings);
}

NoteSparkle::NoteSparkle(QWidget* parent)
    : SpatialEffect3D(parent)
{
    audio_settings = MakeDefaultHighSparkleAudioReactiveSettings3D();
    volume_assist_.setFragmentBody(QString::fromUtf8(NoteSparkleVolumeFieldGlsl()));
    volume_assist_.setResolution(22);
}

EffectInfo3D NoteSparkle::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Note Sparkle";
    info.effect_description =
        "ColorChord note particle cloud (hull + sparks). Defaults to High listen role — change Role/Hz "
        "freely. Assign strip, zone, or room on the stack. Pair with Bass Punch on lows.";
    info.category = "Audio";
    info.effect_type = SPATIAL_EFFECT_NOTE_SPARKLE;
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
    info.default_speed_scale = 18.0f;
    info.default_frequency_scale = 22.0f;
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

void NoteSparkle::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto on_changed = [this]() { emit ParametersChanged(); };

    AudioReactiveUi::AppendStandardFrequencyBandSection(layout, audio_settings, this, on_changed);

    QVBoxLayout* effect_body = EffectUiRows::AppendCollapsibleSectionBody(layout, QStringLiteral("Effect"));
    QWidget* effect_section = EffectUiRows::NewEffectPanel("NoteSparkleEffectSettings");
    QVBoxLayout* effect_layout = EffectUiRows::PanelLayout(effect_section);
    const auto pct_format = [](int v) { return QString::number(v) + QStringLiteral("%"); };

    EffectSliderRow* particle_row = EffectUiRows::AppendSliderRow(
        effect_layout,
        QStringLiteral("Particle amount:"),
        0,
        100,
        (int)std::lround(particle_amount * 100.0f),
        QStringLiteral("Density of the note sparkle cloud (0 = soft hull only)."));
    particle_row->setObjectName(QStringLiteral("particleAmountRow"));
    particle_row->bindValueChanged(
        this, [this](int v) { particle_amount = v / 100.0f; }, pct_format, on_changed);

    EffectSliderRow* turb_row = EffectUiRows::AppendSliderRow(
        effect_layout,
        QStringLiteral("Turbulence:"),
        0,
        100,
        (int)std::lround(turbulence * 100.0f),
        QStringLiteral("Spiral / curl motion on the particle cloud."));
    turb_row->setObjectName(QStringLiteral("turbulenceRow"));
    turb_row->bindValueChanged(
        this, [this](int v) { turbulence = v / 100.0f; }, pct_format, on_changed);

    EffectSliderRow* hull_row = EffectUiRows::AppendSliderRow(
        effect_layout,
        QStringLiteral("Hull size:"),
        8,
        72,
        (int)std::lround(hull_size * 100.0f),
        QStringLiteral("Radius of the soft shell the particles cling to (scales with zone)."));
    hull_row->setObjectName(QStringLiteral("hullSizeRow"));
    hull_row->bindValueChanged(
        this, [this](int v) { hull_size = v / 100.0f; }, pct_format, on_changed);

    if(effect_body)
        effect_body->addWidget(effect_section);
    else
        layout->addWidget(effect_section);

    AudioReactiveUi::AudioResponseUiOptions response_opts;
    response_opts.include_falloff = true;
    response_opts.falloff_label = QStringLiteral("Shell edge:");
    response_opts.falloff_slider_max = 500;
    response_opts.falloff_tooltip =
        QStringLiteral("Thickness / steepness of the hull and particle glow.");
    AudioReactiveUi::AppendStandardResponseSection(layout, audio_settings, this, on_changed, response_opts);
    AudioReactiveUi::AppendAudioSectionBody(layout, QStringLiteral("Color"));
    AudioReactiveUi::AppendAudioPulseColorModeRow(layout, audio_settings, this, on_changed);

    AddWidgetToParent(w, parent);
}

void NoteSparkle::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& /*grid*/)
{
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(0.5f, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());

    const float amplitude = SampleAudioVisualLevel(audio_settings);
    const float drive = EvaluateDrive(amplitude, time_sec);

    AudioInputManager* audio = AudioInputManager::instance();
    float low01 = 0.0f;
    float mid01 = 0.0f;
    float high01 = 0.0f;
    float note_hues[kMaxPackedNotes] = {};
    float note_amps[kMaxPackedNotes] = {};
    int note_count = 0;
    if(audio && audio->isRunning())
    {
        low01 = std::clamp(audio->getBandSlowEnergyHz(40.0f, 180.0f), 0.0f, 1.0f);
        mid01 = std::clamp(audio->getBandSlowEnergyHz(200.0f, 4000.0f), 0.0f, 1.0f);
        high01 = std::clamp(
            std::max(audio->getNoteDrive01(), audio->getBandSlowEnergyHz(2800.0f, 14000.0f)),
            0.0f,
            1.0f);
        const auto notes = audio->getActiveNotes();
        for(const auto& n : notes)
        {
            if(note_count >= kMaxPackedNotes)
                break;
            if(n.amp < 0.04f)
                continue;
            float mean = n.mean;
            if(mean < 0.0f)
                mean += 12.0f;
            if(mean >= 12.0f)
                mean -= 12.0f;
            note_hues[note_count] = std::clamp(mean / 12.0f, 0.0f, 1.0f);
            note_amps[note_count] = std::clamp(n.amp, 0.0f, 1.0f);
            ++note_count;
        }
        if(note_count == 0 && audio->getNoteDrive01() > 0.05f)
        {
            note_hues[0] = audio->getDominantNoteHue01();
            note_amps[0] = audio->getNoteDrive01();
            note_count = 1;
        }
    }

    const float size_m = std::max(0.35f, GetNormalizedSize());
    const float detail = std::max(0.05f, GetScaledDetail());
    const float time_e = time_sec * bb.speed_mul * (0.35f + GetScaledSpeed() * 0.08f);

    float vp[24] = {};
    vp[0] = drive;
    vp[1] = std::clamp(particle_amount, 0.0f, 1.0f);
    vp[2] = std::clamp(turbulence, 0.0f, 1.0f);
    vp[3] = std::clamp(hull_size, 0.08f, 0.72f);
    vp[4] = low01;
    vp[5] = mid01;
    vp[6] = high01;
    vp[7] = std::max(0.25f, audio_settings.falloff);
    vp[8] = size_m;
    vp[9] = detail;
    vp[10] = std::max(0.25f, bb.tight_mul);
    vp[11] = std::max(0.15f, bb.speed_mul);
    for(int i = 0; i < kMaxPackedNotes; ++i)
    {
        vp[12 + i * 2] = note_hues[i];
        vp[13 + i * 2] = note_amps[i];
    }
    vp[20] = (float)note_count;
    vp[21] = time_e;

    if(!volume_assist_.prepare(render_sequence, time_sec, vp, 24))
    {
        static bool logged_once = false;
        if(!logged_once)
        {
            logged_once = true;
            const QString err = volume_assist_.lastError();
            const QByteArray err_bytes = err.isEmpty() ? QByteArray("ensureReady failed") : err.toUtf8();
            LOG_WARNING("[OpenRGB3DSpatialPlugin] NoteSparkle volume assist unavailable: %s",
                        err_bytes.constData());
        }
    }
}

RGBColor NoteSparkle::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
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
    const float note_hue01 = std::clamp(samp.y(), 0.0f, 1.0f);
    const float gradient_pos = samp.z();

    if(GetStratumLayoutMode() == 1)
        intensity = EffectStratumBlend::ApplyMotionToUnit01(intensity, stratum_mot01, 0.18f);
    if(intensity <= 0.004f)
        return 0x00000000;

    const auto mode = static_cast<AudioPulseColorMode>(audio_settings.pulse_color_mode);
    RGBColor color;
    if(GetRainbowMode()
       && mode != AudioPulseColorMode::PerBeatCycle
       && mode != AudioPulseColorMode::Uniform)
    {
        /* Rainbow checkbox = spatial rainbow along the cloud. */
        color = GetRainbowColor(std::clamp(gradient_pos, 0.0f, 1.0f) * 360.0f);
    }
    else if(mode == AudioPulseColorMode::FollowNotes)
    {
        /* Per-LED ColorChord hue — true HSV, not red↔blue palette lerp. */
        color = GetRainbowColor(note_hue01 * 360.0f);
    }
    else
    {
        AudioReactiveColorParams color_params;
        color_params.gradient_pos01 = gradient_pos;
        color_params.intensity = intensity;
        color_params.beat_color_slot = (uint32_t)std::floor(note_hue01 * 12.0f);
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
        color = ResolveAudioReactiveColor(audio_settings, color_params);
    }
    return BrightenAudioEffectColor(color, intensity);
}

nlohmann::json NoteSparkle::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    j["particle_amount"] = particle_amount;
    j["turbulence"] = turbulence;
    j["hull_size"] = hull_size;
    return j;
}

void NoteSparkle::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);

    auto load01 = [&](const char* key, float& dst, float lo, float hi) {
        if(!settings.contains(key))
            return;
        float v = settings[key].get<float>();
        if(v > 1.0f)
            v *= 0.01f;
        dst = std::clamp(v, lo, hi);
    };
    load01("particle_amount", particle_amount, 0.0f, 1.0f);
    load01("turbulence", turbulence, 0.0f, 1.0f);
    load01("hull_size", hull_size, 0.08f, 0.72f);

    smoothed = 0.0f;
    last_intensity_time = std::numeric_limits<float>::lowest();

    AudioReactiveUi::SyncSettingsToHost(GetCustomSettingsHost(), audio_settings);
    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "NoteSparkleEffectSettings"))
        {
            const auto pct = [](int v) { return QString::number(v) + QStringLiteral("%"); };
            EffectUiSync::setSliderValue(fx, "particleAmountRow", (int)std::lround(particle_amount * 100.0f), pct);
            EffectUiSync::setSliderValue(fx, "turbulenceRow", (int)std::lround(turbulence * 100.0f), pct);
            EffectUiSync::setSliderValue(fx, "hullSizeRow", (int)std::lround(hull_size * 100.0f), pct);
        }
    }
}

REGISTER_EFFECT_3D(NoteSparkle)
