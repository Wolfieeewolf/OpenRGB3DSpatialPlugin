// SPDX-License-Identifier: GPL-2.0-only

#include "BeatPulse.h"
#include "SpatialKernelColormap.h"
#include "StripKernelColormapPanel.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"
#include <algorithm>
#include <cmath>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

float BeatPulse::EvaluateIntensity(float amplitude, float time)
{
    amplitude = std::clamp(amplitude, 0.0f, 1.0f);
    float alpha = std::clamp(audio_settings.smoothing, 0.0f, 0.99f);
    if(std::fabs(time - last_intensity_time) > 1e-4f)
    {
        smoothed = alpha * smoothed + (1.0f - alpha) * amplitude;
        last_intensity_time = time;
        float decay = 0.65f + alpha * 0.25f;
        envelope = std::max(envelope * decay, smoothed);
    }
    else if(alpha <= 0.0f)
    {
        smoothed = amplitude;
        envelope = amplitude;
    }
    return ApplyAudioIntensity(envelope, audio_settings);
}

BeatPulse::BeatPulse(QWidget* parent)
    : SpatialEffect3D(parent)
{
}

EffectInfo3D BeatPulse::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Beat Pulse";
    info.effect_description =
        "Beat-triggered waves expand from center; onset detection drives new pulses; optional stratum band tuning";
    info.category = "Audio";
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 0;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 4.0f;
    info.needs_frequency = true;
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

void BeatPulse::SetupCustomUI(QWidget* parent)
{
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(parent->layout());
    if(!layout)
    {
        layout = new QVBoxLayout(parent);
    }

    QHBoxLayout* smooth_row = new QHBoxLayout();
    smooth_row->addWidget(new QLabel("Smoothing:"));
    QSlider* smooth_slider = new QSlider(Qt::Horizontal);
    smooth_slider->setRange(0, 99);
    smooth_slider->setToolTip("Smooths the beat envelope between onsets.");
    smooth_slider->setValue((int)(audio_settings.smoothing * 100.0f));
    QLabel* smooth_label = new QLabel(QString::number(audio_settings.smoothing, 'f', 2));
    smooth_label->setMinimumWidth(36);
    smooth_row->addWidget(smooth_slider);
    smooth_row->addWidget(smooth_label);
    layout->addLayout(smooth_row);

    connect(smooth_slider, &QSlider::valueChanged, this, [this, smooth_label](int v){
        audio_settings.smoothing = v / 100.0f;
        smooth_label->setText(QString::number(audio_settings.smoothing, 'f', 2));
        emit ParametersChanged();
    });

    QHBoxLayout* falloff_row = new QHBoxLayout();
    falloff_row->addWidget(new QLabel("Falloff:"));
    QSlider* falloff_slider = new QSlider(Qt::Horizontal);
    falloff_slider->setRange(20, 500);
    falloff_slider->setToolTip("How tight each expanding wave ring is behind the pulse front.");
    falloff_slider->setValue((int)(audio_settings.falloff * 100.0f));
    QLabel* falloff_label = new QLabel(QString::number(audio_settings.falloff, 'f', 1));
    falloff_label->setMinimumWidth(36);
    falloff_row->addWidget(falloff_slider);
    falloff_row->addWidget(falloff_label);
    layout->addLayout(falloff_row);

    connect(falloff_slider, &QSlider::valueChanged, this, [this, falloff_label](int v){
        audio_settings.falloff = v / 100.0f;
        falloff_label->setText(QString::number(audio_settings.falloff, 'f', 1));
        emit ParametersChanged();
    });

    QHBoxLayout* boost_row = new QHBoxLayout();
    boost_row->addWidget(new QLabel("Peak Boost:"));
    QSlider* boost_slider = new QSlider(Qt::Horizontal);
    boost_slider->setRange(50, 500);
    boost_slider->setToolTip("Boosts band energy used for pulse strength.");
    boost_slider->setValue((int)(audio_settings.peak_boost * 100.0f));
    QLabel* boost_label = new QLabel(QString::number(audio_settings.peak_boost, 'f', 2) + "x");
    boost_label->setMinimumWidth(44);
    boost_row->addWidget(boost_slider);
    boost_row->addWidget(boost_label);
    layout->addLayout(boost_row);

    connect(boost_slider, &QSlider::valueChanged, this, [this, boost_label](int v){
        audio_settings.peak_boost = v / 100.0f;
        boost_label->setText(QString::number(audio_settings.peak_boost, 'f', 2) + "x");
        emit ParametersChanged();
    });

    QHBoxLayout* sens_row = new QHBoxLayout();
    sens_row->addWidget(new QLabel("Beat sensitivity:"));
    QSlider* sens_slider = new QSlider(Qt::Horizontal);
    sens_slider->setRange(0, 95);
    sens_slider->setToolTip("Higher = fewer but stronger beat triggers (uses onset detector).");
    sens_slider->setValue((int)(onset_threshold * 100.0f));
    QLabel* sens_label = new QLabel(QString::number((int)(onset_threshold * 100)) + "%");
    sens_label->setMinimumWidth(40);
    sens_row->addWidget(sens_slider);
    sens_row->addWidget(sens_label);
    layout->addLayout(sens_row);
    connect(sens_slider, &QSlider::valueChanged, this, [this, sens_label](int v){
        onset_threshold = v / 100.0f;
        sens_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });

    strip_cmap_panel = new StripKernelColormapPanel(parent);
    strip_cmap_panel->mirrorStateFromEffect(beatpulse_strip_cmap_on,
                                            beatpulse_strip_cmap_kernel,
                                            beatpulse_strip_cmap_rep,
                                            beatpulse_strip_cmap_unfold,
                                            beatpulse_strip_cmap_dir,
                                            beatpulse_strip_cmap_color_style);
    AddColorPatternWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &BeatPulse::SyncStripColormapFromPanel);

    stratum_panel = new StratumBandPanel(parent);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    AddBandModulationWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &BeatPulse::OnStratumBandChanged);
    OnStratumBandChanged();
}

