// SPDX-License-Identifier: GPL-2.0-only

#include "PulseRing.h"
#include "SpatialKernelColormap.h"
#include "StripKernelColormapPanel.h"
#include "StratumBandPanel.h"
#include "EffectHelpers.h"
#include "SpatialLayerCore.h"
#include <algorithm>
#include <cmath>
#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

REGISTER_EFFECT_3D(PulseRing);

const char* PulseRing::StyleName(int s)
{
    switch(s) { case STYLE_PULSE_RING: return "Pulse Ring"; case STYLE_RADIAL_RAINBOW: return "Radial Rainbow"; default: return "Pulse Ring"; }
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

PulseRing::PulseRing(QWidget* parent) : SpatialEffect3D(parent) {}

EffectInfo3D PulseRing::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Pulse Ring";
    info.effect_description =
        "Pulsing donut / radial rainbow; optional floor/mid/ceiling band tuning for motion and spatial freq";
    info.category = "Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 8.0f;
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

void PulseRing::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* outer = new QVBoxLayout(w);
    outer->setContentsMargins(0, 0, 0, 0);
    QGridLayout* layout = new QGridLayout();
    outer->addLayout(layout);
    int row = 0;
    layout->addWidget(new QLabel("Style:"), row, 0);
    QComboBox* style_combo = new QComboBox();
    for(int s = 0; s < STYLE_COUNT; s++) style_combo->addItem(StyleName(s));
    style_combo->setCurrentIndex(std::max(0, std::min(ring_style, STYLE_COUNT - 1)));
    style_combo->setToolTip("Pulse Ring = expanding donut; Radial Rainbow = hue by angle from center.");
    style_combo->setItemData(0, "Timed rings with configurable thickness and inner hole.", Qt::ToolTipRole);
    style_combo->setItemData(1, "Rainbow locked to horizontal angle—strong on floors and rings of LEDs.", Qt::ToolTipRole);
    layout->addWidget(style_combo, row, 1, 1, 2);
    connect(style_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx){
        ring_style = std::max(0, std::min(idx, STYLE_COUNT - 1));
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Ring thickness:"), row, 0);
    QSlider* thick_slider = new QSlider(Qt::Horizontal);
    thick_slider->setRange(2, 100);
    thick_slider->setToolTip("Gaussian width of the ring band (Pulse Ring style only).");
    thick_slider->setValue((int)(ring_thickness * 100.0f));
    QLabel* thick_label = new QLabel(QString::number((int)(ring_thickness * 100)) + "%");
    thick_label->setMinimumWidth(36);
    layout->addWidget(thick_slider, row, 1);
    layout->addWidget(thick_label, row, 2);
    connect(thick_slider, &QSlider::valueChanged, this, [this, thick_label](int v){
        ring_thickness = v / 100.0f;
        if(thick_label) thick_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Hole size:"), row, 0);
    QSlider* hole_slider = new QSlider(Qt::Horizontal);
    hole_slider->setRange(0, 80);
    hole_slider->setToolTip("Dark core radius inside the ring (fraction of horizontal span).");
    hole_slider->setValue((int)(hole_size * 100.0f));
    QLabel* hole_label = new QLabel(QString::number((int)(hole_size * 100)) + "%");
    hole_label->setMinimumWidth(36);
    layout->addWidget(hole_slider, row, 1);
    layout->addWidget(hole_label, row, 2);
    connect(hole_slider, &QSlider::valueChanged, this, [this, hole_label](int v){
        hole_size = v / 100.0f;
        if(hole_label) hole_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Pulse amplitude:"), row, 0);
    QSlider* amp_slider = new QSlider(Qt::Horizontal);
    amp_slider->setRange(20, 200);
    amp_slider->setToolTip("How strongly the ring front modulates (Pulse Ring style).");
    amp_slider->setValue((int)(pulse_amplitude * 100.0f));
    QLabel* amp_label = new QLabel(QString::number((int)(pulse_amplitude * 100)) + "%");
    amp_label->setMinimumWidth(36);
    layout->addWidget(amp_slider, row, 1);
    layout->addWidget(amp_label, row, 2);
    connect(amp_slider, &QSlider::valueChanged, this, [this, amp_label](int v){
        pulse_amplitude = v / 100.0f;
        if(amp_label) amp_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Direction:"), row, 0);
    QSlider* dir_slider = new QSlider(Qt::Horizontal);
    dir_slider->setRange(0, 360);
    dir_slider->setToolTip("Rotates where the expanding ring starts in the cycle (Pulse Ring style).");
    dir_slider->setValue((int)direction_deg);
    QLabel* dir_label = new QLabel(QString::number((int)direction_deg) + "°");
    dir_label->setMinimumWidth(36);
    layout->addWidget(dir_slider, row, 1);
    layout->addWidget(dir_label, row, 2);
    connect(dir_slider, &QSlider::valueChanged, this, [this, dir_label](int v){
        direction_deg = (float)v;
        if(dir_label) dir_label->setText(QString::number(v) + "°");
        emit ParametersChanged();
    });
    strip_cmap_panel = new StripKernelColormapPanel(w);
    strip_cmap_panel->mirrorStateFromEffect(pulsering_strip_cmap_on,
                                            pulsering_strip_cmap_kernel,
                                            pulsering_strip_cmap_rep,
                                            pulsering_strip_cmap_unfold,
                                            pulsering_strip_cmap_dir,
                                            pulsering_strip_cmap_color_style);
    AddColorPatternWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &PulseRing::SyncStripColormapFromPanel);
    stratum_panel = new StratumBandPanel(w);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    AddBandModulationWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &PulseRing::OnStratumBandChanged);
    OnStratumBandChanged();
    AddWidgetToParent(w, parent);
}

void PulseRing::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
}

void PulseRing::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    pulsering_strip_cmap_on = strip_cmap_panel->useStripColormap();
    pulsering_strip_cmap_kernel = strip_cmap_panel->kernelId();
    pulsering_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    pulsering_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    pulsering_strip_cmap_dir = strip_cmap_panel->directionDeg();
    pulsering_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
}

void PulseRing::UpdateParams(SpatialEffectParams& params) { (void)params; }


RGBColor PulseRing::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float progress_raw = CalculateProgress(time);
    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    float coord2 = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float stratum_w[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, stratum_w);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(stratum_layout_mode, stratum_w, stratum_tuning_);
    float progress = progress_raw * bb.speed_mul;
    EffectGridAxisHalfExtents e = MakeEffectGridAxisHalfExtents(grid, GetNormalizedScale());
    constexpr float kRingHorizontalFill = 1.0f / 3.0f;
    float hw = std::max(1e-6f, e.hw * kRingHorizontalFill);
    float hd = std::max(1e-6f, e.hd * kRingHorizontalFill);
    float r_corner = EffectGridHorizontalRadialNormXZ(rot.x - origin.x, rot.z - origin.z, hw, hd);
    float r = EffectGridHorizontalRadialNorm01(r_corner);
    float hole_r = std::max(0.0f, std::min(0.8f, hole_size));
    float max_r = 1.0f;
    float usable = std::max(0.01f, max_r - hole_r);
    float pos_norm = (r - hole_r) / usable;
    pos_norm = std::max(0.0f, std::min(1.0f, pos_norm));

    int style = std::max(0, std::min(ring_style, STYLE_COUNT - 1));
    float intensity = 1.0f;

    if(style == STYLE_RADIAL_RAINBOW)
    {
        if(r < hole_r - 0.02f) return 0x00000000;
        intensity = 1.0f;
    }
    else
    {
        float phase = progress * (float)(2.0 * M_PI);
        float detail = std::max(0.05f, GetScaledDetail());
        float freq = std::max(0.3f, std::min(6.0f, GetScaledFrequency() * 0.15f * bb.tight_mul));
        float amp = std::max(0.2f, std::min(2.0f, pulse_amplitude));
        float sigma = std::max(ring_thickness, 0.02f);
        float phase_offset = direction_deg / 360.0f + bb.phase_deg * (1.0f / 360.0f);
        float expand_progress = fmodf(progress + phase_offset, 1.0f);
        float ring_center = hole_r + expand_progress * usable;
        float d = fabsf(r - ring_center);
        const float d_cutoff = 3.0f * sigma * std::max(1.0f, amp);
        if(d > d_cutoff) return 0x00000000;
        if(r < hole_r - 0.02f) return 0x00000000;
        intensity = expf(-d * d / ((sigma / (0.6f + 0.4f * detail)) * (sigma / (0.6f + 0.4f * detail))));
        float pulse_mod = 0.5f + 0.5f * sinf(phase * freq);
        intensity *= amp * pulse_mod;
    }
    intensity = std::min(1.0f, std::max(0.0f, intensity));

    float azimuth_deg = atan2f(rot.z - origin.z, rot.x - origin.x) * 57.2957795f;

    SpatialLayerCore::Basis basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
    SpatialLayerCore::MapperSettings map;
    EffectStratumBlend::InitStratumBreaks(map);
    map.blend_softness = std::clamp(0.10f, 0.05f, 0.22f);
    map.center_size = std::clamp(0.11f + 0.24f * GetNormalizedScale(), 0.06f, 0.52f);
    map.directional_sharpness = std::clamp(1.05f + std::max(0.05f, GetScaledDetail()) * 0.08f, 0.9f, 2.3f);

    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = x;
    sp.grid_y = y;
    sp.grid_z = z;
    sp.origin_x = origin.x;
    sp.origin_y = origin.y;
    sp.origin_z = origin.z;
    sp.y_norm = coord2;

    const float pr_phase01 = std::fmod(progress + bb.phase_deg * (1.0f / 360.0f) + 1.0f, 1.0f);
    float strip_p01 = 0.0f;
    if(pulsering_strip_cmap_on)
    {
        strip_p01 = SampleStripKernelPalette01(pulsering_strip_cmap_kernel,
                                               pulsering_strip_cmap_rep,
                                               pulsering_strip_cmap_unfold,
                                               pulsering_strip_cmap_dir,
                                               pr_phase01,
                                               time,
                                               grid,
                                               GetNormalizedSize(),
                                               origin,
                                               rot);
    }

    RGBColor c;
    if(pulsering_strip_cmap_on)
    {
        float p01v = ApplyVoxelDriveToPalette01(strip_p01, x, y, z, time, grid);
        c = ResolveStripKernelFinalColor(*this, pulsering_strip_cmap_kernel, p01v, pulsering_strip_cmap_color_style, time,
                                          GetScaledFrequency() * 12.0f);
    }
    else if(GetRainbowMode())
    {
        float hue = fmodf(
            pos_norm * 360.0f
            + 0.22f * azimuth_deg
            + progress * (style == STYLE_RADIAL_RAINBOW ? 36.0f : 84.0f)
            + (float)style * 52.0f
            + direction_deg * (style == STYLE_PULSE_RING ? 0.85f : 0.30f)
            + (style == STYLE_PULSE_RING ? intensity * 55.0f : 0.0f),
            360.0f);
        if(hue < 0.0f) hue += 360.0f;
        hue = ApplySpatialRainbowHue(hue, pos_norm, basis, sp, map, time, &grid);
        float p01 = std::fmod(hue / 360.0f, 1.0f);
        if(p01 < 0.0f)
        {
            p01 += 1.0f;
        }
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        c = GetRainbowColor(p01 * 360.0f);
    }
    else
    {
        float p = ApplySpatialPalette01(pos_norm, basis, sp, map, time, &grid);
        p = ApplyVoxelDriveToPalette01(p, x, y, z, time, grid);
        c = GetColorAtPosition(p);
    }
    int r_ = std::min(255, std::max(0, (int)((c & 0xFF) * intensity)));
    int g_ = std::min(255, std::max(0, (int)(((c >> 8) & 0xFF) * intensity)));
    int b_ = std::min(255, std::max(0, (int)(((c >> 16) & 0xFF) * intensity)));
    return (RGBColor)((b_ << 16) | (g_ << 8) | r_);
}

nlohmann::json PulseRing::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["ring_style"] = ring_style;
    j["ring_thickness"] = ring_thickness;
    j["hole_size"] = hole_size;
    j["pulse_amplitude"] = pulse_amplitude;
    j["direction_deg"] = direction_deg;
    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j,
                                           "pulsering_stratum_layout_mode",
                                           sm,
                                           st,
                                           "pulsering_stratum_band_speed_pct",
                                           "pulsering_stratum_band_tight_pct",
                                           "pulsering_stratum_band_phase_deg");
    StripColormapSaveJson(j, "pulsering", pulsering_strip_cmap_on, pulsering_strip_cmap_kernel, pulsering_strip_cmap_rep,
                          pulsering_strip_cmap_unfold, pulsering_strip_cmap_dir,
                          pulsering_strip_cmap_color_style);
    return j;
}

