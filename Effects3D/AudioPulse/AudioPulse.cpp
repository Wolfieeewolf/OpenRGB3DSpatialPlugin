// SPDX-License-Identifier: GPL-2.0-only

#include "AudioPulse.h"
#include "AudioReactiveUi.h"
#include "SpatialKernelColormap.h"
#include "SpatialLayerCore.h"
#include "../EffectHelpers.h"
#include <QVBoxLayout>
#include "EffectUiRows.h"
#include "EffectUiSync.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float AudioPulse::Hash01(float x, float y, float z, unsigned int salt)
{
    unsigned int sx = (unsigned int)((x + 1e6f) * 100.0f);
    unsigned int sy = (unsigned int)((y + 1e6f) * 100.0f);
    unsigned int sz = (unsigned int)((z + 1e6f) * 100.0f);
    unsigned int v = sx * 73856093u ^ sy * 19349663u ^ sz * 83492791u ^ salt * 2654435761u;
    v = (v << 13u) ^ v;
    v = v * (v * v * 15731u + 789221u) + 1376312589u;
    return ((v & 0xFFFFu) / 65535.0f);
}

float AudioPulse::HeightFade01(float height_norm) const
{
    float height_fade = 1.0f - std::clamp(height_norm, 0.0f, 1.0f);
    if(UseSpatialRoomTint())
    {
        SpatialLayerCore::MapperSettings stratum_map;
        stratum_map.floor_end = 0.30f;
        stratum_map.desk_end = 0.55f;
        stratum_map.upper_end = 0.78f;
        stratum_map.blend_softness = 0.10f;
        float layer_w[SpatialLayerCore::kMaxLayerCount]{};
        SpatialLayerCore::ComputeVerticalStratumWeights(height_norm, stratum_map, 3, layer_w);
        height_fade = 0.55f + 0.45f * (layer_w[0] * 1.0f + layer_w[1] * 0.72f + layer_w[2] * 0.48f);
    }
    return std::clamp(height_fade, 0.15f, 1.0f);
}