void BeatPulse::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    beatpulse_strip_cmap_on = strip_cmap_panel->useStripColormap();
    beatpulse_strip_cmap_kernel = strip_cmap_panel->kernelId();
    beatpulse_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    beatpulse_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    beatpulse_strip_cmap_dir = strip_cmap_panel->directionDeg();
    beatpulse_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
}

void BeatPulse::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
}

void BeatPulse::TickPulses(float time)
{
    if(std::fabs(time - last_tick_time) < 1e-5f) return;
    float dt = (last_tick_time == std::numeric_limits<float>::lowest()) ? 0.0f
               : std::clamp(time - last_tick_time, 0.0f, 0.1f);
    last_tick_time = time;

    AudioInputManager* audio = AudioInputManager::instance();
    float onset_raw = audio->getOnsetLevel();
    onset_smoothed = 0.5f * onset_smoothed + 0.5f * onset_raw;

    if(onset_hold > 0.0f) onset_hold = std::max(0.0f, onset_hold - dt);

    if(onset_hold <= 0.0f && onset_smoothed >= onset_threshold)
    {
        float amp = audio->getBandEnergyHz((float)audio_settings.low_hz, (float)audio_settings.high_hz);
        float strength = ApplyAudioIntensity(std::clamp(amp, 0.0f, 1.0f), audio_settings);
        PulseData p;
        p.birth_time = time;
        p.strength   = strength;
        pulses.push_back(p);
        onset_hold = 0.12f;
    }

    pulses.erase(std::remove_if(pulses.begin(), pulses.end(), [time](const PulseData& p) {
        float age = time - p.birth_time;
        return age > 2.0f || p.strength * std::exp(-2.5f * age) < 0.004f;
    }), pulses.end());
}

float BeatPulse::SamplePulseField(float radial_norm,
                                  float height_norm,
                                  float time,
                                  const EffectStratumBlend::BandBlendScalars& bb) const
{
    float pulse_speed = (2.0f + GetScaledSpeed()) * bb.speed_mul;
    float size_m = GetNormalizedSize();
    float detail = std::max(0.05f, GetScaledDetail());
    float tm = std::clamp(bb.tight_mul, 0.25f, 4.0f);
    float width_k =
        ((36.0f * (0.6f + 0.4f * detail)) / std::max(0.35f, size_m)) * tm;
    float phase_radial = bb.phase_deg * (1.0f / 360.0f);

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

    float total = 0.0f;
    for(const PulseData& p : pulses)
    {
        float age = time - p.birth_time;
        if(age < 0.0f) continue;
        float front = std::min(1.2f, age * pulse_speed);
        float rn = std::fmod(radial_norm + phase_radial + 1.0f, 1.0f);
        float distance = std::fabs(rn - front);
        float pulse = std::exp(-distance * distance * width_k);
        float tail = std::exp(-std::max(distance - 0.2f, 0.0f) * 6.0f);
        float fade = p.strength * std::exp(-2.5f * age);
        float contrib = fade * (0.7f * pulse + 0.3f * tail) * height_fade;
        total += contrib;
    }
    return std::clamp(total, 0.0f, 1.0f);
}

void BeatPulse::UpdateParams(SpatialEffectParams& /*params*/)
{
}


