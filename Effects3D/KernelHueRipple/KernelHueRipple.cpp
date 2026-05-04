// SPDX-License-Identifier: GPL-2.0-only

#include "KernelHueRipple.h"
#include "AudioReactiveCommon.h"
#include "SpatialKernelColormap.h"
#include "StripKernelColormapPanel.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

KernelHueRipple::KernelHueRipple(QWidget* parent)
    : SpatialEffect3D(parent)
{
    ripples.reserve(64);
    SetRainbowMode(false);
}

EffectInfo3D KernelHueRipple::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 1;
    info.effect_name = "Kernel Hue Ripple";
    info.effect_description = "Beat-triggered rings shape brightness; strip kernel supplies hue only.";
    info.category = "Audio";
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.default_speed_scale = 4.0f;
    info.needs_frequency = true;
    info.default_frequency_scale = 20.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = true;
    return info;
}

void KernelHueRipple::SetupCustomUI(QWidget* parent)
{
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(parent->layout());
    if(!layout)
        layout = new QVBoxLayout(parent);

    QHBoxLayout* width_row = new QHBoxLayout();
    width_row->addWidget(new QLabel("Ring width:"));
    QSlider* width_slider = new QSlider(Qt::Horizontal);
    width_slider->setRange(5, 200);
    width_slider->setValue((int)(trail_width * 100.0f));
    QLabel* width_label = new QLabel(QString::number((int)(trail_width * 100)) + "%");
    width_row->addWidget(width_slider);
    width_row->addWidget(width_label);
    layout->addLayout(width_row);
    connect(width_slider, &QSlider::valueChanged, this, [this, width_label](int v) {
        trail_width = v / 100.0f;
        width_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });

    QHBoxLayout* decay_row = new QHBoxLayout();
    decay_row->addWidget(new QLabel("Decay:"));
    QSlider* decay_slider = new QSlider(Qt::Horizontal);
    decay_slider->setRange(50, 800);
    decay_slider->setValue((int)(decay_rate * 100.0f));
    QLabel* decay_label = new QLabel(QString::number(decay_rate, 'f', 1));
    decay_row->addWidget(decay_slider);
    decay_row->addWidget(decay_label);
    layout->addLayout(decay_row);
    connect(decay_slider, &QSlider::valueChanged, this, [this, decay_label](int v) {
        decay_rate = v / 100.0f;
        decay_label->setText(QString::number(decay_rate, 'f', 1));
        emit ParametersChanged();
    });

    QHBoxLayout* thresh_row = new QHBoxLayout();
    thresh_row->addWidget(new QLabel("Threshold:"));
    QSlider* thresh_slider = new QSlider(Qt::Horizontal);
    thresh_slider->setRange(0, 95);
    thresh_slider->setValue((int)(onset_threshold * 100.0f));
    QLabel* thresh_label = new QLabel(QString::number((int)(onset_threshold * 100)) + "%");
    thresh_row->addWidget(thresh_slider);
    thresh_row->addWidget(thresh_label);
    layout->addLayout(thresh_row);
    connect(thresh_slider, &QSlider::valueChanged, this, [this, thresh_label](int v) {
        onset_threshold = v / 100.0f;
        thresh_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });

    QHBoxLayout* boost_row = new QHBoxLayout();
    boost_row->addWidget(new QLabel("Peak boost:"));
    QSlider* boost_slider = new QSlider(Qt::Horizontal);
    boost_slider->setRange(50, 500);
    boost_slider->setValue((int)(audio_settings.peak_boost * 100.0f));
    QLabel* boost_label = new QLabel(QString::number(audio_settings.peak_boost, 'f', 2) + "x");
    boost_row->addWidget(boost_slider);
    boost_row->addWidget(boost_label);
    layout->addLayout(boost_row);
    connect(boost_slider, &QSlider::valueChanged, this, [this, boost_label](int v) {
        audio_settings.peak_boost = v / 100.0f;
        boost_label->setText(QString::number(audio_settings.peak_boost, 'f', 2) + "x");
        emit ParametersChanged();
    });

    QHBoxLayout* edge_row = new QHBoxLayout();
    edge_row->addWidget(new QLabel("Edge:"));
    QComboBox* edge_combo = new QComboBox();
    edge_combo->addItem("Round");
    edge_combo->addItem("Sharp");
    edge_combo->addItem("Square");
    edge_combo->setCurrentIndex(std::clamp(ripple_edge_shape, 0, 2));
    edge_row->addWidget(edge_combo);
    layout->addLayout(edge_row);
    connect(edge_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        ripple_edge_shape = std::clamp(idx, 0, 2);
        emit ParametersChanged();
    });

    strip_cmap_panel = new StripKernelColormapPanel(parent);
    strip_cmap_panel->mirrorStateFromEffect(khripple_strip_cmap_on,
                                            khripple_strip_cmap_kernel,
                                            khripple_strip_cmap_rep,
                                            khripple_strip_cmap_unfold,
                                            khripple_strip_cmap_dir,
                                            khripple_strip_cmap_color_style);
    layout->addWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &KernelHueRipple::SyncStripColormapFromPanel);

    stratum_panel = new StratumBandPanel(parent);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    layout->addWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &KernelHueRipple::OnStratumBandChanged);
    OnStratumBandChanged();
}

