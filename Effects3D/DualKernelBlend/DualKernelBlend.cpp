// SPDX-License-Identifier: GPL-2.0-only

#include "DualKernelBlend.h"
#include "SpatialKernelColormap.h"
#include "StripKernelColormapPanel.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace
{
RGBColor LerpRgbColor(RGBColor a, RGBColor b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    int ra = (int)(a & 0xFF), ga = (int)((a >> 8) & 0xFF), ba = (int)((a >> 16) & 0xFF);
    int rb = (int)(b & 0xFF), gb = (int)((b >> 8) & 0xFF), bb = (int)((b >> 16) & 0xFF);
    int r = (int)(ra * (1.0f - t) + rb * t + 0.5f);
    int g = (int)(ga * (1.0f - t) + gb * t + 0.5f);
    int bl = (int)(ba * (1.0f - t) + bb * t + 0.5f);
    return (RGBColor)((std::min(255, bl) << 16) | (std::min(255, g) << 8) | std::min(255, r));
}
} // namespace

DualKernelBlend::DualKernelBlend(QWidget* parent)
    : SpatialEffect3D(parent)
{
    SetRainbowMode(false);
}

EffectInfo3D DualKernelBlend::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 1;
    info.effect_name = "Dual Kernel Blend";
    info.effect_description = "Cross-fade two strip kernels (or kernel vs palette) using height, a slider, or stratum.";
    info.category = "Spatial";
    info.is_reversible = false;
    info.supports_random = true;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 3;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
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

void DualKernelBlend::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* bm_row = new QHBoxLayout();
    bm_row->addWidget(new QLabel("Blend driver:"));
    QComboBox* bm_combo = new QComboBox();
    bm_combo->addItem("Height (Y)");
    bm_combo->addItem("Manual slider");
    bm_combo->addItem("Stratum (mid band)");
    bm_combo->setCurrentIndex(std::clamp(blend_mode, 0, 2));
    bm_row->addWidget(bm_combo);
    vbox->addLayout(bm_row);
    connect(bm_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        blend_mode = std::clamp(idx, 0, 2);
        emit ParametersChanged();
    });

    QHBoxLayout* bl_row = new QHBoxLayout();
    bl_row->addWidget(new QLabel("Blend mix:"));
    QSlider* bl_slider = new QSlider(Qt::Horizontal);
    bl_slider->setRange(0, 1000);
    bl_slider->setValue((int)(blend_slider * 1000.0f));
    QLabel* bl_label = new QLabel(QString::number(blend_slider, 'f', 2));
    bl_row->addWidget(bl_slider);
    bl_row->addWidget(bl_label);
    vbox->addLayout(bl_row);
    connect(bl_slider, &QSlider::valueChanged, this, [this, bl_label](int v) {
        blend_slider = v / 1000.0f;
        bl_label->setText(QString::number(blend_slider, 'f', 2));
        emit ParametersChanged();
    });

    QGroupBox* ga = new QGroupBox("Kernel A");
    QVBoxLayout* la = new QVBoxLayout(ga);
    strip_a_panel = new StripKernelColormapPanel(ga);
    strip_a_panel->mirrorStateFromEffect(dual_a_on, dual_a_kernel, dual_a_rep, dual_a_unfold, dual_a_dir, dual_a_color_style);
    la->addWidget(strip_a_panel);
    vbox->addWidget(ga);

    QGroupBox* gb = new QGroupBox("Kernel B");
    QVBoxLayout* lb = new QVBoxLayout(gb);
    strip_b_panel = new StripKernelColormapPanel(gb);
    strip_b_panel->mirrorStateFromEffect(dual_b_on, dual_b_kernel, dual_b_rep, dual_b_unfold, dual_b_dir, dual_b_color_style);
    lb->addWidget(strip_b_panel);
    vbox->addWidget(gb);

    connect(strip_a_panel, &StripKernelColormapPanel::colormapChanged, this, &DualKernelBlend::SyncStripColormapFromPanels);
    connect(strip_b_panel, &StripKernelColormapPanel::colormapChanged, this, &DualKernelBlend::SyncStripColormapFromPanels);

    stratum_panel = new StratumBandPanel(w);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    vbox->addWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &DualKernelBlend::OnStratumBandChanged);
    OnStratumBandChanged();

    AddWidgetToParent(w, parent);
}

void DualKernelBlend::SyncStripColormapFromPanels()
{
    if(strip_a_panel)
    {
        dual_a_on = strip_a_panel->useStripColormap();
        dual_a_kernel = strip_a_panel->kernelId();
        dual_a_rep = strip_a_panel->kernelRepeats();
        dual_a_unfold = strip_a_panel->unfoldMode();
        dual_a_dir = strip_a_panel->directionDeg();
        dual_a_color_style = strip_a_panel->colorStyle();
    }
    if(strip_b_panel)
    {
        dual_b_on = strip_b_panel->useStripColormap();
        dual_b_kernel = strip_b_panel->kernelId();
        dual_b_rep = strip_b_panel->kernelRepeats();
        dual_b_unfold = strip_b_panel->unfoldMode();
        dual_b_dir = strip_b_panel->directionDeg();
        dual_b_color_style = strip_b_panel->colorStyle();
    }
    emit ParametersChanged();
}

void DualKernelBlend::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
}

