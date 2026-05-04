// SPDX-License-Identifier: GPL-2.0-only

#include "RoomScannerKernel.h"
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

RoomScannerKernel::RoomScannerKernel(QWidget* parent)
    : SpatialEffect3D(parent)
{
    SetRainbowMode(false);
}

EffectInfo3D RoomScannerKernel::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 1;
    info.effect_name = "Room Scanner Kernel";
    info.effect_description =
        "A moving plane slices the volume—only LEDs near the plane show the strip kernel; elsewhere dark.";
    info.category = "Spatial";
    info.is_reversible = false;
    info.supports_random = true;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_frequency = true;
    info.default_frequency_scale = 14.0f;
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

void RoomScannerKernel::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* ax_row = new QHBoxLayout();
    ax_row->addWidget(new QLabel("Plane normal:"));
    QComboBox* ax_combo = new QComboBox();
    ax_combo->addItem("X");
    ax_combo->addItem("Y");
    ax_combo->addItem("Z");
    ax_combo->setCurrentIndex(std::clamp(plane_axis, 0, 2));
    ax_row->addWidget(ax_combo);
    vbox->addLayout(ax_row);
    connect(ax_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        plane_axis = std::clamp(idx, 0, 2);
        emit ParametersChanged();
    });

    QHBoxLayout* th_row = new QHBoxLayout();
    th_row->addWidget(new QLabel("Slice thickness:"));
    QSlider* th_slider = new QSlider(Qt::Horizontal);
    th_slider->setRange(5, 120);
    th_slider->setValue((int)(slice_thickness * 1000.0f));
    QLabel* th_label = new QLabel(QString::number(slice_thickness, 'f', 3));
    th_row->addWidget(th_slider);
    th_row->addWidget(th_label);
    vbox->addLayout(th_row);
    connect(th_slider, &QSlider::valueChanged, this, [this, th_label](int v) {
        slice_thickness = v / 1000.0f;
        th_label->setText(QString::number(slice_thickness, 'f', 3));
        emit ParametersChanged();
    });

    QHBoxLayout* ph_row = new QHBoxLayout();
    ph_row->addWidget(new QLabel("Scan phase offset:"));
    QSlider* ph_slider = new QSlider(Qt::Horizontal);
    ph_slider->setRange(0, 1000);
    ph_slider->setValue((int)(scan_phase_off * 1000.0f));
    QLabel* ph_label = new QLabel(QString::number(scan_phase_off, 'f', 2));
    ph_row->addWidget(ph_slider);
    ph_row->addWidget(ph_label);
    vbox->addLayout(ph_row);
    connect(ph_slider, &QSlider::valueChanged, this, [this, ph_label](int v) {
        scan_phase_off = v / 1000.0f;
        ph_label->setText(QString::number(scan_phase_off, 'f', 2));
        emit ParametersChanged();
    });

    strip_cmap_panel = new StripKernelColormapPanel(w);
    strip_cmap_panel->mirrorStateFromEffect(rscan_strip_cmap_on,
                                            rscan_strip_cmap_kernel,
                                            rscan_strip_cmap_rep,
                                            rscan_strip_cmap_unfold,
                                            rscan_strip_cmap_dir,
                                            rscan_strip_cmap_color_style);
    vbox->addWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &RoomScannerKernel::SyncStripColormapFromPanel);

    stratum_panel = new StratumBandPanel(w);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    vbox->addWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &RoomScannerKernel::OnStratumBandChanged);
    OnStratumBandChanged();

    AddWidgetToParent(w, parent);
}

void RoomScannerKernel::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    rscan_strip_cmap_on = strip_cmap_panel->useStripColormap();
    rscan_strip_cmap_kernel = strip_cmap_panel->kernelId();
    rscan_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    rscan_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    rscan_strip_cmap_dir = strip_cmap_panel->directionDeg();
    rscan_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
}

void RoomScannerKernel::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
}

void RoomScannerKernel::UpdateParams(SpatialEffectParams& /*params*/) {}

