// SPDX-License-Identifier: GPL-2.0-only

#include "KernelFogWash.h"
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
#include <cmath>

KernelFogWash::KernelFogWash(QWidget* parent)
    : SpatialEffect3D(parent)
{
    SetRainbowMode(false);
}

EffectInfo3D KernelFogWash::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 1;
    info.effect_name = "Kernel Fog Wash";
    info.effect_description =
        "Slow strip-kernel hues with brightness shaped by height or distance—volumetric colored mist.";
    info.category = "Spatial";
    info.is_reversible = false;
    info.supports_random = true;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 6.0f;
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

void KernelFogWash::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* mode_row = new QHBoxLayout();
    mode_row->addWidget(new QLabel("Brightness from:"));
    QComboBox* mode_combo = new QComboBox();
    mode_combo->addItem("Height (Y)");
    mode_combo->addItem("Distance from origin");
    mode_combo->setCurrentIndex(std::clamp(v_mode, 0, 1));
    mode_row->addWidget(mode_combo);
    vbox->addLayout(mode_row);
    connect(mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        v_mode = std::clamp(idx, 0, 1);
        emit ParametersChanged();
    });

    QHBoxLayout* con_row = new QHBoxLayout();
    con_row->addWidget(new QLabel("Mist contrast:"));
    QSlider* con_slider = new QSlider(Qt::Horizontal);
    con_slider->setRange(20, 100);
    con_slider->setValue((int)(mist_contrast * 100.0f));
    QLabel* con_label = new QLabel(QString::number(mist_contrast, 'f', 2));
    con_row->addWidget(con_slider);
    con_row->addWidget(con_label);
    vbox->addLayout(con_row);
    connect(con_slider, &QSlider::valueChanged, this, [this, con_label](int v) {
        mist_contrast = v / 100.0f;
        con_label->setText(QString::number(mist_contrast, 'f', 2));
        emit ParametersChanged();
    });

    strip_cmap_panel = new StripKernelColormapPanel(w);
    strip_cmap_panel->mirrorStateFromEffect(kfog_strip_cmap_on,
                                            kfog_strip_cmap_kernel,
                                            kfog_strip_cmap_rep,
                                            kfog_strip_cmap_unfold,
                                            kfog_strip_cmap_dir,
                                            kfog_strip_cmap_color_style);
    vbox->addWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &KernelFogWash::SyncStripColormapFromPanel);

    stratum_panel = new StratumBandPanel(w);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    vbox->addWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &KernelFogWash::OnStratumBandChanged);
    OnStratumBandChanged();

    AddWidgetToParent(w, parent);
}

void KernelFogWash::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    kfog_strip_cmap_on = strip_cmap_panel->useStripColormap();
    kfog_strip_cmap_kernel = strip_cmap_panel->kernelId();
    kfog_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    kfog_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    kfog_strip_cmap_dir = strip_cmap_panel->directionDeg();
    kfog_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
}

void KernelFogWash::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
}

void KernelFogWash::UpdateParams(SpatialEffectParams& /*params*/) {}