float DualKernelBlend::SamplePalette01ForSide(bool strip_on,
                                                int kern,
                                                float rep,
                                                int unfold,
                                                float dir,
                                                float phase01,
                                                float time,
                                                const GridContext3D& grid,
                                                float size_m,
                                                const Vector3D& origin,
                                                const Vector3D& rot,
                                                float spatial_palette_driver) const
{
    if(strip_on)
    {
        float p01 = SampleStripKernelPalette01(kern, rep, unfold, dir, phase01, time, grid, size_m, origin, rot);
        return std::clamp(p01, 0.0f, 1.0f);
    }
    return std::clamp(spatial_palette_driver, 0.0f, 1.0f);
}

void DualKernelBlend::UpdateParams(SpatialEffectParams& /*params*/) {}

RGBColor DualKernelBlend::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
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

    float w_mix = blend_slider;
    if(blend_mode == 0)
        w_mix = coord2;
    else if(blend_mode == 2)
        w_mix = sw[1];

    w_mix = std::clamp(w_mix, 0.0f, 1.0f);

    const float size_m = GetNormalizedSize();
    const float ph01 =
        std::fmod(time * (0.04f + 0.03f * GetScaledFrequency() * 0.01f) * bb.speed_mul + coord1 * 0.07f +
                      coord2 * 0.06f + coord3 * 0.05f + bb.phase_deg * (1.f / 360.f) + 1.f,
                  1.f);

    float pal_drv = std::fmod(0.5f * coord1 + 0.3f * coord3 + ph01 * 0.25f + bb.phase_deg * (1.f / 360.f), 1.0f);
    if(pal_drv < 0.0f)
        pal_drv += 1.0f;

    float p01a = SamplePalette01ForSide(dual_a_on,
                                        dual_a_kernel,
                                        dual_a_rep,
                                        dual_a_unfold,
                                        dual_a_dir,
                                        ph01,
                                        time,
                                        grid,
                                        size_m,
                                        origin,
                                        rp,
                                        pal_drv);
    float p01b = SamplePalette01ForSide(dual_b_on,
                                        dual_b_kernel,
                                        dual_b_rep,
                                        dual_b_unfold,
                                        dual_b_dir,
                                        ph01,
                                        time,
                                        grid,
                                        size_m,
                                        origin,
                                        rp,
                                        std::fmod(pal_drv + 0.5f, 1.0f));

    float p01a_v = ApplyVoxelDriveToPalette01(p01a, x, y, z, time, grid);
    float p01b_v = ApplyVoxelDriveToPalette01(p01b, x, y, z, time, grid);
    const float rbow_mul = GetScaledFrequency() * 12.0f * bb.speed_mul;

    auto side_rgb = [&](bool on, int kern, int cstyle, float pv) -> RGBColor {
        if(on)
            return ResolveStripKernelFinalColor(*this, kern, pv, cstyle, time, rbow_mul);
        if(GetRainbowMode())
            return GetRainbowColor(pv * 360.0f);
        return GetColorAtPosition(pv);
    };

    RGBColor ca = side_rgb(dual_a_on, dual_a_kernel, dual_a_color_style, p01a_v);
    RGBColor cb = side_rgb(dual_b_on, dual_b_kernel, dual_b_color_style, p01b_v);
    RGBColor rgb = LerpRgbColor(ca, cb, w_mix);

    return PostProcessColorGrid(rgb);
}

nlohmann::json DualKernelBlend::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["dual_blend_mode"] = blend_mode;
    j["dual_blend_slider"] = blend_slider;
    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j,
                                           "dual_stratum_layout_mode",
                                           sm,
                                           st,
                                           "dual_stratum_band_speed_pct",
                                           "dual_stratum_band_tight_pct",
                                           "dual_stratum_band_phase_deg");
    StripColormapSaveJson(j, "dual_a", dual_a_on, dual_a_kernel, dual_a_rep, dual_a_unfold, dual_a_dir, dual_a_color_style);
    StripColormapSaveJson(j, "dual_b", dual_b_on, dual_b_kernel, dual_b_rep, dual_b_unfold, dual_b_dir, dual_b_color_style);
    return j;
}

void DualKernelBlend::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("dual_blend_mode"))
        blend_mode = std::clamp(settings["dual_blend_mode"].get<int>(), 0, 2);
    if(settings.contains("dual_blend_slider"))
        blend_slider = std::clamp(settings["dual_blend_slider"].get<float>(), 0.0f, 1.0f);
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "dual_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "dual_stratum_band_speed_pct",
                                            "dual_stratum_band_tight_pct",
                                            "dual_stratum_band_phase_deg");
    StripColormapLoadJson(settings, "dual_a", dual_a_on, dual_a_kernel, dual_a_rep, dual_a_unfold, dual_a_dir,
                          dual_a_color_style, GetRainbowMode());
    StripColormapLoadJson(settings, "dual_b", dual_b_on, dual_b_kernel, dual_b_rep, dual_b_unfold, dual_b_dir,
                          dual_b_color_style, GetRainbowMode());
    if(strip_a_panel)
    {
        strip_a_panel->mirrorStateFromEffect(dual_a_on, dual_a_kernel, dual_a_rep, dual_a_unfold, dual_a_dir, dual_a_color_style);
    }
    if(strip_b_panel)
    {
        strip_b_panel->mirrorStateFromEffect(dual_b_on, dual_b_kernel, dual_b_rep, dual_b_unfold, dual_b_dir, dual_b_color_style);
    }
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}

REGISTER_EFFECT_3D(DualKernelBlend)