void AudioPulse::TickPulses(float time)
{
    if(std::fabs(time - last_tick_time) < 1e-5f)
    {
        return;
    }
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

float AudioPulse::ParticleDebrisAt(float x,
                                   float y,
                                   float z,
                                   float burst_phase,
                                   float distance,
                                   float shell_radius,
                                   int salt) const
{
    if(particle_amount <= 0)
    {
        return 0.0f;
    }
    float h = Hash01(x, y, z, (unsigned int)(burst_phase * 1000.0f) + (unsigned int)salt * 131u);
    if(h > (float)particle_amount / 100.0f)
    {
        return 0.0f;
    }
    float spread = shell_radius * (0.35f + 0.65f * std::min(1.0f, burst_phase));
    float falloff = expf(-fabsf(distance - spread * 0.85f) * 0.12f);
    return falloff * (0.45f + 0.55f * h) * ((float)particle_amount / 100.0f);
}

BeatShellHit AudioPulse::SampleBeatShell(float distance,
                                         float radius_basis,
                                         float time,
                                         const EffectStratumBlend::BandBlendScalars& bb,
                                         float stratum_mot01,
                                         float height_fade,
                                         float x,
                                         float y,
                                         float z) const
{
    const float size_m = std::clamp(GetNormalizedSize(), 0.2f, 2.0f);
    const float detail = std::max(0.05f, GetScaledDetail());
    const float tm = std::clamp(bb.tight_mul, 0.25f, 4.0f);
    const AudioBeatWaveMode wave_mode =
        static_cast<AudioBeatWaveMode>(audio_settings.beat_wave_mode);

    if(wave_mode == AudioBeatWaveMode::ClassicWave)
    {
        const float pulse_speed = (1.0f + GetScaledSpeed() * 0.2f) * bb.speed_mul;
        float total = 0.0f;
        float best_contrib = 0.0f;
        BeatShellHit hit;
        for(size_t pi = 0; pi < pulses.size(); ++pi)
        {
            const PulseData& p = pulses[pi];
            const float age = time - p.birth_time;
            if(age < 0.0f)
            {
                continue;
            }

            float shell = SampleClassicExplosionBeatShell(distance,
                                                          radius_basis,
                                                          age,
                                                          pulse_speed,
                                                          audio_settings.falloff,
                                                          size_m,
                                                          detail,
                                                          tm,
                                                          bb,
                                                          stratum_mot01,
                                                          audio_settings);

            const float burst_phase =
                std::min(BeatWaveBurstPhaseCap(audio_settings), age * BeatWaveScaledSpeed(pulse_speed, audio_settings) * 0.55f);
            const float explosion_radius =
                burst_phase * radius_basis * (0.14f + 0.86f * size_m);
            float debris = 0.0f;
            if(particle_amount > 0)
            {
                debris = std::max(
                    ParticleDebrisAt(x, y, z, burst_phase, distance, explosion_radius, 1),
                    ParticleDebrisAt(x, y, z, burst_phase * 0.97f, distance, explosion_radius * 0.92f, 2));
            }

            const float fade =
                p.strength * std::exp(-BeatWaveShellDecay(audio_settings, 2.0f) * age);
            const float contrib = fade * height_fade * std::max(shell, debris);
            total += contrib;
            if(contrib > best_contrib)
            {
                best_contrib = contrib;
                hit.pulse_index = pi;
            }
        }
        hit.energy = std::clamp(total, 0.0f, 1.0f);
        return hit;
    }

    const float pulse_speed =
        BeatWaveScaledSpeed((0.42f + GetScaledSpeed() * 0.10f) * bb.speed_mul, audio_settings);
    const float half_w =
        AudioRingHalfWidthFromFalloff(radius_basis, audio_settings.falloff, size_m, tm, detail);
    const float max_travel = radius_basis * (0.88f + 0.12f * size_m);
    BeatShellHit hit;
    for(size_t pi = 0; pi < pulses.size(); ++pi)
    {
        const PulseData& p = pulses[pi];
        float age = time - p.birth_time;
        if(age < 0.0f)
        {
            continue;
        }

        const float ring_age = BeatWaveRingAgeSec(age, wave_mode);
        float ring_radius = ring_age * pulse_speed * max_travel;
        if(BeatWaveUsesRing(wave_mode) && ring_radius > max_travel * 1.04f)
        {
            continue;
        }
        if(!BeatWaveUsesRing(wave_mode) && age > 0.35f)
        {
            continue;
        }

        float shell = 0.0f;
        if(BeatWaveUsesRing(wave_mode))
        {
            AudioExpandingRingParams ring;
            ring.coord = distance;
            ring.ring_radius = ring_radius;
            ring.half_width = half_w;
            shell = SampleAudioExpandingRingBand(ring);
        }
        shell = SampleBeatWaveShell(wave_mode, age, distance, ring_radius, half_w, shell, audio_settings);

        if(particle_amount > 0 && BeatWaveUsesRing(wave_mode)
           && std::fabs(distance - ring_radius) < half_w * 2.0f && shell > 0.02f)
        {
            const float burst_phase = ring_radius / std::max(max_travel, 1e-4f);
            float debris = std::max(ParticleDebrisAt(x, y, z, burst_phase, distance, ring_radius, 1),
                                    ParticleDebrisAt(x, y, z, burst_phase * 0.97f, distance, ring_radius, 2));
            debris *= shell;
            shell = std::max(shell, debris);
        }

        const float fade =
            p.strength * std::exp(-BeatWaveShellDecay(audio_settings, 2.35f) * age);
        const float contrib = fade * height_fade * shell;
        if(contrib > hit.energy)
        {
            hit.energy = contrib;
            hit.pulse_index = pi;
        }
    }
    hit.energy = std::clamp(hit.energy * 1.28f, 0.0f, 1.0f);
    return hit;
}

AudioPulse::AudioPulse(QWidget* parent)
    : SpatialEffect3D(parent)
{
}

EffectInfo3D AudioPulse::GetEffectInfo() const
{
    EffectInfo3D info{};
    info.effect_name = "Audio Pulse";
    info.effect_description =
        "Beat-triggered shockwaves from the origin: classic layered pulse (default) or expanding-ring / flash modes; "
        "optional surface sparks on the wave front";
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
    AudioReactiveUi::AppendStandardDriveSection(layout, audio_settings, this, on_changed);

    AudioReactiveUi::AudioBeatUiOptions beat_opts;
    beat_opts.include_pulse_color = true;
    beat_opts.include_shell_falloff = true;
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
    {
        effect_body->addWidget(effect_section);
    }
    else
    {
        layout->addWidget(effect_section);
    }

    AddWidgetToParent(w, parent);
}

RGBColor AudioPulse::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
    {
        return 0x00000000;
    }

    TickPulses(time);

    Vector3D o = GetEffectOriginGrid(grid);
    Vector3D rotated_pos = TransformPointByRotation(x, y, z, o);
    float coord2 = NormalizeGridAxis01(rotated_pos.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(GetStratumLayoutMode(), sw, GetStratumTuning());
    const float stratum_mot01 =
        ComputeStratumMotion01(sw, grid, x, y, z, o, time);


    float dx = rotated_pos.x - o.x;
    float dy = rotated_pos.y - o.y;
    float dz = rotated_pos.z - o.z;
    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    constexpr float kExplosionGridFill = 3.0f;
    float radius_basis =
        EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f * kExplosionGridFill;
    radius_basis = std::max(radius_basis, 1e-3f);

    float height_fade = HeightFade01(coord2);
    const BeatShellHit hit =
        SampleBeatShell(distance, radius_basis, time, bb, stratum_mot01, height_fade, x, y, z);

    const bool classic_wave =
        static_cast<AudioBeatWaveMode>(audio_settings.beat_wave_mode) == AudioBeatWaveMode::ClassicWave;
    constexpr float kShellSilenceEpsClassic = 0.006f;
    constexpr float kShellSilenceEpsRing = 0.012f;
    const float silence_eps = classic_wave ? kShellSilenceEpsClassic : kShellSilenceEpsRing;
    if(!hit.valid() || hit.energy <= silence_eps)
    {
        return 0x00000000;
    }

    const float display_energy =
        std::min(1.0f, hit.energy * (classic_wave ? 1.45f : 1.22f));
    float gradient_pos = std::clamp(distance / radius_basis, 0.0f, 1.0f);

    AudioReactiveColorParams color_params;
    color_params.gradient_pos01 = 1.0f - gradient_pos;
    color_params.intensity = display_energy;
    color_params.beat_color_slot = pulses[hit.pulse_index].color_slot;
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
    {
        onset_threshold = std::clamp(settings["onset_threshold"].get<float>(), 0.05f, 0.95f);
    }
    if(settings.contains("particle_amount"))
    {
        particle_amount = std::clamp(settings["particle_amount"].get<int>(), 0, 100);
    }
    pulses.clear();
    pulse_trigger = {};
    last_tick_time = std::numeric_limits<float>::lowest();
    next_pulse_color_slot = 0;

    AudioReactiveUi::SyncSettingsToHost(GetCustomSettingsHost(), audio_settings, this);
    const auto pct = [](int v) { return QString::number(v) + QStringLiteral("%"); };
    EffectUiSync::setSliderByCaption(GetCustomSettingsHost(), QStringLiteral("Beat sensitivity:"),
                                     (int)(onset_threshold * 100.0f), pct);
    if(QWidget* panel = CustomSettingsPanelWidget())
    {
        if(QWidget* fx = EffectUiSync::effectPanel(panel, "AudioPulseEffectSettings"))
        {
            EffectUiSync::setSliderValue(fx, "surfaceSparksRow", particle_amount, pct);
        }
    }
}

REGISTER_EFFECT_3D(AudioPulse)