RGBColor KernelFogWash::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
        return 0x00000000;

    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    Vector3D rp = TransformPointByRotation(x, y, z, origin);
    float coord1 = NormalizeGridAxis01(rp.x, grid.min_x, grid.max_x);
    float coord2 = NormalizeGridAxis01(rp.y, grid.min_y, grid.max_y);
    float coord3 = NormalizeGridAxis01(rp.z, grid.min_z, grid.max_z);

    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(stratum_layout_mode, sw, stratum_tuning_);

    float dx = rp.x - origin.x, dy = rp.y - origin.y, dz = rp.z - origin.z;
    float max_r = EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f;
    float dist_norm = std::clamp(std::sqrt(dx * dx + dy * dy + dz * dz) / std::max(max_r, 1e-5f), 0.0f, 1.0f);

    float depth01 = (v_mode == 0) ? coord2 : dist_norm;
    float slow = std::sin(time * (0.35f + 0.25f * GetScaledSpeed() * 0.01f) * bb.speed_mul +
                          bb.phase_deg * (3.14159265f / 180.f));
    float grain = std::sin((coord1 * 2.1f + coord3 * 1.7f + time * 0.08f) * (6.0f + 4.0f * GetScaledDetail()) *
                           bb.tight_mul);
    float bright =
        std::clamp(
            (0.18f + 0.82f * std::pow(1.0f - 0.92f * depth01, 0.85f + mist_contrast * 0.6f)) *
                (0.52f + 0.48f * (0.5f + 0.5f * slow)) * (0.65f + 0.35f * (0.5f + 0.5f * grain)),
            0.0f, 1.0f);

    const float size_m = GetNormalizedSize();
    const float ph01 =
        std::fmod(time * (0.025f + 0.02f * GetScaledFrequency() * 0.01f) * bb.speed_mul +
                      coord1 * 0.04f + coord2 * 0.035f + coord3 * 0.03f + bb.phase_deg * (1.f / 360.f) + 1.f,
                  1.f);

    RGBColor rgb = 0x00000000;
    if(kfog_strip_cmap_on)
    {
        float p01 = SampleStripKernelPalette01(kfog_strip_cmap_kernel,
                                               kfog_strip_cmap_rep,
                                               kfog_strip_cmap_unfold,
                                               kfog_strip_cmap_dir,
                                               ph01,
                                               time,
                                               grid,
                                               size_m,
                                               origin,
                                               rp);
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        rgb = ResolveStripKernelFinalColor(*this, kfog_strip_cmap_kernel, p01, kfog_strip_cmap_color_style, time,
                                           GetScaledFrequency() * 12.0f * bb.speed_mul);
    }
    else if(GetRainbowMode())
    {
        float hue = ph01 * 360.0f + bb.phase_deg;
        rgb = GetRainbowColor(hue);
    }
    else
    {
        rgb = GetColorAtPosition(ph01);
    }

    return PostProcessColorGrid(ScaleRGBColor(rgb, bright));
}

nlohmann::json KernelFogWash::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["kfog_v_mode"] = v_mode;
    j["kfog_mist_contrast"] = mist_contrast;
    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j,
                                           "kfog_stratum_layout_mode",
                                           sm,
                                           st,
                                           "kfog_stratum_band_speed_pct",
                                           "kfog_stratum_band_tight_pct",
                                           "kfog_stratum_band_phase_deg");
    StripColormapSaveJson(j,
                          "kfog",
                          kfog_strip_cmap_on,
                          kfog_strip_cmap_kernel,
                          kfog_strip_cmap_rep,
                          kfog_strip_cmap_unfold,
                          kfog_strip_cmap_dir,
                          kfog_strip_cmap_color_style);
    return j;
}

void KernelFogWash::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("kfog_v_mode"))
        v_mode = std::clamp(settings["kfog_v_mode"].get<int>(), 0, 1);
    if(settings.contains("kfog_mist_contrast"))
        mist_contrast = std::clamp(settings["kfog_mist_contrast"].get<float>(), 0.2f, 1.0f);
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "kfog_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "kfog_stratum_band_speed_pct",
                                            "kfog_stratum_band_tight_pct",
                                            "kfog_stratum_band_phase_deg");
    StripColormapLoadJson(settings,
                          "kfog",
                          kfog_strip_cmap_on,
                          kfog_strip_cmap_kernel,
                          kfog_strip_cmap_rep,
                          kfog_strip_cmap_unfold,
                          kfog_strip_cmap_dir,
                          kfog_strip_cmap_color_style,
                          GetRainbowMode());
    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(kfog_strip_cmap_on,
                                                kfog_strip_cmap_kernel,
                                                kfog_strip_cmap_rep,
                                                kfog_strip_cmap_unfold,
                                                kfog_strip_cmap_dir,
                                                kfog_strip_cmap_color_style);
    }
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}

REGISTER_EFFECT_3D(KernelFogWash)
