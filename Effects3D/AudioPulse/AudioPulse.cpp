// SPDX-License-Identifier: GPL-2.0-only

#include "AudioPulse.h"
#include "AudioPulseVolumeFieldGlsl.h"
#include "AudioReactiveUi.h"
#include "PluginLog.h"
#include "SpatialLayerCore.h"
#include <QVBoxLayout>
#include <QByteArray>
#include <QVector3D>
#include "EffectUiRows.h"
#include "EffectUiSync.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
constexpr int kMaxPackedPulses = 5;
}

void AudioPulse::TickPulses(float time)
{
    if(std::fabs(time - last_tick_time) < 1e-5f)
        return;

    float dt = (last_tick_time == std::numeric_limits<float>::lowest()) ? 0.0f
               : std::clamp(time - last_tick_time, 0.0f, 0.1f);
    last_tick_time = time;

    AudioInputManager* audio = AudioInputManager::instance();
    float strength = 0.0f;
    if(audio->isRunning()
       && TryTriggerAudioPulse(dt,
                               audio_settings,
                               pulse_trigger,
                               onset_threshold,
                               AudioReactiveOnsetSmoothAlpha(audio_settings),
                               AudioReactiveBeatPulseHoldSec(),
                               strength))
    {
        PulseData p;
        p.birth_time = time;
        p.strength = strength;
        p.color_slot = next_pulse_color_slot++;
        pulses.push_back(p);
    }

    pulses.erase(std::remove_if(pulses.begin(), pulses.end(),
                                [this, time](const PulseData& p) {
                                    float age = time - p.birth_time;
                                    return age > 2.6f
                                           || AudioReactivePulseFade(
                                                  p.strength,
                                                  age,
                                                  BeatWaveShellDecay(audio_settings, 1.25f))
                                                  < 0.003f;
                                }),
                 pulses.end());
}

AudioPulse::AudioPulse(QWidget* parent)
    : SpatialEffect3D(parent)
{
    volume_assist_.setFragmentBody(QString::fromUtf8(AudioPulseVolumeFieldGlsl()));
    volume_assist_.setResolution(24);
}

EffectInfo3D AudioPulse::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Audio Pulse";
    info.effect_description =
        "Beat-triggered shockwaves from the origin (GPU volume field). Use Role + Hz to pick lows/mids/highs; "
        "enable Rainbow for full color range. Wave spread/fade control how far and how fast shells travel.";
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
    info.default_speed_scale = 35.0f;
    info.default_frequency_scale = 20.0f;
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

void AudioPulse::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    const auto on_changed = [this]() { emit ParametersChanged(); };

    AudioReactiveUi::AppendStandardFrequencyBandSection(layout, audio_settings, this, on_changed);

    AudioReactiveUi::AudioBeatUiOptions beat_opts;
    beat_opts.include_pulse_color = true;
    beat_opts.include_shell_falloff = true;
    beat_opts.include_spread_fade = true;
    AudioReactiveUi::AppendStandardBeatWaveSection(layout, audio_settings, this, on_changed, beat_opts);

    AudioReactiveUi::AudioResponseUiOptions response_opts;
    response_opts.use_onset_smoothing_label = true;
    AudioReactiveUi::AppendStandardResponseSection(layout, audio_settings, this, on_changed, response_opts);
    AudioReactiveUi::AppendBeatSensitivityRow(layout, onset_threshold, this, on_changed);

    QVBoxLayout* effect_body = EffectUiRows::AppendCollapsibleSectionBody(layout, QStringLiteral("Effect"));

    QWidget* effect_section = EffectUiRows::NewEffectPanel("AudioPulseEffectSettings");
    EffectSliderRow* surface_sparks_row = EffectUiRows::AppendSliderRow(
        EffectUiRows::PanelLayout(effect_section),
        QStringLiteral("Surface sparks:"),
        0,
        100,
        (int)particle_amount,
        QStringLiteral("Sparse spark particles on the shell (0 = smooth ring only)."));
    surface_sparks_row->setObjectName(QStringLiteral("surfaceSparksRow"));
    surface_sparks_row->bindValueChanged(
        this,
        [this](int v) { particle_amount = v; },
        [](int v) { return QString::number(v) + QStringLiteral("%"); },
        on_changed);
    if(effect_body)
        effect_body->addWidget(effect_section);
    else
        layout->addWidget(effect_section);

    AddWidgetToParent(w, parent);
}