void PulseRing::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "pulsering_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "pulsering_stratum_band_speed_pct",
                                            "pulsering_stratum_band_tight_pct",
                                            "pulsering_stratum_band_phase_deg");
    StripColormapLoadJson(settings, "pulsering", pulsering_strip_cmap_on, pulsering_strip_cmap_kernel, pulsering_strip_cmap_rep,
                          pulsering_strip_cmap_unfold, pulsering_strip_cmap_dir,
                          pulsering_strip_cmap_color_style,
                          GetRainbowMode());
    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(pulsering_strip_cmap_on,
                                                pulsering_strip_cmap_kernel,
                                                pulsering_strip_cmap_rep,
                                                pulsering_strip_cmap_unfold,
                                                pulsering_strip_cmap_dir,
                                                pulsering_strip_cmap_color_style);
    }
    if(settings.contains("ring_style") && settings["ring_style"].is_number_integer())
        ring_style = std::max(0, std::min(settings["ring_style"].get<int>(), STYLE_COUNT - 1));
    if(settings.contains("ring_thickness") && settings["ring_thickness"].is_number())
    { float v = settings["ring_thickness"].get<float>(); ring_thickness = std::max(0.02f, std::min(1.0f, v)); }
    if(settings.contains("hole_size") && settings["hole_size"].is_number())
    { float v = settings["hole_size"].get<float>(); hole_size = std::max(0.0f, std::min(0.8f, v)); }
    if(settings.contains("pulse_amplitude") && settings["pulse_amplitude"].is_number())
    { float v = settings["pulse_amplitude"].get<float>(); pulse_amplitude = std::max(0.2f, std::min(2.0f, v)); }
    if(settings.contains("direction_deg") && settings["direction_deg"].is_number())
    { float v = settings["direction_deg"].get<float>(); direction_deg = fmodf(v + 360.0f, 360.0f); }
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}
