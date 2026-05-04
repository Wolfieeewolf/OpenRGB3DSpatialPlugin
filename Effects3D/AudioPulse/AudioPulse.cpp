// SPDX-License-Identifier: GPL-2.0-only

#include "AudioPulse.h"
#include "SpatialKernelColormap.h"
#include "StripKernelColormapPanel.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"
#include "../EffectHelpers.h"
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <QHBoxLayout>
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
    float onset_raw = audio->getOnsetLevel();
    float a = std::clamp(audio_settings.smoothing, 0.0f, 0.95f);
    onset_smoothed = a * onset_smoothed + (1.0f - a) * onset_raw;

    if(onset_hold > 0.0f)
    {
        onset_hold = std::max(0.0f, onset_hold - dt);
    }

    /* Require real band energy so spectral-flux / FFT noise does not spawn rings in silence. */
    constexpr float kMinBandForPulse = 0.042f;
    constexpr float kMinPulseStrength = 0.018f;
    if(audio->isRunning() && onset_hold <= 0.0f && onset_smoothed >= onset_threshold)
    {
        float amp = audio->getBandEnergyHz((float)audio_settings.low_hz, (float)audio_settings.high_hz);
        if(amp >= kMinBandForPulse)
        {
            float strength = ApplyAudioIntensity(std::clamp(amp, 0.0f, 1.0f), audio_settings);
            if(strength >= kMinPulseStrength)
            {
                PulseData p;
                p.birth_time = time;
                p.strength = strength;
                pulses.push_back(p);
                onset_hold = 0.12f;
            }
        }
    }

    pulses.erase(std::remove_if(pulses.begin(), pulses.end(), [time](const PulseData& p) {
        float age = time - p.birth_time;
        return age > 2.5f || p.strength * std::exp(-2.0f * age) < 0.003f;
    }), pulses.end());
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

float AudioPulse::SampleBeatShell(float distance,
                                  float radius_basis,
                                  float time,
                                  const EffectStratumBlend::BandBlendScalars& bb,
                                  float height_fade,
                                  float x,
                                  float y,
                                  float z) const
{
    float pulse_speed = (1.0f + GetScaledSpeed() * 0.2f) * bb.speed_mul;
    float size_m = GetNormalizedSize();
    float detail = std::max(0.05f, GetScaledDetail());
    float tm = std::clamp(bb.tight_mul, 0.25f, 4.0f);
    /* Narrower shells when falloff (audio UI) is higher — matches “tight ring” feel */
    float wave_thickness =
        radius_basis * (0.02f + 0.09f / std::max(0.2f, audio_settings.falloff)) * size_m;
    wave_thickness /= std::max(0.25f, tm);
    wave_thickness *= std::clamp(0.85f + 0.15f * detail, 0.7f, 1.15f);

    float freq_rip = detail * bb.tight_mul * 0.08f / std::max(0.08f, size_m);

    float total = 0.0f;
    for(const PulseData& p : pulses)
    {
        float age = time - p.birth_time;
        if(age < 0.0f)
        {
            continue;
        }
        float burst_phase = std::min(1.35f, age * pulse_speed * 0.55f);
        float explosion_radius =
            burst_phase * radius_basis * (0.14f + 0.86f * std::clamp(size_m, 0.2f, 2.0f));

        float primary =
            1.0f - smoothstep(explosion_radius - wave_thickness, explosion_radius + wave_thickness, distance);
        primary *= expf(-fabsf(distance - explosion_radius) *
                        (7.0f / std::max(wave_thickness, radius_basis * 0.0015f)));

        float secondary_radius = explosion_radius * 0.68f;
        float secondary =
            1.0f - smoothstep(secondary_radius - wave_thickness * 0.45f,
                              secondary_radius + wave_thickness * 0.55f, distance);
        secondary *= expf(-fabsf(distance - secondary_radius) * 0.11f) * 0.62f;

        float shock = 0.22f * sinf(distance * freq_rip * 10.0f - burst_phase * 6.283f +
                                   bb.phase_deg * (float)(M_PI / 180.0f));
        shock *= expf(-distance * 0.065f);

        float core = (distance < explosion_radius * 0.24f)
            ? (1.0f - distance / (explosion_radius * 0.24f + 1e-4f)) * 0.4f
            : 0.0f;

        float shell = std::min(1.0f, primary + secondary + shock + core);

        float debris = std::max(ParticleDebrisAt(x, y, z, burst_phase, distance, explosion_radius, 1),
                                ParticleDebrisAt(x, y, z, burst_phase * 0.97f, distance, explosion_radius * 0.92f, 2));

        float fade = p.strength * expf(-2.0f * age);
        total += fade * height_fade * std::max(shell, debris);
    }
    return std::clamp(total, 0.0f, 1.0f);
}