RGBColor BeatPulse::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    TickPulses(time);
    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float coord2 = NormalizeGridAxis01(rotated_pos.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(stratum_layout_mode, sw, stratum_tuning_);
    float dx = rotated_pos.x - origin.x;
    float dy = rotated_pos.y - origin.y;
    float dz = rotated_pos.z - origin.z;
    float max_radius = EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f;
    float radial_norm = ComputeRadialNormalized(dx, dy, dz, max_radius);
    float height_norm = coord2;
    float energy = SamplePulseField(radial_norm, height_norm, time, bb);
    float ambient = EvaluateIntensity(
        AudioInputManager::instance()->getBandEnergyHz((float)audio_settings.low_hz, (float)audio_settings.high_hz), time);
    energy = std::clamp(energy + 0.15f * ambient, 0.0f, 1.0f);

    float gradient_pos = std::clamp(radial_norm, 0.0f, 1.0f);

    float detail = std::max(0.05f, GetScaledDetail());
    SpatialLayerCore::Basis basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
    SpatialLayerCore::MapperSettings map;
    SpatialLayerCore::InitAudioEffectMapperSettings(map, GetNormalizedScale(), detail);
    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = x;
    sp.grid_y = y;
    sp.grid_z = z;
    sp.origin_x = origin.x;
    sp.origin_y = origin.y;
    sp.origin_z = origin.z;
    sp.y_norm = coord2;

    RGBColor color = ComposeAudioGradientColor(audio_settings, gradient_pos, energy);
    color = ScaleRGBColor(color, (0.25f + 0.75f * energy));

    RGBColor user_color;
    if(beatpulse_strip_cmap_on)
    {
        const float size_m = GetNormalizedSize();
        const float ph01 =
            std::fmod(CalculateProgress(time) * 0.31f + gradient_pos * 0.18f +
                          time * GetScaledFrequency() * 12.0f * bb.speed_mul * (1.f / 360.f) + 1.f,
                      1.f);
        float p01 = SampleStripKernelPalette01(beatpulse_strip_cmap_kernel,
                                               beatpulse_strip_cmap_rep,
                                               beatpulse_strip_cmap_unfold,
                                               beatpulse_strip_cmap_dir,
                                               ph01,
                                               time,
                                               grid,
                                               size_m,
                                               origin,
                                               rotated_pos);
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        user_color = ResolveStripKernelFinalColor(*this, beatpulse_strip_cmap_kernel, p01, beatpulse_strip_cmap_color_style, time,
                                                  GetScaledFrequency() * 12.0f * bb.speed_mul);
    }
    else if(GetRainbowMode())
    {
        float hue = gradient_pos * 360.0f + time * GetScaledFrequency() * 12.0f * bb.speed_mul
                    + bb.phase_deg * (1.0f / 360.0f) * 40.0f;
        hue = ApplySpatialRainbowHue(hue, gradient_pos, basis, sp, map, time, &grid);
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
        float p = ApplySpatialPalette01(gradient_pos, basis, sp, map, time, &grid);
        p = ApplyVoxelDriveToPalette01(p, x, y, z, time, grid);
        user_color = GetColorAtPosition(p);
    }
    return ModulateRGBColors(color, user_color);
}

nlohmann::json BeatPulse::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    j["onset_threshold"] = onset_threshold;
    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j,
                                           "beatpulse_stratum_layout_mode",
                                           sm,
                                           st,
                                           "beatpulse_stratum_band_speed_pct",
                                           "beatpulse_stratum_band_tight_pct",
                                           "beatpulse_stratum_band_phase_deg");
    StripColormapSaveJson(j,
                          "beatpulse",
                          beatpulse_strip_cmap_on,
                          beatpulse_strip_cmap_kernel,
                          beatpulse_strip_cmap_rep,
                          beatpulse_strip_cmap_unfold,
                          beatpulse_strip_cmap_dir,
                          beatpulse_strip_cmap_color_style);
    return j;
}

void BeatPulse::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "beatpulse_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "beatpulse_stratum_band_speed_pct",
                                            "beatpulse_stratum_band_tight_pct",
                                            "beatpulse_stratum_band_phase_deg");
    if(settings.contains("onset_threshold")) onset_threshold = std::clamp(settings["onset_threshold"].get<float>(), 0.05f, 0.95f);
    envelope = 0.0f;
    smoothed = 0.0f;
    last_intensity_time = std::numeric_limits<float>::lowest();
    pulses.clear();
    onset_smoothed = 0.0f;
    onset_hold = 0.0f;
    last_tick_time = std::numeric_limits<float>::lowest();
    StripColormapLoadJson(settings,
                          "beatpulse",
                          beatpulse_strip_cmap_on,
                          beatpulse_strip_cmap_kernel,
                          beatpulse_strip_cmap_rep,
                          beatpulse_strip_cmap_unfold,
                          beatpulse_strip_cmap_dir,
                          beatpulse_strip_cmap_color_style,
                          GetRainbowMode());
    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(beatpulse_strip_cmap_on,
                                                beatpulse_strip_cmap_kernel,
                                                beatpulse_strip_cmap_rep,
                                                beatpulse_strip_cmap_unfold,
                                                beatpulse_strip_cmap_dir,
                                                beatpulse_strip_cmap_color_style);
    }
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}

REGISTER_EFFECT_3D(BeatPulse)
