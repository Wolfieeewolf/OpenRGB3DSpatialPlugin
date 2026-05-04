// SPDX-License-Identifier: GPL-2.0-only

#include "SpiralStaircaseKernel.h"
#include "SpatialKernelColormap.h"
#include "StripKernelColormapPanel.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

SpiralStaircaseKernel::SpiralStaircaseKernel(QWidget* parent)
    : SpatialEffect3D(parent)
{
    SetRainbowMode(false);
}

EffectInfo3D SpiralStaircaseKernel::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 1;
    info.effect_name = "Spiral Staircase Kernel";
    info.effect_description =
        "Helix-style phase combines azimuth and height so strip kernels read as a path climbing the room.";
    info.category = "Spatial";
    info.is_reversible = false;
    info.supports_random = true;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.needs_frequency = true;
    info.default_frequency_scale = 10.0f;
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

void SpiralStaircaseKernel::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* tw_row = new QHBoxLayout();
    tw_row->addWidget(new QLabel("Turn weight:"));
    QSlider* tw_slider = new QSlider(Qt::Horizontal);
    tw_slider->setRange(10, 500);
    tw_slider->setValue((int)(turn_weight * 100.0f));
    QLabel* tw_label = new QLabel(QString::number(turn_weight, 'f', 2));
    tw_row->addWidget(tw_slider);
    tw_row->addWidget(tw_label);
    vbox->addLayout(tw_row);
    connect(tw_slider, &QSlider::valueChanged, this, [this, tw_label](int v) {
        turn_weight = v / 100.0f;
        tw_label->setText(QString::number(turn_weight, 'f', 2));
        emit ParametersChanged();
    });

    QHBoxLayout* pw_row = new QHBoxLayout();
    pw_row->addWidget(new QLabel("Pitch weight:"));
    QSlider* pw_slider = new QSlider(Qt::Horizontal);
    pw_slider->setRange(10, 500);
    pw_slider->setValue((int)(pitch_weight * 100.0f));
    QLabel* pw_label = new QLabel(QString::number(pitch_weight, 'f', 2));
    pw_row->addWidget(pw_slider);
    pw_row->addWidget(pw_label);
    vbox->addLayout(pw_row);
    connect(pw_slider, &QSlider::valueChanged, this, [this, pw_label](int v) {
        pitch_weight = v / 100.0f;
        pw_label->setText(QString::number(pitch_weight, 'f', 2));
        emit ParametersChanged();
    });

    strip_cmap_panel = new StripKernelColormapPanel(w);
    strip_cmap_panel->mirrorStateFromEffect(spiral_strip_cmap_on,
                                            spiral_strip_cmap_kernel,
                                            spiral_strip_cmap_rep,
                                            spiral_strip_cmap_unfold,
                                            spiral_strip_cmap_dir,
                                            spiral_strip_cmap_color_style);
    vbox->addWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &SpiralStaircaseKernel::SyncStripColormapFromPanel);

    stratum_panel = new StratumBandPanel(w);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    vbox->addWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &SpiralStaircaseKernel::OnStratumBandChanged);
    OnStratumBandChanged();

    AddWidgetToParent(w, parent);
}

void SpiralStaircaseKernel::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    spiral_strip_cmap_on = strip_cmap_panel->useStripColormap();
    spiral_strip_cmap_kernel = strip_cmap_panel->kernelId();
    spiral_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    spiral_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    spiral_strip_cmap_dir = strip_cmap_panel->directionDeg();
    spiral_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
}

void SpiralStaircaseKernel::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
}

void SpiralStaircaseKernel::UpdateParams(SpatialEffectParams& /*params*/) {}

RGBColor SpiralStaircaseKernel::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
        return 0x00000000;

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

    float dx = rp.x - origin.x;
    float dz = rp.z - origin.z;
    float angle = std::atan2(dz, dx);
    float arc01 = (float)(angle / (2.0 * M_PI)) * turn_weight + coord2 * pitch_weight;
    arc01 += time * (0.02f + 0.015f * GetScaledSpeed() * 0.01f) * bb.speed_mul;
    arc01 += bb.phase_deg * (1.f / 360.f);
    float ph01 = std::fmod(arc01 + 1000.0f, 1.0f);

    const float size_m = GetNormalizedSize();
    RGBColor rgb = 0x00000000;
    if(spiral_strip_cmap_on)
    {
        float p01 = SampleStripKernelPalette01(spiral_strip_cmap_kernel,
                                               spiral_strip_cmap_rep,
                                               spiral_strip_cmap_unfold,
                                               spiral_strip_cmap_dir,
                                               ph01,
                                               time,
                                               grid,
                                               size_m,
                                               origin,
                                               rp);
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        rgb = ResolveStripKernelFinalColor(*this, spiral_strip_cmap_kernel, p01, spiral_strip_cmap_color_style, time,
                                          GetScaledFrequency() * 12.0f * bb.speed_mul);
    }
    else if(GetRainbowMode())
    {
        rgb = GetRainbowColor(ph01 * 360.0f);
    }
    else
    {
        rgb = GetColorAtPosition(ph01);
    }

    return PostProcessColorGrid(rgb);
}

nlohmann::json SpiralStaircaseKernel::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["spiral_turn_weight"] = turn_weight;
    j["spiral_pitch_weight"] = pitch_weight;
    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j,
                                           "spiral_stratum_layout_mode",
                                           sm,
                                           st,
                                           "spiral_stratum_band_speed_pct",
                                           "spiral_stratum_band_tight_pct",
                                           "spiral_stratum_band_phase_deg");
    StripColormapSaveJson(j,
                          "spiral",
                          spiral_strip_cmap_on,
                          spiral_strip_cmap_kernel,
                          spiral_strip_cmap_rep,
                          spiral_strip_cmap_unfold,
                          spiral_strip_cmap_dir,
                          spiral_strip_cmap_color_style);
    return j;
}

void SpiralStaircaseKernel::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("spiral_turn_weight"))
        turn_weight = std::clamp(settings["spiral_turn_weight"].get<float>(), 0.1f, 6.0f);
    if(settings.contains("spiral_pitch_weight"))
        pitch_weight = std::clamp(settings["spiral_pitch_weight"].get<float>(), 0.1f, 6.0f);
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "spiral_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "spiral_stratum_band_speed_pct",
                                            "spiral_stratum_band_tight_pct",
                                            "spiral_stratum_band_phase_deg");
    StripColormapLoadJson(settings,
                          "spiral",
                          spiral_strip_cmap_on,
                          spiral_strip_cmap_kernel,
                          spiral_strip_cmap_rep,
                          spiral_strip_cmap_unfold,
                          spiral_strip_cmap_dir,
                          spiral_strip_cmap_color_style,
                          GetRainbowMode());
    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(spiral_strip_cmap_on,
                                                spiral_strip_cmap_kernel,
                                                spiral_strip_cmap_rep,
                                                spiral_strip_cmap_unfold,
                                                spiral_strip_cmap_dir,
                                                spiral_strip_cmap_color_style);
    }
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}

REGISTER_EFFECT_3D(SpiralStaircaseKernel)