AudioPulse::AudioPulse(QWidget* parent)
    : SpatialEffect3D(parent)
{
}

EffectInfo3D AudioPulse::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 4;
    info.effect_name = "Audio Pulse";
    info.effect_description =
        "Beat-triggered shockwaves: thin expanding shells (explosion-style) with optional surface sparks; "
        "onset detector fires new rings from the effect origin";
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
    return info;
}

void AudioPulse::SetupCustomUI(QWidget* parent)
{
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(parent->layout());
    if(!layout)
    {
        layout = new QVBoxLayout(parent);
    }

    QHBoxLayout* smooth_row = new QHBoxLayout();
    smooth_row->addWidget(new QLabel("Onset smoothing:"));
    QSlider* smooth_slider = new QSlider(Qt::Horizontal);
    smooth_slider->setRange(0, 95);
    smooth_slider->setToolTip("Smooths the onset detector input (higher = fewer false triggers).");
    smooth_slider->setValue((int)(audio_settings.smoothing * 100.0f));
    QLabel* smooth_label = new QLabel(QString::number(audio_settings.smoothing, 'f', 2));
    smooth_label->setMinimumWidth(36);
    smooth_row->addWidget(smooth_slider);
    smooth_row->addWidget(smooth_label);
    layout->addLayout(smooth_row);

    connect(smooth_slider, &QSlider::valueChanged, this, [this, smooth_label](int v) {
        audio_settings.smoothing = v / 100.0f;
        smooth_label->setText(QString::number(audio_settings.smoothing, 'f', 2));
        emit ParametersChanged();
    });

    QHBoxLayout* falloff_row = new QHBoxLayout();
    falloff_row->addWidget(new QLabel("Shell thickness:"));
    QSlider* falloff_slider = new QSlider(Qt::Horizontal);
    falloff_slider->setRange(20, 800);
    falloff_slider->setToolTip("Thickness of each expanding shell (like explosion wave width).");
    falloff_slider->setValue((int)(audio_settings.falloff * 100.0f));
    QLabel* falloff_label = new QLabel(QString::number(audio_settings.falloff, 'f', 1));
    falloff_label->setMinimumWidth(36);
    falloff_row->addWidget(falloff_slider);
    falloff_row->addWidget(falloff_label);
    layout->addLayout(falloff_row);

    connect(falloff_slider, &QSlider::valueChanged, this, [this, falloff_label](int v) {
        audio_settings.falloff = v / 100.0f;
        falloff_label->setText(QString::number(audio_settings.falloff, 'f', 1));
        emit ParametersChanged();
    });

    QHBoxLayout* boost_row = new QHBoxLayout();
    boost_row->addWidget(new QLabel("Peak Boost:"));
    QSlider* boost_slider = new QSlider(Qt::Horizontal);
    boost_slider->setRange(50, 500);
    boost_slider->setToolTip("Gain on band energy when a beat spawns a shell.");
    boost_slider->setValue((int)(audio_settings.peak_boost * 100.0f));
    QLabel* boost_label = new QLabel(QString::number(audio_settings.peak_boost, 'f', 2) + "x");
    boost_label->setMinimumWidth(44);
    boost_row->addWidget(boost_slider);
    boost_row->addWidget(boost_label);
    layout->addLayout(boost_row);

    connect(boost_slider, &QSlider::valueChanged, this, [this, boost_label](int v) {
        audio_settings.peak_boost = v / 100.0f;
        boost_label->setText(QString::number(audio_settings.peak_boost, 'f', 2) + "x");
        emit ParametersChanged();
    });

    QHBoxLayout* sens_row = new QHBoxLayout();
    sens_row->addWidget(new QLabel("Beat sensitivity:"));
    QSlider* sens_slider = new QSlider(Qt::Horizontal);
    sens_slider->setRange(5, 92);
    sens_slider->setToolTip("Higher = fewer, stronger beat triggers (onset threshold).");
    sens_slider->setValue((int)(onset_threshold * 100.0f));
    QLabel* sens_label = new QLabel(QString::number((int)(onset_threshold * 100)) + "%");
    sens_label->setMinimumWidth(40);
    sens_row->addWidget(sens_slider);
    sens_row->addWidget(sens_label);
    layout->addLayout(sens_row);
    connect(sens_slider, &QSlider::valueChanged, this, [this, sens_label](int v) {
        onset_threshold = v / 100.0f;
        sens_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });

    QHBoxLayout* part_row = new QHBoxLayout();
    part_row->addWidget(new QLabel("Surface sparks:"));
    QSlider* part_slider = new QSlider(Qt::Horizontal);
    part_slider->setRange(0, 100);
    part_slider->setToolTip("Sparse spark particles on the shell (0 = smooth ring only, like wave surface).");
    part_slider->setValue(particle_amount);
    QLabel* part_label = new QLabel(QString::number(particle_amount) + "%");
    part_label->setMinimumWidth(36);
    part_row->addWidget(part_slider);
    part_row->addWidget(part_label);
    layout->addLayout(part_row);
    connect(part_slider, &QSlider::valueChanged, this, [this, part_label](int v) {
        particle_amount = v;
        part_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });

    strip_cmap_panel = new StripKernelColormapPanel(parent);
    strip_cmap_panel->mirrorStateFromEffect(audiopulse_strip_cmap_on,
                                            audiopulse_strip_cmap_kernel,
                                            audiopulse_strip_cmap_rep,
                                            audiopulse_strip_cmap_unfold,
                                            audiopulse_strip_cmap_dir,
                                            audiopulse_strip_cmap_color_style);
    layout->addWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &AudioPulse::SyncStripColormapFromPanel);

    stratum_panel = new StratumBandPanel(parent);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    layout->addWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &AudioPulse::OnStratumBandChanged);
    OnStratumBandChanged();
}

