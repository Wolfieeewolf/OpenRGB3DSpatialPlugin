// SPDX-License-Identifier: GPL-2.0-only

#include "Tornado.h"
#include "SpatialKernelColormap.h"
#include "StripKernelColormapPanel.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

REGISTER_EFFECT_3D(Tornado);

Tornado::Tornado(QWidget* parent) : SpatialEffect3D(parent)
{
    core_radius_slider = nullptr;
    core_radius_label = nullptr;
    height_slider = nullptr;
    height_label = nullptr;
    core_radius = 80;
    tornado_height = 250;
    SetRainbowMode(true);
    SetFrequency(50);
}

Tornado::~Tornado() = default;

EffectInfo3D Tornado::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 3;
    info.effect_name = "Tornado";
    info.effect_description = "Spinning vortex; optional floor/mid/ceiling band tuning (speed, tightness, phase)";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_TORNADO;
    info.is_reversible = true;
    info.supports_random = true;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;

    info.default_speed_scale = 25.0f;
    info.default_frequency_scale = 6.0f;
    info.use_size_parameter = true;

    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = true;

    return info;
}

void Tornado::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* outer = new QVBoxLayout(w);
    outer->setContentsMargins(0,0,0,0);
    QGridLayout* layout = new QGridLayout();
    outer->addLayout(layout);

    layout->addWidget(new QLabel("Core Radius:"), 0, 0);
    core_radius_slider = new QSlider(Qt::Horizontal);
    core_radius_slider->setRange(20, 300);
    core_radius_slider->setValue(core_radius);
    core_radius_slider->setToolTip("Tornado core radius (affects base funnel size)");
    layout->addWidget(core_radius_slider, 0, 1);
    core_radius_label = new QLabel(QString::number(core_radius));
    core_radius_label->setMinimumWidth(30);
    layout->addWidget(core_radius_label, 0, 2);

    layout->addWidget(new QLabel("Height:"), 1, 0);
    height_slider = new QSlider(Qt::Horizontal);
    height_slider->setRange(50, 500);
    height_slider->setValue(tornado_height);
    height_slider->setToolTip("Tornado height (relative to room height)");
    layout->addWidget(height_slider, 1, 1);
    height_label = new QLabel(QString::number(tornado_height));
    height_label->setMinimumWidth(30);
    layout->addWidget(height_label, 1, 2);

    connect(core_radius_slider, &QSlider::valueChanged, this, &Tornado::OnTornadoParameterChanged);
    connect(core_radius_slider, &QSlider::valueChanged, core_radius_label, [this](int value) {
        core_radius_label->setText(QString::number(value));
    });
    connect(height_slider, &QSlider::valueChanged, this, &Tornado::OnTornadoParameterChanged);
    connect(height_slider, &QSlider::valueChanged, height_label, [this](int value) {
        height_label->setText(QString::number(value));
    });

    strip_cmap_panel = new StripKernelColormapPanel(w);
    strip_cmap_panel->mirrorStateFromEffect(tornado_strip_cmap_on,
                                            tornado_strip_cmap_kernel,
                                            tornado_strip_cmap_rep,
                                            tornado_strip_cmap_unfold,
                                            tornado_strip_cmap_dir,
                                            tornado_strip_cmap_color_style);
    AddColorPatternWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &Tornado::SyncStripColormapFromPanel);

    stratum_panel = new StratumBandPanel(w);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    AddBandModulationWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &Tornado::OnStratumBandChanged);
    OnStratumBandChanged();

    AddWidgetToParent(w, parent);
}

void Tornado::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_TORNADO;
}

void Tornado::OnTornadoParameterChanged()
{
    if(core_radius_slider)
    {
        core_radius = core_radius_slider->value();
        if(core_radius_label) core_radius_label->setText(QString::number(core_radius));
    }
    if(height_slider)
    {
        tornado_height = height_slider->value();
        if(height_label) height_label->setText(QString::number(tornado_height));
    }
    emit ParametersChanged();
}

void Tornado::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
}

void Tornado::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    tornado_strip_cmap_on = strip_cmap_panel->useStripColormap();
    tornado_strip_cmap_kernel = strip_cmap_panel->kernelId();
    tornado_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    tornado_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    tornado_strip_cmap_dir = strip_cmap_panel->directionDeg();
    tornado_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
}


