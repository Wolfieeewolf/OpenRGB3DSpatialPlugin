// SPDX-License-Identifier: GPL-2.0-only

#include "AudioPulse.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"
#include <QLabel>
#include <QCheckBox>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <cmath>
#include <algorithm>

float AudioPulse::EvaluateIntensity(float amplitude, float time)
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

AudioPulse::AudioPulse(QWidget* parent)
    : SpatialEffect3D(parent)
{
}

EffectInfo3D AudioPulse::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Audio Pulse";
    info.effect_description = "Room brightness pulses from a chosen frequency band; optional stratum band tuning";
    info.category = "Audio";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 0;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;
    info.default_speed_scale = 1.0f;
    info.default_frequency_scale = 20.0f;
    info.use_size_parameter = true;
    info.show_speed_control = false;
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
    smooth_row->addWidget(new QLabel("Smoothing:"));
    QSlider* smooth_slider = new QSlider(Qt::Horizontal);
    smooth_slider->setRange(0, 99);
    smooth_slider->setToolTip("How quickly level follows the selected frequency band (higher = smoother).");
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
    falloff_slider->setRange(20, 800);
    falloff_slider->setToolTip("When radial fade is on, how quickly brightness drops from the center outward.");
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
    boost_slider->setToolTip("Gain on band energy so quiet passages still move the room.");
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

    QCheckBox* radial_check = new QCheckBox("Radial Fade");
    radial_check->setToolTip("When on, output varies with distance from the effect origin; off = uniform room pulse.");
    radial_check->setChecked(use_radial);
    connect(radial_check, &QCheckBox::toggled, this, [this](bool checked){
        use_radial = checked;
        emit ParametersChanged();
    });
    layout->addWidget(radial_check);

    QCheckBox* breath_check = new QCheckBox("Breathing (radius grows with level)");
    breath_check->setChecked(radius_grows_with_level);
    breath_check->setToolTip("Quiet = center only; loud = fill room");
    connect(breath_check, &QCheckBox::toggled, this, [this](bool checked){
        radius_grows_with_level = checked;
        emit ParametersChanged();
    });
    layout->addWidget(breath_check);
    stratum_panel = new StratumBandPanel(parent);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    layout->addWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &AudioPulse::OnStratumBandChanged);
    OnStratumBandChanged();
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
    float amplitude = AudioInputManager::instance()->getBandEnergyHz(
        (float)audio_settings.low_hz, (float)audio_settings.high_hz);
    float intensity = EvaluateIntensity(amplitude, time);

    Vector3D o = GetEffectOriginGrid(grid);
    Vector3D rotated_pos = TransformPointByRotation(x, y, z, o);
    float coord2 = NormalizeGridAxis01(rotated_pos.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(stratum_layout_mode, sw, stratum_tuning_);

    float distance = 0.0f;
    if(use_radial)
    {
        float dx = rotated_pos.x - o.x;
        float dy = rotated_pos.y - o.y;
        float dz = rotated_pos.z - o.z;
        float max_radius = EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f;
        if(max_radius < 1e-5f) max_radius = 1e-5f;
        distance = std::clamp(std::sqrt(dx*dx + dy*dy + dz*dz) / max_radius, 0.0f, 1.0f);
    }

    float tm = std::clamp(bb.tight_mul, 0.25f, 4.0f);
    float brightness;
    if(use_radial && radius_grows_with_level)
    {
        float size_m = GetNormalizedSize();
        float effective_radius = 0.25f + 0.75f * intensity;
        effective_radius = std::min(1.0f, effective_radius * (0.6f + 0.4f * size_m));
        float fade_band = 0.35f / tm;
        float fade = (distance <= effective_radius) ? 1.0f : std::max(0.0f, 1.0f - (distance - effective_radius) / fade_band);
        brightness = intensity * fade;
    }
    else
    {
        brightness = use_radial ? intensity * (1.0f - distance * 0.5f * tm) : intensity;
    }
    brightness = std::clamp(brightness, 0.0f, 1.0f);

    RGBColor color = ComposeAudioGradientColor(audio_settings, use_radial ? (1.0f - distance) : 0.5f, intensity);
    color = ScaleRGBColor(color, 0.25f + 0.75f * brightness);

    float detail = std::max(0.05f, GetScaledDetail());
    float hue_pos = use_radial ? (1.0f - distance) : 0.5f;
    hue_pos = fmodf(hue_pos * (0.6f + 0.4f * detail * tm) + bb.phase_deg * (1.0f / 360.0f), 1.0f);
    if(hue_pos < 0.0f) hue_pos += 1.0f;

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
    if(GetRainbowMode())
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
    j["use_radial"] = use_radial;
    j["radius_grows_with_level"] = radius_grows_with_level;
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
    if(settings.contains("use_radial")) use_radial = settings["use_radial"].get<bool>();
    if(settings.contains("radius_grows_with_level")) radius_grows_with_level = settings["radius_grows_with_level"].get<bool>();
    smoothed = 0.0f;
    last_intensity_time = std::numeric_limits<float>::lowest();
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}

REGISTER_EFFECT_3D(AudioPulse)