RGBColor RoomScannerKernel::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
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

    float nx = 0.0f, ny = 0.0f, nz = 0.0f;
    float span = 1.0f;
    float cx = (grid.min_x + grid.max_x) * 0.5f;
    float cy = (grid.min_y + grid.max_y) * 0.5f;
    float cz = (grid.min_z + grid.max_z) * 0.5f;
    if(plane_axis == 0)
    {
        nx = 1.0f;
        span = grid.width;
    }
    else if(plane_axis == 1)
    {
        ny = 1.0f;
        span = grid.height;
    }
    else
    {
        nz = 1.0f;
        span = grid.depth;
    }

    float med = EffectGridMedianHalfExtent(grid, GetNormalizedScale());
    float thick = slice_thickness * (0.5f + 0.5f * med);

    float wave = std::sin((time * (0.35f + 0.2f * GetScaledSpeed() * 0.01f) + scan_phase_off * 6.2831853f) *
                          GetScaledFrequency() * 0.03f * bb.speed_mul +
                      bb.phase_deg * (3.14159265f / 180.f));
    float offset = wave * span * 0.42f;

    float px = x - cx - nx * offset;
    float py = y - cy - ny * offset;
    float pz = z - cz - nz * offset;
    float dist = std::fabs(px * nx + py * ny + pz * nz);
    float edge = std::clamp(1.0f - dist / std::max(1e-4f, thick), 0.0f, 1.0f);
    float smooth = edge * edge * (3.0f - 2.0f * edge);
    if(smooth < 0.02f)
        return 0x00000000;

    const float size_m = GetNormalizedSize();
    const float ph01 =
        std::fmod(dist / std::max(1e-4f, thick) * 0.35f + time * 0.05f * bb.speed_mul + coord2 * 0.08f +
                      bb.phase_deg * (1.f / 360.f) + 1.f,
                  1.f);

    RGBColor rgb = 0x00000000;
    if(rscan_strip_cmap_on)
    {
        float p01 = SampleStripKernelPalette01(rscan_strip_cmap_kernel,
                                               rscan_strip_cmap_rep,
                                               rscan_strip_cmap_unfold,
                                               rscan_strip_cmap_dir,
                                               ph01,
                                               time,
                                               grid,
                                               size_m,
                                               origin,
                                               rp);
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        rgb = ResolveStripKernelFinalColor(*this, rscan_strip_cmap_kernel, p01, rscan_strip_cmap_color_style, time,
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

    return PostProcessColorGrid(ScaleRGBColor(rgb, smooth));
}

nlohmann::json RoomScannerKernel::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["rscan_plane_axis"] = plane_axis;
    j["rscan_slice_thickness"] = slice_thickness;
    j["rscan_scan_phase_off"] = scan_phase_off;
    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j,
                                           "rscan_stratum_layout_mode",
                                           sm,
                                           st,
                                           "rscan_stratum_band_speed_pct",
                                           "rscan_stratum_band_tight_pct",
                                           "rscan_stratum_band_phase_deg");
    StripColormapSaveJson(j,
                          "rscan",
                          rscan_strip_cmap_on,
                          rscan_strip_cmap_kernel,
                          rscan_strip_cmap_rep,
                          rscan_strip_cmap_unfold,
                          rscan_strip_cmap_dir,
                          rscan_strip_cmap_color_style);
    return j;
}

void RoomScannerKernel::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("rscan_plane_axis"))
        plane_axis = std::clamp(settings["rscan_plane_axis"].get<int>(), 0, 2);
    if(settings.contains("rscan_slice_thickness"))
        slice_thickness = std::clamp(settings["rscan_slice_thickness"].get<float>(), 0.005f, 0.12f);
    if(settings.contains("rscan_scan_phase_off"))
        scan_phase_off = std::clamp(settings["rscan_scan_phase_off"].get<float>(), 0.0f, 1.0f);
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "rscan_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "rscan_stratum_band_speed_pct",
                                            "rscan_stratum_band_tight_pct",
                                            "rscan_stratum_band_phase_deg");
    StripColormapLoadJson(settings,
                          "rscan",
                          rscan_strip_cmap_on,
                          rscan_strip_cmap_kernel,
                          rscan_strip_cmap_rep,
                          rscan_strip_cmap_unfold,
                          rscan_strip_cmap_dir,
                          rscan_strip_cmap_color_style,
                          GetRainbowMode());
    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(rscan_strip_cmap_on,
                                                rscan_strip_cmap_kernel,
                                                rscan_strip_cmap_rep,
                                                rscan_strip_cmap_unfold,
                                                rscan_strip_cmap_dir,
                                                rscan_strip_cmap_color_style);
    }
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}

REGISTER_EFFECT_3D(RoomScannerKernel)