RGBColor Tornado::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
    {
        return 0x00000000;
    }
    Vector3D origin = GetEffectOriginGrid(grid);

    float speed = GetScaledSpeed();
    float rate = GetScaledFrequency();
    float detail = std::max(0.05f, GetScaledDetail());
    float size_m = GetNormalizedSize();
    float progress = CalculateProgress(time);

    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float rot_rel_x = rotated_pos.x - origin.x;
    float rot_rel_y = rotated_pos.y - origin.y;
    float rot_rel_z = rotated_pos.z - origin.z;

    float axial = NormalizeGridAxis01(rotated_pos.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float swt[3];
    EffectStratumBlend::WeightsForYNorm(axial, strat_st, swt);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(stratum_layout_mode, swt, stratum_tuning_);
    float progress_e = progress * bb.speed_mul;
    float detail_e = detail * bb.tight_mul;

    float height_center = 0.5f;
    float height_range_val = (tornado_height / 500.0f) * 0.5f;
    float h_norm = fmax(0.0f, fmin(1.0f, (axial - (height_center - height_range_val)) / (2.0f * height_range_val + 0.0001f)));
    EffectGridAxisHalfExtents ex = MakeEffectGridAxisHalfExtents(grid, GetNormalizedScale());
    float base_radius = std::max(ex.hw, ex.hd);
    float core_scale = 0.04f + (core_radius / 300.0f) * 0.56f;
    constexpr float kTornadoGridFill = 2.5f;
    float funnel_radius = (base_radius * core_scale) * (0.6f + 0.4f * h_norm) * size_m * kTornadoGridFill;

    float a = atan2f(rot_rel_z, rot_rel_x);
    float rad = sqrtf(rot_rel_x*rot_rel_x + rot_rel_z*rot_rel_z);
    float along = rot_rel_y;
    float swirl = a + along * (0.015f * detail_e) - time * speed * 0.25f * bb.speed_mul +
        bb.phase_deg * (float)(M_PI / 180.0f);

    float ring = fabsf(rad - funnel_radius);
    float thickness_base = (ex.hw + ex.hd) * 0.01f;
    float ring_thickness = thickness_base * (0.6f + 1.2f * size_m);
    float ring_intensity = fmax(0.0f, 1.0f - ring / ring_thickness);

    float arms = 4.0f + 4.0f * size_m;
    float band = 0.5f * (1.0f + cosf(swirl * arms));
    float band2 = 0.2f * (1.0f + cosf(swirl * arms * 2.0f + progress_e));
    band = fmin(1.0f, band + band2);

    float y_fade = fmax(0.0f, 1.0f - fabsf(axial - 0.5f) / (height_range_val + 0.001f));
    
    float radial_glow = 0.15f * (1.0f - fmin(1.0f, rad / (funnel_radius * 3.0f)));

    float intensity = ring_intensity * (0.5f + 0.5f * band) * y_fade + radial_glow;
    intensity = fmax(0.0f, fmin(1.0f, intensity));

    SpatialLayerCore::Basis compass_basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), compass_basis);
    SpatialLayerCore::MapperSettings compass_map;
    EffectStratumBlend::InitStratumBreaks(compass_map);
    compass_map.blend_softness =
        std::clamp(0.09f + 0.04f * (1.0f - detail), 0.05f, 0.20f);
    compass_map.center_size = std::clamp(0.10f + 0.20f * size_m, 0.06f, 0.50f);
    compass_map.directional_sharpness = std::clamp(1.0f + detail * 0.1f, 0.85f, 2.3f);

    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = x;
    sp.grid_y = y;
    sp.grid_z = z;
    sp.origin_x = origin.x;
    sp.origin_y = origin.y;
    sp.origin_z = origin.z;
    sp.y_norm = axial;

    const float phase01 = std::fmod(progress_e * 0.25f + bb.phase_deg * (1.0f / 360.0f) + 1.0f, 1.0f);
    float strip_p01 = 0.0f;
    if(tornado_strip_cmap_on)
    {
        strip_p01 = SampleStripKernelPalette01(tornado_strip_cmap_kernel,
                                                 tornado_strip_cmap_rep,
                                                 tornado_strip_cmap_unfold,
                                                 tornado_strip_cmap_dir,
                                                 phase01,
                                                 time,
                                                 grid,
                                                 size_m,
                                                 origin,
                                                 rotated_pos);
    }

    RGBColor final_color;
    if(GetRainbowMode())
    {
        float hue =
            200.0f + swirl * 57.2958f * 0.2f + h_norm * 80.0f + time * rate * 12.0f * bb.speed_mul;
        if(tornado_strip_cmap_on)
            hue = strip_p01 * 360.0f + time * rate * 12.0f * bb.speed_mul;
        hue = ApplySpatialRainbowHue(hue, std::clamp(h_norm, 0.0f, 1.0f), compass_basis, sp, compass_map, time, &grid);
        float p01 = std::fmod(hue / 360.0f, 1.0f);
        if(p01 < 0.0f)
        {
            p01 += 1.0f;
        }
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        final_color = GetRainbowColor(p01 * 360.0f);
    }
    else
    {
        float pos = fmodf(0.5f + 0.5f * intensity + time * rate * 0.02f * bb.speed_mul, 1.0f);
        if(tornado_strip_cmap_on)
            pos = strip_p01;
        if(pos < 0.0f) pos += 1.0f;
        float p = ApplySpatialPalette01(pos, compass_basis, sp, compass_map, time, &grid);
        p = ApplyVoxelDriveToPalette01(p, x, y, z, time, grid);
        final_color = GetColorAtPosition(p);
    }

    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    r = (unsigned char)(r * intensity);
    g = (unsigned char)(g * intensity);
    b = (unsigned char)(b * intensity);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json Tornado::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["core_radius"] = core_radius;
    j["tornado_height"] = tornado_height;
    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j,
                                           "tornado_stratum_layout_mode",
                                           sm,
                                           st,
                                           "tornado_stratum_band_speed_pct",
                                           "tornado_stratum_band_tight_pct",
                                           "tornado_stratum_band_phase_deg");
    j["tornado_strip_cmap_on"] = tornado_strip_cmap_on;
    j["tornado_strip_cmap_kernel"] = tornado_strip_cmap_kernel;
    j["tornado_strip_cmap_rep"] = tornado_strip_cmap_rep;
    j["tornado_strip_cmap_unfold"] = tornado_strip_cmap_unfold;
    j["tornado_strip_cmap_dir"] = tornado_strip_cmap_dir;
    return j;
}