void AudioPulse::PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid)
{
    TickPulses(time_sec);

    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(0.5f, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());

    const AudioBeatWaveMode wave_mode =
        static_cast<AudioBeatWaveMode>(audio_settings.beat_wave_mode);
    const float size_m = std::clamp(GetNormalizedSize(), 0.2f, 2.0f);
    const float detail = std::max(0.05f, GetScaledDetail());
    const float tm = std::clamp(bb.tight_mul, 0.25f, 4.0f);

    constexpr float kExplosionGridFill = 3.0f;
    float radius_basis =
        EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f * kExplosionGridFill;
    radius_basis = std::max(radius_basis, 1e-3f);

    float pulse_speed = 0.0f;
    float decay = BeatWaveShellDecay(audio_settings, 2.35f);
    float half_w =
        AudioRingHalfWidthFromFalloff(radius_basis, audio_settings.falloff, size_m, tm, detail);
    float max_travel_or_burst = radius_basis * (0.88f + 0.12f * size_m);
    if(wave_mode == AudioBeatWaveMode::ClassicWave)
    {
        /* Classic wave: scale speed once; slot [7] = burst phase cap. */
        pulse_speed = BeatWaveScaledSpeed((1.0f + GetScaledSpeed() * 0.2f) * bb.speed_mul, audio_settings);
        decay = BeatWaveShellDecay(audio_settings, 2.0f);
        max_travel_or_burst = BeatWaveBurstPhaseCap(audio_settings);
    }
    else
    {
        pulse_speed =
            BeatWaveScaledSpeed((0.42f + GetScaledSpeed() * 0.10f) * bb.speed_mul, audio_settings);
    }

    EffectGridAxisHalfExtents extents = MakeEffectGridAxisHalfExtents(grid, GetNormalizedScale());

    float vp[24] = {};
    vp[0] = (float)audio_settings.beat_wave_mode;
    vp[1] = size_m;
    vp[2] = detail;
    vp[3] = std::max(0.25f, audio_settings.falloff);
    vp[4] = pulse_speed;
    vp[5] = radius_basis;
    vp[6] = half_w;
    vp[7] = max_travel_or_burst;
    vp[8] = std::clamp(particle_amount / 100.0f, 0.0f, 1.0f);
    vp[9] = decay;
    vp[10] = std::max(1e-5f, extents.hw);
    vp[11] = std::max(1e-5f, extents.hh);
    vp[12] = std::max(1e-5f, extents.hd);
    vp[13] = tm;

    packed_pulse_count = 0;
    for(int i = 0; i < kMaxPackedPulses; ++i)
    {
        vp[14 + i * 2] = -1.0f;
        vp[15 + i * 2] = 0.0f;
        packed_color_slots[i] = 0;
    }

    /* Prefer newest pulses when more than 5 are alive. */
    const int start = std::max(0, (int)pulses.size() - kMaxPackedPulses);
    for(int pi = start; pi < (int)pulses.size() && packed_pulse_count < kMaxPackedPulses; ++pi)
    {
        const PulseData& p = pulses[pi];
        const float age = time_sec - p.birth_time;
        if(age < 0.0f || p.strength <= 0.0f)
            continue;
        const int slot = packed_pulse_count++;
        vp[14 + slot * 2] = age;
        vp[15 + slot * 2] = p.strength;
        packed_color_slots[slot] = p.color_slot;
    }

    if(!volume_assist_.prepare(render_sequence, time_sec, vp, 24))
    {
        static bool logged_once = false;
        if(!logged_once)
        {
            logged_once = true;
            const QString err = volume_assist_.lastError();
            const QByteArray err_bytes = err.isEmpty() ? QByteArray("ensureReady failed") : err.toUtf8();
            LOG_WARNING("[OpenRGB3DSpatialPlugin] AudioPulse volume assist unavailable: %s",
                        err_bytes.constData());
        }
    }
}