void KernelHueRipple::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    khripple_strip_cmap_on = strip_cmap_panel->useStripColormap();
    khripple_strip_cmap_kernel = strip_cmap_panel->kernelId();
    khripple_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    khripple_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    khripple_strip_cmap_dir = strip_cmap_panel->directionDeg();
    khripple_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
}

void KernelHueRipple::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
}

float KernelHueRipple::smoothstep(float edge0, float edge1, float x) const
{
    float t = (x - edge0) / (std::max(0.0001f, edge1 - edge0));
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void KernelHueRipple::TickRipples(float time)
{
    if(std::fabs(time - last_tick_time) < 1e-4f)
        return;
    float dt = (last_tick_time == std::numeric_limits<float>::lowest()) ? 0.0f
                                                                         : std::clamp(time - last_tick_time, 0.0f, 0.1f);
    last_tick_time = time;

    float onset_raw = AudioInputManager::instance()->getOnsetLevel();
    onset_smoothed = 0.5f * onset_smoothed + 0.5f * onset_raw;

    if(onset_hold > 0.0f)
        onset_hold = std::max(0.0f, onset_hold - dt);

    if(onset_hold <= 0.0f && onset_smoothed >= onset_threshold)
    {
        Ripple r;
        r.birth_time = time;
        r.strength = std::clamp(onset_smoothed * audio_settings.peak_boost, 0.0f, 1.0f);
        ripples.push_back(r);
        onset_hold = 0.12f;
    }

    ripples.erase(std::remove_if(ripples.begin(), ripples.end(),
                                [&](const Ripple& r) {
                                    float age = time - r.birth_time;
                                    float alive = r.strength * std::exp(-decay_rate * age);
                                    return alive < 0.004f;
                                }),
                  ripples.end());
}

float KernelHueRipple::RippleBrightness(float dist_norm, float time, const EffectStratumBlend::BandBlendScalars& bb) const
{
    float peak = 0.0f;
    float size_m = GetNormalizedSize();
    float detail = std::max(0.05f, GetScaledDetail());
    float tm = std::clamp(bb.tight_mul, 0.25f, 4.0f);
    float phase_shift = bb.phase_deg * (1.0f / 360.0f);

    for(const Ripple& r : ripples)
    {
        float age = time - r.birth_time;
        if(age < 0.0f)
            continue;

        float expand = (0.2f + GetScaledSpeed() * 0.95f) * bb.speed_mul;
        float front = expand * age + phase_shift * 0.35f;
        float ring_dist = std::fabs(dist_norm - front);
        float half_w = std::max(trail_width * 0.5f * size_m, 1e-3f) / ((0.6f + 0.4f * detail) * tm);

        float ring_bright = 0.0f;
        switch(ripple_edge_shape)
        {
        case 0:
            ring_bright = 1.0f - smoothstep(0.0f, half_w, ring_dist);
            break;
        case 1:
            ring_bright = ring_dist < half_w * 0.5f ? 1.0f : 0.0f;
            break;
        default:
            ring_bright = ring_dist < half_w ? 1.0f : 0.0f;
            break;
        }

        float fade = r.strength * std::exp(-decay_rate * age);
        peak = std::max(peak, ring_bright * fade);
    }
    return std::clamp(peak, 0.0f, 1.0f);
}

void KernelHueRipple::UpdateParams(SpatialEffectParams& /*params*/) {}

RGBColor KernelHueRipple::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
        return 0x00000000;

    TickRipples(time);

    Vector3D origin = GetEffectOriginGrid(grid);
    Vector3D rp = TransformPointByRotation(x, y, z, origin);
    float coord2 = NormalizeGridAxis01(rp.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(stratum_layout_mode, sw, stratum_tuning_);

    float dx = rp.x - origin.x, dy = rp.y - origin.y, dz = rp.z - origin.z;
    float max_radius = EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f;
    float dist_norm = std::clamp(std::sqrt(dx * dx + dy * dy + dz * dz) / std::max(max_radius, 1e-5f), 0.0f, 2.0f);

    float bright = RippleBrightness(dist_norm, time, bb);
    float ambient = 0.06f;
    bright = std::clamp(bright + ambient, 0.0f, 1.0f);

    const float size_m = GetNormalizedSize();
    const float ph01 =
        std::fmod(CalculateProgress(time) * 0.37f + dist_norm * 0.21f +
                      time * GetScaledFrequency() * 12.0f * bb.speed_mul * (1.f / 360.f) + 1.f,
                  1.f);

    RGBColor rgb = 0x00000000;
    if(khripple_strip_cmap_on)
    {
        float p01 = SampleStripKernelPalette01(khripple_strip_cmap_kernel,
                                               khripple_strip_cmap_rep,
                                               khripple_strip_cmap_unfold,
                                               khripple_strip_cmap_dir,
                                               ph01,
                                               time,
                                               grid,
                                               size_m,
                                               origin,
                                               rp);
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        rgb = ResolveStripKernelFinalColor(*this, khripple_strip_cmap_kernel, std::min(p01, 1.0f),
                                           khripple_strip_cmap_color_style, time,
                                           GetScaledFrequency() * 12.0f * bb.speed_mul);
    }
    else if(GetRainbowMode())
    {
        rgb = GetRainbowColor(ph01 * 360.0f + bb.phase_deg);
    }
    else
    {
        rgb = GetColorAtPosition(ph01);
    }

    return PostProcessColorGrid(ScaleRGBColor(rgb, bright));
}

nlohmann::json KernelHueRipple::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    AudioReactiveSaveToJson(j, audio_settings);
    j["khripple_trail_width"] = trail_width;
    j["khripple_decay_rate"] = decay_rate;
    j["khripple_onset_threshold"] = onset_threshold;
    j["khripple_edge_shape"] = ripple_edge_shape;
    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j,
                                           "khripple_stratum_layout_mode",
                                           sm,
                                           st,
                                           "khripple_stratum_band_speed_pct",
                                           "khripple_stratum_band_tight_pct",
                                           "khripple_stratum_band_phase_deg");
    StripColormapSaveJson(j,
                          "khripple",
                          khripple_strip_cmap_on,
                          khripple_strip_cmap_kernel,
                          khripple_strip_cmap_rep,
                          khripple_strip_cmap_unfold,
                          khripple_strip_cmap_dir,
                          khripple_strip_cmap_color_style);
    return j;
}

