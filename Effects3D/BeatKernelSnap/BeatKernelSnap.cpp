// SPDX-License-Identifier: GPL-2.0-only

#include "BeatKernelSnap.h"
#include "AudioReactiveCommon.h"
#include "SpatialKernelColormap.h"
#include "StripKernelColormapPanel.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"
#include "Audio/AudioInputManager.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <cmath>

BeatKernelSnap::BeatKernelSnap(QWidget* parent)
    : SpatialEffect3D(parent)
{
    SetRainbowMode(false);
}

EffectInfo3D BeatKernelSnap::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 1;
    info.effect_name = "Beat Kernel Snap";
    info.effect_description =
        "Audio onset freezes or jumps strip-kernel phase while spatial kernel structure keeps moving.";
    info.category = "Audio";
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_frequency = true;
    info.default_frequency_scale = 12.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_detail_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = true;
    return info;
}

void BeatKernelSnap::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* snap_row = new QHBoxLayout();
    snap_row->addWidget(new QLabel("On beat:"));
    QComboBox* snap_combo = new QComboBox();
    snap_combo->addItem("Freeze phase");
    snap_combo->addItem("Jump phase (+0.5)");
    snap_combo->setCurrentIndex(std::clamp(snap_mode, 0, 1));
    snap_row->addWidget(snap_combo);
    vbox->addLayout(snap_row);
    connect(snap_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        snap_mode = std::clamp(idx, 0, 1);
        emit ParametersChanged();
    });

    QHBoxLayout* hold_row = new QHBoxLayout();
    hold_row->addWidget(new QLabel("Hold (ms):"));
    QSlider* hold_slider = new QSlider(Qt::Horizontal);
    hold_slider->setRange(40, 350);
    hold_slider->setValue((int)(hold_duration * 1000.0f));
    QLabel* hold_label = new QLabel(QString::number((int)(hold_duration * 1000.f)));
    hold_row->addWidget(hold_slider);
    hold_row->addWidget(hold_label);
    vbox->addLayout(hold_row);
    connect(hold_slider, &QSlider::valueChanged, this, [this, hold_label](int v) {
        hold_duration = v / 1000.0f;
        hold_label->setText(QString::number(v));
        emit ParametersChanged();
    });

    QHBoxLayout* sens_row = new QHBoxLayout();
    sens_row->addWidget(new QLabel("Sensitivity:"));
    QSlider* sens_slider = new QSlider(Qt::Horizontal);
    sens_slider->setRange(5, 90);
    sens_slider->setValue((int)(onset_threshold * 100.0f));
    QLabel* sens_label = new QLabel(QString::number((int)(onset_threshold * 100)) + "%");
    sens_row->addWidget(sens_slider);
    sens_row->addWidget(sens_label);
    vbox->addLayout(sens_row);
    connect(sens_slider, &QSlider::valueChanged, this, [this, sens_label](int v) {
        onset_threshold = v / 100.0f;
        sens_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });

    QHBoxLayout* adv_row = new QHBoxLayout();
    adv_row->addWidget(new QLabel("Phase drift:"));
    QSlider* adv_slider = new QSlider(Qt::Horizontal);
    adv_slider->setRange(1, 100);
    adv_slider->setValue((int)(phase_advance_scale * 100.0f));
    QLabel* adv_label = new QLabel(QString::number(phase_advance_scale, 'f', 2));
    adv_row->addWidget(adv_slider);
    adv_row->addWidget(adv_label);
    vbox->addLayout(adv_row);
    connect(adv_slider, &QSlider::valueChanged, this, [this, adv_label](int v) {
        phase_advance_scale = v / 100.0f;
        adv_label->setText(QString::number(phase_advance_scale, 'f', 2));
        emit ParametersChanged();
    });

    strip_cmap_panel = new StripKernelColormapPanel(w);
    strip_cmap_panel->mirrorStateFromEffect(bksnap_strip_cmap_on,
                                            bksnap_strip_cmap_kernel,
                                            bksnap_strip_cmap_rep,
                                            bksnap_strip_cmap_unfold,
                                            bksnap_strip_cmap_dir,
                                            bksnap_strip_cmap_color_style);
    vbox->addWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &BeatKernelSnap::SyncStripColormapFromPanel);

    stratum_panel = new StratumBandPanel(w);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    vbox->addWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &BeatKernelSnap::OnStratumBandChanged);
    OnStratumBandChanged();

    AddWidgetToParent(w, parent);
}

void BeatKernelSnap::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    bksnap_strip_cmap_on = strip_cmap_panel->useStripColormap();
    bksnap_strip_cmap_kernel = strip_cmap_panel->kernelId();
    bksnap_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    bksnap_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    bksnap_strip_cmap_dir = strip_cmap_panel->directionDeg();
    bksnap_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
}

void BeatKernelSnap::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
}

