// SPDX-License-Identifier: GPL-2.0-only

#include "TravelingUnfoldKernel.h"
#include "Game/StripPatternSurface.h"
#include "SpatialKernelColormap.h"
#include "StripKernelColormapPanel.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <cmath>

TravelingUnfoldKernel::TravelingUnfoldKernel(QWidget* parent)
    : SpatialEffect3D(parent)
{
    SetRainbowMode(false);
}

EffectInfo3D TravelingUnfoldKernel::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 1;
    info.effect_name = "Traveling Unfold Kernel";
    info.effect_description =
        "Strip kernel with spinning direction and optional stepped unfold cycling—pattern appears to unwrap through space.";
    info.category = "Spatial";
    info.is_reversible = false;
    info.supports_random = true;
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

void TravelingUnfoldKernel::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* spin_row = new QHBoxLayout();
    spin_row->addWidget(new QLabel("Dir spin (°/s):"));
    QSlider* spin_slider = new QSlider(Qt::Horizontal);
    spin_slider->setRange(-200, 200);
    spin_slider->setValue((int)dir_spin_deg_per_sec);
    QLabel* spin_label = new QLabel(QString::number((int)dir_spin_deg_per_sec));
    spin_row->addWidget(spin_slider);
    spin_row->addWidget(spin_label);
    vbox->addLayout(spin_row);
    connect(spin_slider, &QSlider::valueChanged, this, [this, spin_label](int v) {
        dir_spin_deg_per_sec = (float)v;
        spin_label->setText(QString::number(v));
        emit ParametersChanged();
    });

    QHBoxLayout* cy_row = new QHBoxLayout();
    cy_row->addWidget(new QLabel("Unfold step (s, 0=off):"));
    QSlider* cy_slider = new QSlider(Qt::Horizontal);
    cy_slider->setRange(0, 80);
    cy_slider->setValue((int)(unfold_cycle_sec * 10.0f));
    QLabel* cy_label = new QLabel(QString::number(unfold_cycle_sec, 'f', 1));
    cy_row->addWidget(cy_slider);
    cy_row->addWidget(cy_label);
    vbox->addLayout(cy_row);
    connect(cy_slider, &QSlider::valueChanged, this, [this, cy_label](int v) {
        unfold_cycle_sec = v / 10.0f;
        cy_label->setText(QString::number(unfold_cycle_sec, 'f', 1));
        emit ParametersChanged();
    });

    strip_cmap_panel = new StripKernelColormapPanel(w);
    strip_cmap_panel->mirrorStateFromEffect(tunf_strip_cmap_on,
                                            tunf_strip_cmap_kernel,
                                            tunf_strip_cmap_rep,
                                            tunf_strip_cmap_unfold,
                                            tunf_strip_cmap_dir,
                                            tunf_strip_cmap_color_style);
    vbox->addWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &TravelingUnfoldKernel::SyncStripColormapFromPanel);

    stratum_panel = new StratumBandPanel(w);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    vbox->addWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &TravelingUnfoldKernel::OnStratumBandChanged);
    OnStratumBandChanged();

    AddWidgetToParent(w, parent);
}

void TravelingUnfoldKernel::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    tunf_strip_cmap_on = strip_cmap_panel->useStripColormap();
    tunf_strip_cmap_kernel = strip_cmap_panel->kernelId();
    tunf_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    tunf_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    tunf_strip_cmap_dir = strip_cmap_panel->directionDeg();
    tunf_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
}

void TravelingUnfoldKernel::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
}

void TravelingUnfoldKernel::UpdateParams(SpatialEffectParams& /*params*/) {}

RGBColor TravelingUnfoldKernel::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
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

    const int unfold_count = (int)StripPatternSurface::UnfoldMode::COUNT;
    int eff_unfold = tunf_strip_cmap_unfold;
    if(unfold_cycle_sec > 0.05f)
    {
        int step = (int)(time / unfold_cycle_sec);
        eff_unfold = (tunf_strip_cmap_unfold + step) % unfold_count;
        if(eff_unfold < 0)
            eff_unfold += unfold_count;
    }

    float eff_dir = std::fmod(tunf_strip_cmap_dir + time * dir_spin_deg_per_sec * bb.speed_mul + 720.0f, 360.0f);

    const float size_m = GetNormalizedSize();
    const float ph01 =
        std::fmod(time * (0.045f + 0.02f * GetScaledFrequency() * 0.01f) * bb.speed_mul +
                      bb.phase_deg * (1.f / 360.f) + 1.f,
                  1.f);

    RGBColor rgb = 0x00000000;
    if(tunf_strip_cmap_on)
    {
        float p01 = SampleStripKernelPalette01(tunf_strip_cmap_kernel,
                                               tunf_strip_cmap_rep,
                                               eff_unfold,
                                               eff_dir,
                                               ph01,
                                               time,
                                               grid,
                                               size_m,
                                               origin,
                                               rp);
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        rgb = ResolveStripKernelFinalColor(*this, tunf_strip_cmap_kernel, p01, tunf_strip_cmap_color_style, time,
                                          GetScaledFrequency() * 12.0f * bb.speed_mul);
    }
    else if(GetRainbowMode())
    {
        rgb = GetRainbowColor(ph01 * 360.0f + eff_unfold * 40.0f);
    }
    else
    {
        rgb = GetColorAtPosition(ph01);
    }

    return PostProcessColorGrid(rgb);
}

nlohmann::json TravelingUnfoldKernel::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["tunf_dir_spin"] = dir_spin_deg_per_sec;
    j["tunf_unfold_cycle_sec"] = unfold_cycle_sec;
    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j,
                                           "tunf_stratum_layout_mode",
                                           sm,
                                           st,
                                           "tunf_stratum_band_speed_pct",
                                           "tunf_stratum_band_tight_pct",
                                           "tunf_stratum_band_phase_deg");
    StripColormapSaveJson(j,
                          "tunf",
                          tunf_strip_cmap_on,
                          tunf_strip_cmap_kernel,
                          tunf_strip_cmap_rep,
                          tunf_strip_cmap_unfold,
                          tunf_strip_cmap_dir,
                          tunf_strip_cmap_color_style);
    return j;
}

void TravelingUnfoldKernel::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("tunf_dir_spin"))
        dir_spin_deg_per_sec = std::clamp(settings["tunf_dir_spin"].get<float>(), -360.0f, 360.0f);
    if(settings.contains("tunf_unfold_cycle_sec"))
        unfold_cycle_sec = std::max(0.0f, settings["tunf_unfold_cycle_sec"].get<float>());
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "tunf_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "tunf_stratum_band_speed_pct",
                                            "tunf_stratum_band_tight_pct",
                                            "tunf_stratum_band_phase_deg");
    StripColormapLoadJson(settings,
                          "tunf",
                          tunf_strip_cmap_on,
                          tunf_strip_cmap_kernel,
                          tunf_strip_cmap_rep,
                          tunf_strip_cmap_unfold,
                          tunf_strip_cmap_dir,
                          tunf_strip_cmap_color_style,
                          GetRainbowMode());
    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(tunf_strip_cmap_on,
                                                tunf_strip_cmap_kernel,
                                                tunf_strip_cmap_rep,
                                                tunf_strip_cmap_unfold,
                                                tunf_strip_cmap_dir,
                                                tunf_strip_cmap_color_style);
    }
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}

REGISTER_EFFECT_3D(TravelingUnfoldKernel)