void KernelHueRipple::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    AudioReactiveLoadFromJson(audio_settings, settings);
    if(settings.contains("khripple_trail_width"))
        trail_width = settings["khripple_trail_width"].get<float>();
    if(settings.contains("khripple_decay_rate"))
        decay_rate = settings["khripple_decay_rate"].get<float>();
    if(settings.contains("khripple_onset_threshold"))
        onset_threshold = settings["khripple_onset_threshold"].get<float>();
    if(settings.contains("khripple_edge_shape"))
        ripple_edge_shape = std::clamp(settings["khripple_edge_shape"].get<int>(), 0, 2);
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "khripple_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "khripple_stratum_band_speed_pct",
                                            "khripple_stratum_band_tight_pct",
                                            "khripple_stratum_band_phase_deg");
    StripColormapLoadJson(settings,
                          "khripple",
                          khripple_strip_cmap_on,
                          khripple_strip_cmap_kernel,
                          khripple_strip_cmap_rep,
                          khripple_strip_cmap_unfold,
                          khripple_strip_cmap_dir,
                          khripple_strip_cmap_color_style,
                          GetRainbowMode());
    ripples.clear();
    last_tick_time = std::numeric_limits<float>::lowest();
    onset_smoothed = 0.0f;
    onset_hold = 0.0f;
    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(khripple_strip_cmap_on,
                                                khripple_strip_cmap_kernel,
                                                khripple_strip_cmap_rep,
                                                khripple_strip_cmap_unfold,
                                                khripple_strip_cmap_dir,
                                                khripple_strip_cmap_color_style);
    }
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}

REGISTER_EFFECT_3D(KernelHueRipple)