RGBColor AudioPulse::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
        return 0x00000000;

    Vector3D o = GetEffectOriginGrid(grid);
    float rel_x = x - o.x, rel_y = y - o.y, rel_z = z - o.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    if(!volume_assist_.isAvailable())
        return 0x00000000;

    Vector3D rotated_pos{x, y, z};
    float coord2 = NormalizeGridAxis01(rotated_pos.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(sw, grid, x, y, z, o, time);

    float c1 = 0.5f, c2 = 0.5f, c3 = 0.5f;
    SampleGpuVolumeOriginLocal01(rotated_pos.x, rotated_pos.y, rotated_pos.z, grid, o,
                                 GetNormalizedScale(), &c1, &c2, &c3);
    const QVector3D samp = volume_assist_.sample01(c1, c2, c3);
    float energy = samp.x();
    const float pulse_idx01 = samp.y();
    const float gradient_pos = samp.z();

    if(GetStratumLayoutMode() == 1)
        energy = EffectStratumBlend::ApplyMotionToUnit01(energy, stratum_mot01, 0.18f);

    const bool classic_wave =
        static_cast<AudioBeatWaveMode>(audio_settings.beat_wave_mode) == AudioBeatWaveMode::ClassicWave;
    constexpr float kShellSilenceEpsClassic = 0.006f;
    constexpr float kShellSilenceEpsRing = 0.012f;
    const float silence_eps = classic_wave ? kShellSilenceEpsClassic : kShellSilenceEpsRing;
    if(energy <= silence_eps)
        return 0x00000000;

    const float display_energy =
        std::min(1.0f, energy * (classic_wave ? 1.45f : 1.22f));

    int pulse_slot = (int)std::floor(pulse_idx01 * 5.0f);
    pulse_slot = std::clamp(pulse_slot, 0, kMaxPackedPulses - 1);
    const uint32_t color_slot =
        (packed_pulse_count > 0) ? packed_color_slots[pulse_slot] : 0u;

    AudioReactiveColorParams color_params;
    color_params.gradient_pos01 = 1.0f - std::clamp(gradient_pos, 0.0f, 1.0f);
    color_params.intensity = display_energy;
    color_params.beat_color_slot = color_slot;
    color_params.time = time;
    color_params.grid_x = x;
    color_params.grid_y = y;
    color_params.grid_z = z;
    color_params.grid = &grid;
    color_params.origin = o;
    color_params.rotated_pos = rotated_pos;
    color_params.y_norm01 = coord2;
    color_params.stratum_mot01 = stratum_mot01;
    color_params.band_scalars = &bb;

    RGBColor pulse_color = ResolveAudioReactiveColor(audio_settings, color_params);
    return BrightenAudioEffectColor(pulse_color, display_energy);
}

nlohmann::json AudioPulse::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    j["onset_threshold"] = onset_threshold;
    j["particle_amount"] = particle_amount;
    return j;
}

void AudioPulse::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    if(settings.contains("onset_threshold"))
        onset_threshold = std::clamp(settings["onset_threshold"].get<float>(), 0.05f, 0.95f);
    if(settings.contains("particle_amount"))
        particle_amount = std::clamp(settings["particle_amount"].get<int>(), 0, 100);
    pulses.clear();
    pulse_trigger = {};
    last_tick_time = std::numeric_limits<float>::lowest();
    next_pulse_color_slot = 0;
    packed_pulse_count = 0;

    AudioReactiveUi::SyncSettingsToHost(GetCustomSettingsHost(), audio_settings);
    const auto pct = [](int v) { return QString::number(v) + QStringLiteral("%"); };
    EffectUiSync::setSliderByCaption(GetCustomSettingsHost(), QStringLiteral("Beat trigger:"),
                                     (int)(onset_threshold * 100.0f), pct);
    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "AudioPulseEffectSettings"))
            EffectUiSync::setSliderValue(fx, "surfaceSparksRow", particle_amount, pct);
    }
}

REGISTER_EFFECT_3D(AudioPulse)