void AudioPulse::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    audiopulse_strip_cmap_on = strip_cmap_panel->useStripColormap();
    audiopulse_strip_cmap_kernel = strip_cmap_panel->kernelId();
    audiopulse_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    audiopulse_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    audiopulse_strip_cmap_dir = strip_cmap_panel->directionDeg();
    audiopulse_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
}

void AudioPulse::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
}

void AudioPulse::UpdateParams(SpatialEffectParams& /*params*/)
{
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
        EffectStratumBlend::BlendBands(stratum_layout_mode, sw, stratum_tuning_);

    float dx = rotated_pos.x - o.x;
    float dy = rotated_pos.y - o.y;
    float dz = rotated_pos.z - o.z;
    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    constexpr float kExplosionGridFill = 3.0f;
    float radius_basis =
        EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f * kExplosionGridFill;
    radius_basis = std::max(radius_basis, 1e-3f);

    float height_fade = HeightFade01(coord2);
    float energy =
        SampleBeatShell(distance, radius_basis, time, bb, height_fade, x, y, z);
    energy = std::clamp(energy, 0.0f, 1.0f);

    /* No fixed brightness floor — silence / decayed shells stay off (was 20% floor via 0.2f + 0.8f*energy). */
    constexpr float kShellSilenceEps = 0.006f;
    if(energy <= kShellSilenceEps)
    {
        return 0x00000000;
    }

    float max_r = radius_basis;
    float gradient_pos = std::clamp(distance / std::max(max_r, 1e-4f), 0.0f, 1.0f);

    RGBColor color = ComposeAudioGradientColor(audio_settings, 1.0f - gradient_pos, energy);
    float brightness = std::clamp(0.08f + 0.92f * energy, 0.0f, 1.0f);
    color = ScaleRGBColor(color, brightness);

    float detail = std::max(0.05f, GetScaledDetail());
    float hue_pos = std::clamp(1.0f - gradient_pos, 0.0f, 1.0f);
    hue_pos = fmodf(hue_pos * (0.65f + 0.35f * detail * bb.tight_mul) + bb.phase_deg * (1.0f / 360.0f), 1.0f);
    if(hue_pos < 0.0f)
    {
        hue_pos += 1.0f;
    }

    SpatialLayerCore::Basis basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
    SpatialLayerCore::MapperSettings map;
    SpatialLayerCore::InitAudioEffectMapperSettings(map, GetNormalizedScale(), std::max(0.05f, detail));
    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = x;
    sp.grid_y = y;
    sp.grid_z = z;
    sp.origin_x = o.x;
    sp.origin_y = o.y;
    sp.origin_z = o.z;
    sp.y_norm = coord2;

    RGBColor user_color;
    if(audiopulse_strip_cmap_on)
    {
        const float size_m = GetNormalizedSize();
        const float ph01 =
            std::fmod(CalculateProgress(time) * 0.33f + hue_pos * 0.2f +
                          time * GetScaledFrequency() * 12.0f * bb.speed_mul * (1.f / 360.f) + 1.f,
                      1.f);
        float p01 = SampleStripKernelPalette01(audiopulse_strip_cmap_kernel,
                                               audiopulse_strip_cmap_rep,
                                               audiopulse_strip_cmap_unfold,
                                               audiopulse_strip_cmap_dir,
                                               ph01,
                                               time,
                                               grid,
                                               size_m,
                                               o,
                                               rotated_pos);
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        user_color = ResolveStripKernelFinalColor(*this, audiopulse_strip_cmap_kernel, p01, audiopulse_strip_cmap_color_style,
                                                   time, GetScaledFrequency() * 12.0f * bb.speed_mul);
    }
    else if(GetRainbowMode())
    {
        float hue = hue_pos * 360.0f + CalculateProgress(time) * 40.0f * bb.speed_mul
                    + time * GetScaledFrequency() * 12.0f * bb.speed_mul;
        hue = ApplySpatialRainbowHue(hue, hue_pos, basis, sp, map, time, &grid);
        float p01 = std::fmod(hue / 360.0f, 1.0f);
        if(p01 < 0.0f)
        {
            p01 += 1.0f;
        }
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        user_color = GetRainbowColor(p01 * 360.0f);
    }
    else
    {
        float p = ApplySpatialPalette01(hue_pos, basis, sp, map, time, &grid);
        p = ApplyVoxelDriveToPalette01(p, x, y, z, time, grid);
        user_color = GetColorAtPosition(p);
    }
    return ModulateRGBColors(color, user_color);
}