void BeatKernelSnap::TickSnap(float time)
{
    if(std::fabs(time - last_tick_time) < 1e-5f)
        return;
    float dt = (last_tick_time == std::numeric_limits<float>::lowest()) ? 0.0f
                                                                         : std::clamp(time - last_tick_time, 0.0f, 0.1f);
    last_tick_time = time;

    running_phase01 =
        std::fmod(running_phase01 + dt * (0.05f + phase_advance_scale * GetScaledSpeed() * 0.04f) + 1.0f, 1.0f);

    AudioInputManager* audio = AudioInputManager::instance();
    float onset_raw = audio->getOnsetLevel();
    onset_smoothed = 0.5f * onset_smoothed + 0.5f * onset_raw;

    if(onset_hold > 0.0f)
        onset_hold = std::max(0.0f, onset_hold - dt);

    if(onset_hold <= 0.0f && onset_smoothed >= onset_threshold)
    {
        if(snap_mode == 0)
            snapped_phase01 = running_phase01;
        else
            snapped_phase01 = std::fmod(running_phase01 + 0.5f, 1.0f);
        holding = true;
        hold_remaining = hold_duration;
        onset_hold = 0.12f;
    }

    if(holding)
    {
        hold_remaining -= dt;
        if(hold_remaining <= 0.0f)
            holding = false;
    }
}

void BeatKernelSnap::UpdateParams(SpatialEffectParams& /*params*/) {}

RGBColor BeatKernelSnap::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
        return 0x00000000;

    TickSnap(time);

    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rp = TransformPointByRotation(x, y, z, origin);
    float coord2 = NormalizeGridAxis01(rp.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(stratum_layout_mode, sw, stratum_tuning_);

    float phase01_use = holding ? snapped_phase01 : running_phase01;
    const float ph_kernel =
        std::fmod(phase01_use + coord2 * 0.08f + time * GetScaledFrequency() * 0.002f * bb.speed_mul +
                      bb.phase_deg * (1.f / 360.f),
                  1.f);

    const float size_m = GetNormalizedSize();
    RGBColor rgb = 0x00000000;
    if(bksnap_strip_cmap_on)
    {
        float p01 = SampleStripKernelPalette01(bksnap_strip_cmap_kernel,
                                               bksnap_strip_cmap_rep,
                                               bksnap_strip_cmap_unfold,
                                               bksnap_strip_cmap_dir,
                                               ph_kernel,
                                               time,
                                               grid,
                                               size_m,
                                               origin,
                                               rp);
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        rgb = ResolveStripKernelFinalColor(*this, bksnap_strip_cmap_kernel, p01, bksnap_strip_cmap_color_style, time,
                                           GetScaledFrequency() * 12.0f * bb.speed_mul);
    }
    else if(GetRainbowMode())
    {
        rgb = GetRainbowColor(ph_kernel * 360.0f + bb.phase_deg);
    }
    else
    {
        rgb = GetColorAtPosition(ph_kernel);
    }

    float pulse = holding ? (0.65f + 0.35f * std::clamp(hold_remaining / std::max(0.001f, hold_duration), 0.0f, 1.0f))
                          : 1.0f;
    return PostProcessColorGrid(ScaleRGBColor(rgb, pulse));
}

nlohmann::json BeatKernelSnap::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["bksnap_snap_mode"] = snap_mode;
    j["bksnap_hold_duration"] = hold_duration;
    j["bksnap_onset_threshold"] = onset_threshold;
    j["bksnap_phase_advance_scale"] = phase_advance_scale;
    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j,
                                           "bksnap_stratum_layout_mode",
                                           sm,
                                           st,
                                           "bksnap_stratum_band_speed_pct",
                                           "bksnap_stratum_band_tight_pct",
                                           "bksnap_stratum_band_phase_deg");
    StripColormapSaveJson(j,
                          "bksnap",
                          bksnap_strip_cmap_on,
                          bksnap_strip_cmap_kernel,
                          bksnap_strip_cmap_rep,
                          bksnap_strip_cmap_unfold,
                          bksnap_strip_cmap_dir,
                          bksnap_strip_cmap_color_style);
    return j;
}

void BeatKernelSnap::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("bksnap_snap_mode"))
        snap_mode = std::clamp(settings["bksnap_snap_mode"].get<int>(), 0, 1);
    if(settings.contains("bksnap_hold_duration"))
        hold_duration = std::clamp(settings["bksnap_hold_duration"].get<float>(), 0.04f, 0.35f);
    if(settings.contains("bksnap_onset_threshold"))
        onset_threshold = std::clamp(settings["bksnap_onset_threshold"].get<float>(), 0.05f, 0.95f);
    if(settings.contains("bksnap_phase_advance_scale"))
        phase_advance_scale = std::clamp(settings["bksnap_phase_advance_scale"].get<float>(), 0.01f, 1.0f);
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "bksnap_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "bksnap_stratum_band_speed_pct",
                                            "bksnap_stratum_band_tight_pct",
                                            "bksnap_stratum_band_phase_deg");
    StripColormapLoadJson(settings,
                          "bksnap",
                          bksnap_strip_cmap_on,
                          bksnap_strip_cmap_kernel,
                          bksnap_strip_cmap_rep,
                          bksnap_strip_cmap_unfold,
                          bksnap_strip_cmap_dir,
                          bksnap_strip_cmap_color_style,
                          GetRainbowMode());
    running_phase01 = 0.0f;
    snapped_phase01 = 0.0f;
    holding = false;
    hold_remaining = 0.0f;
    last_tick_time = std::numeric_limits<float>::lowest();
    onset_smoothed = 0.0f;
    onset_hold = 0.0f;
    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(bksnap_strip_cmap_on,
                                                bksnap_strip_cmap_kernel,
                                                bksnap_strip_cmap_rep,
                                                bksnap_strip_cmap_unfold,
                                                bksnap_strip_cmap_dir,
                                                bksnap_strip_cmap_color_style);
    }
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}

REGISTER_EFFECT_3D(BeatKernelSnap)