void Tornado::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "tornado_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "tornado_stratum_band_speed_pct",
                                            "tornado_stratum_band_tight_pct",
                                            "tornado_stratum_band_phase_deg");
    if(settings.contains("core_radius")) core_radius = settings["core_radius"];
    if(settings.contains("tornado_height")) tornado_height = settings["tornado_height"];
    if(settings.contains("tornado_strip_cmap_on") && settings["tornado_strip_cmap_on"].is_boolean())
        tornado_strip_cmap_on = settings["tornado_strip_cmap_on"].get<bool>();
    else if(settings.contains("tornado_strip_cmap_on") && settings["tornado_strip_cmap_on"].is_number_integer())
        tornado_strip_cmap_on = settings["tornado_strip_cmap_on"].get<int>() != 0;
    if(settings.contains("tornado_strip_cmap_kernel") && settings["tornado_strip_cmap_kernel"].is_number_integer())
        tornado_strip_cmap_kernel = std::clamp(settings["tornado_strip_cmap_kernel"].get<int>(), 0, StripShellKernelCount() - 1);
    if(settings.contains("tornado_strip_cmap_rep") && settings["tornado_strip_cmap_rep"].is_number())
        tornado_strip_cmap_rep = std::max(1.0f, std::min(40.0f, settings["tornado_strip_cmap_rep"].get<float>()));
    if(settings.contains("tornado_strip_cmap_unfold") && settings["tornado_strip_cmap_unfold"].is_number_integer())
        tornado_strip_cmap_unfold = std::clamp(settings["tornado_strip_cmap_unfold"].get<int>(), 0,
                                           (int)StripPatternSurface::UnfoldMode::COUNT - 1);
    if(settings.contains("tornado_strip_cmap_dir") && settings["tornado_strip_cmap_dir"].is_number())
        tornado_strip_cmap_dir = std::fmod(settings["tornado_strip_cmap_dir"].get<float>() + 360.0f, 360.0f);

    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(tornado_strip_cmap_on,
                                                tornado_strip_cmap_kernel,
                                                tornado_strip_cmap_rep,
                                                tornado_strip_cmap_unfold,
                                                tornado_strip_cmap_dir,
                                                tornado_strip_cmap_color_style);
    }
    if(core_radius_slider) core_radius_slider->setValue(core_radius);
    if(height_slider) height_slider->setValue(tornado_height);
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}