nlohmann::json AudioPulse::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    j["onset_threshold"] = onset_threshold;
    j["particle_amount"] = particle_amount;
    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j,
                                           "audiopulse_stratum_layout_mode",
                                           sm,
                                           st,
                                           "audiopulse_stratum_band_speed_pct",
                                           "audiopulse_stratum_band_tight_pct",
                                           "audiopulse_stratum_band_phase_deg");
    StripColormapSaveJson(j,
                          "audiopulse",
                          audiopulse_strip_cmap_on,
                          audiopulse_strip_cmap_kernel,
                          audiopulse_strip_cmap_rep,
                          audiopulse_strip_cmap_unfold,
                          audiopulse_strip_cmap_dir,
                          audiopulse_strip_cmap_color_style);
    return j;
}

void AudioPulse::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "audiopulse_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "audiopulse_stratum_band_speed_pct",
                                            "audiopulse_stratum_band_tight_pct",
                                            "audiopulse_stratum_band_phase_deg");
    if(settings.contains("onset_threshold"))
    {
        onset_threshold = std::clamp(settings["onset_threshold"].get<float>(), 0.05f, 0.95f);
    }
    if(settings.contains("particle_amount"))
    {
        particle_amount = std::clamp(settings["particle_amount"].get<int>(), 0, 100);
    }
    pulses.clear();
    onset_smoothed = 0.0f;
    onset_hold = 0.0f;
    last_tick_time = std::numeric_limits<float>::lowest();
    StripColormapLoadJson(settings,
                          "audiopulse",
                          audiopulse_strip_cmap_on,
                          audiopulse_strip_cmap_kernel,
                          audiopulse_strip_cmap_rep,
                          audiopulse_strip_cmap_unfold,
                          audiopulse_strip_cmap_dir,
                          audiopulse_strip_cmap_color_style,
                          GetRainbowMode());
    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(audiopulse_strip_cmap_on,
                                                audiopulse_strip_cmap_kernel,
                                                audiopulse_strip_cmap_rep,
                                                audiopulse_strip_cmap_unfold,
                                                audiopulse_strip_cmap_dir,
                                                audiopulse_strip_cmap_color_style);
    }
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}

REGISTER_EFFECT_3D(AudioPulse)
