// SPDX-License-Identifier: GPL-2.0-only

#include "Bubbles.h"
#include "EffectHelpers.h"
#include "SpatialKernelColormap.h"
#include "StripKernelColormapPanel.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"
#include <algorithm>
#include <cmath>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

REGISTER_EFFECT_3D(Bubbles);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static float hash_f(unsigned int seed, unsigned int salt)
{
    unsigned int v = seed * 73856093u ^ salt * 19349663u;
    v = (v << 13u) ^ v;
    v = v * (v * v * 15731u + 789221u) + 1376312589u;
    return ((v & 0xFFFFu) / 65535.0f) * 2.0f - 1.0f;
}

Bubbles::Bubbles(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(true);
    SetFrequency(50);
}

EffectInfo3D Bubbles::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 3;
    info.effect_name = "Bubbles";
    info.effect_description =
        "Rising expanding spheres (like OpenRGB Bubbles); optional floor/mid/ceiling band tuning for motion and shell detail";
    info.category = "Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 12.0f;
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

void Bubbles::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* outer = new QVBoxLayout(w);
    outer->setContentsMargins(0, 0, 0, 0);
    QGridLayout* layout = new QGridLayout();
    outer->addLayout(layout);
    int row = 0;
    layout->addWidget(new QLabel("Max bubbles:"), row, 0);
    QSlider* max_slider = new QSlider(Qt::Horizontal);
    max_slider->setRange(4, 100);
    max_slider->setToolTip("How many bubble centers are simulated (internally capped for performance).");
    max_slider->setValue(max_bubbles);
    QLabel* max_label = new QLabel(QString::number(max_bubbles));
    max_label->setMinimumWidth(36);
    layout->addWidget(max_slider, row, 1);
    layout->addWidget(max_label, row, 2);
    connect(max_slider, &QSlider::valueChanged, this, [this, max_label](int v){
        max_bubbles = v;
        if(max_label) max_label->setText(QString::number(v));
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Ring thickness:"), row, 0);
    QSlider* thick_slider = new QSlider(Qt::Horizontal);
    thick_slider->setRange(2, 100);
    thick_slider->setToolTip("Shell thickness of each bubble as a fraction of room scale.");
    thick_slider->setValue((int)(bubble_thickness * 100.0f));
    QLabel* thick_label = new QLabel(QString::number((int)(bubble_thickness * 100)) + "%");
    thick_label->setMinimumWidth(36);
    layout->addWidget(thick_slider, row, 1);
    layout->addWidget(thick_label, row, 2);
    connect(thick_slider, &QSlider::valueChanged, this, [this, thick_label](int v){
        bubble_thickness = v / 100.0f;
        if(thick_label) thick_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Rise speed:"), row, 0);
    QSlider* rise_slider = new QSlider(Qt::Horizontal);
    rise_slider->setRange(20, 200);
    rise_slider->setToolTip("How fast bubbles drift upward through the volume.");
    rise_slider->setValue((int)(rise_speed * 100.0f));
    QLabel* rise_label = new QLabel(QString::number(rise_speed, 'f', 2));
    rise_label->setMinimumWidth(36);
    layout->addWidget(rise_slider, row, 1);
    layout->addWidget(rise_label, row, 2);
    connect(rise_slider, &QSlider::valueChanged, this, [this, rise_label](int v){
        rise_speed = v / 100.0f;
        if(rise_label) rise_label->setText(QString::number(rise_speed, 'f', 2));
        emit ParametersChanged();
    });
    row++;
    layout->addWidget(new QLabel("Spawn rate:"), row, 0);
    QSlider* spawn_slider = new QSlider(Qt::Horizontal);
    spawn_slider->setRange(30, 200);
    spawn_slider->setToolTip("Spacing between bubble phases (lower = busier, more motion).");
    spawn_slider->setValue((int)(spawn_interval * 100.0f));
    QLabel* spawn_label = new QLabel(QString::number(spawn_interval, 'f', 2));
    spawn_label->setMinimumWidth(36);
    layout->addWidget(spawn_slider, row, 1);
    layout->addWidget(spawn_label, row, 2);
    connect(spawn_slider, &QSlider::valueChanged, this, [this, spawn_label](int v){
        spawn_interval = v / 100.0f;
        if(spawn_label) spawn_label->setText(QString::number(spawn_interval, 'f', 2));
        emit ParametersChanged();
    });
    strip_cmap_panel = new StripKernelColormapPanel(w);
    strip_cmap_panel->mirrorStateFromEffect(bubbles_strip_cmap_on,
                                            bubbles_strip_cmap_kernel,
                                            bubbles_strip_cmap_rep,
                                            bubbles_strip_cmap_unfold,
                                            bubbles_strip_cmap_dir,
                                            bubbles_strip_cmap_color_style);
    outer->addWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &Bubbles::SyncStripColormapFromPanel);
    stratum_panel = new StratumBandPanel(w);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    outer->addWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &Bubbles::OnStratumBandChanged);
    OnStratumBandChanged();
    AddWidgetToParent(w, parent);
}

void Bubbles::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    bubbles_strip_cmap_on = strip_cmap_panel->useStripColormap();
    bubbles_strip_cmap_kernel = strip_cmap_panel->kernelId();
    bubbles_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    bubbles_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    bubbles_strip_cmap_dir = strip_cmap_panel->directionDeg();
    bubbles_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
}

void Bubbles::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
}

void Bubbles::UpdateParams(SpatialEffectParams& params) { (void)params; }


RGBColor Bubbles::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

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
    const bool strat_on = (stratum_layout_mode == 1);

    // Use unscaled room half-extents so bubble centers and radii stay inside the real grid volume.
    // (Scaled half-extents from GetNormalizedScale() inflate coordinates and push bubbles off-layout.)
    EffectGridAxisHalfExtents e_room = MakeEffectGridAxisHalfExtents(grid, 1.0f);
    float h_scale = std::max({e_room.hw, e_room.hh, e_room.hd});
    float speed_scale = GetScaledSpeed() * 0.015f;
    float size_m = GetNormalizedSize();
    float detail = std::max(0.05f, GetScaledDetail());
    float color_cycle = time * GetScaledFrequency() * 12.0f;
    if(strat_on)
    {
        color_cycle = color_cycle * bb.speed_mul + bb.phase_deg;
    }
    int n_bub = std::max(4, std::min(30, max_bubbles));
    float thick = std::max(0.02f, std::min(4.0f, bubble_thickness * h_scale)) / std::max(0.35f, detail);
    if(strat_on)
    {
        thick /= std::max(0.25f, bb.tight_mul);
    }
    float rise = std::max(0.2f, std::min(2.0f, rise_speed)) * speed_scale * e_room.hh;
    float interval = std::max(0.3f, std::min(2.0f, spawn_interval));
    const float cycle_t = interval * (float)n_bub;
    float max_r = std::max(0.5f, std::min(2.0f, max_radius)) * h_scale * size_m * 0.35f;

    if(!strat_on)
    {
        if(bubble_centers_cached.size() != (size_t)n_bub || fabsf(time - bubble_cache_time) > 0.001f)
        {
            bubble_cache_time = time;
            bubble_centers_cached.resize(n_bub);
            for(int i = 0; i < n_bub; i++)
            {
                float phase = fmodf(time + (float)i * interval, cycle_t);
                float radius = (phase / cycle_t) * max_r * 0.4f;
                float cx = origin.x + hash_f((unsigned int)(i * 1000), 1u) * e_room.hw * 0.5f;
                float cy = origin.y - e_room.hh +
                           fmodf(time * rise * 0.5f + (float)i * 0.3f, e_room.hh * 2.0f);
                float cz = origin.z + hash_f((unsigned int)(i * 1000), 2u) * e_room.hd * 0.5f;
                bubble_centers_cached[i] = {cx, cy, cz, radius};
            }
        }
    }

    float max_intensity = 0.0f;
    float best_hue = 0.0f;

    for(int i = 0; i < n_bub; i++)
    {
        BubbleCenter3D b;
        if(strat_on)
        {
            float phase = fmodf(time * bb.speed_mul + (float)i * interval, cycle_t);
            b.radius = (phase / cycle_t) * max_r * 0.4f;
            b.cx = origin.x + hash_f((unsigned int)(i * 1000), 1u) * e_room.hw * 0.5f;
            b.cy = origin.y - e_room.hh +
                   fmodf(time * rise * 0.5f * bb.speed_mul + (float)i * 0.3f, e_room.hh * 2.0f);
            b.cz = origin.z + hash_f((unsigned int)(i * 1000), 2u) * e_room.hd * 0.5f;
        }
        else
        {
            b = bubble_centers_cached[i];
        }
        float dx = x - b.cx;
        float dy = y - b.cy;
        float dz = z - b.cz;
        float dist_sq = dx*dx + dy*dy + dz*dz;
        float far = b.radius + thick * 4.0f;
        if(dist_sq > far * far) continue;
        float dist = sqrtf(dist_sq);
        float shallow = fabsf(dist - b.radius) / thick;
        float value = (shallow < 0.01f) ? 1.0f : 1.0f / (1.0f + shallow * shallow);
        value = fmaxf(0.0f, fminf(1.0f, value));

        if(value > max_intensity)
        {
            max_intensity = value;
            best_hue = fmodf((float)i * 40.0f + color_cycle, 360.0f);
            if(best_hue < 0.0f) best_hue += 360.0f;
        }
    }

    RGBColor final_color;
    if(bubbles_strip_cmap_on)
    {
        const float ph01 = std::fmod(color_cycle * (1.f / 360.f) + best_hue * (1.f / 360.f) + 1.f, 1.f);
        float pal01 = SampleStripKernelPalette01(bubbles_strip_cmap_kernel,
                                                 bubbles_strip_cmap_rep,
                                                 bubbles_strip_cmap_unfold,
                                                 bubbles_strip_cmap_dir,
                                                 ph01,
                                                 time,
                                                 grid,
                                                 size_m,
                                                 origin,
                                                 rp);
        pal01 = ApplyVoxelDriveToPalette01(pal01, x, y, z, time, grid);
        final_color = ResolveStripKernelFinalColor(*this,
                                                   bubbles_strip_cmap_kernel,
                                                   std::clamp(pal01, 0.0f, 1.0f),
                                                   bubbles_strip_cmap_color_style,
                                                   time,
                                                   GetScaledFrequency() * 12.0f * (strat_on ? bb.speed_mul : 1.0f));
    }
    else
    {
        final_color = GetRainbowMode() ? GetRainbowColor(best_hue) : GetColorAtPosition(0.5f);
    }
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    r = (unsigned char)(r * max_intensity);
    g = (unsigned char)(g * max_intensity);
    b = (unsigned char)(b * max_intensity);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json Bubbles::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j,
                                           "bubbles_stratum_layout_mode",
                                           sm,
                                           st,
                                           "bubbles_stratum_band_speed_pct",
                                           "bubbles_stratum_band_tight_pct",
                                           "bubbles_stratum_band_phase_deg");
    j["max_bubbles"] = max_bubbles;
    j["bubble_thickness"] = bubble_thickness;
    j["rise_speed"] = rise_speed;
    j["spawn_interval"] = spawn_interval;
    j["max_radius"] = max_radius;
    StripColormapSaveJson(j,
                          "bubbles",
                          bubbles_strip_cmap_on,
                          bubbles_strip_cmap_kernel,
                          bubbles_strip_cmap_rep,
                          bubbles_strip_cmap_unfold,
                          bubbles_strip_cmap_dir,
                          bubbles_strip_cmap_color_style);
    return j;
}

void Bubbles::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "bubbles_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "bubbles_stratum_band_speed_pct",
                                            "bubbles_stratum_band_tight_pct",
                                            "bubbles_stratum_band_phase_deg");
    if(settings.contains("max_bubbles") && settings["max_bubbles"].is_number_integer())
        max_bubbles = std::max(4, std::min(30, settings["max_bubbles"].get<int>()));
    if(settings.contains("bubble_thickness") && settings["bubble_thickness"].is_number())
        bubble_thickness = std::max(0.02f, std::min(1.0f, settings["bubble_thickness"].get<float>()));
    if(settings.contains("rise_speed") && settings["rise_speed"].is_number())
        rise_speed = std::max(0.2f, std::min(2.0f, settings["rise_speed"].get<float>()));
    if(settings.contains("spawn_interval") && settings["spawn_interval"].is_number())
        spawn_interval = std::max(0.3f, std::min(2.0f, settings["spawn_interval"].get<float>()));
    if(settings.contains("max_radius") && settings["max_radius"].is_number())
        max_radius = std::max(0.5f, std::min(2.0f, settings["max_radius"].get<float>()));
    StripColormapLoadJson(settings,
                          "bubbles",
                          bubbles_strip_cmap_on,
                          bubbles_strip_cmap_kernel,
                          bubbles_strip_cmap_rep,
                          bubbles_strip_cmap_unfold,
                          bubbles_strip_cmap_dir,
                          bubbles_strip_cmap_color_style,
                          GetRainbowMode());
    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(bubbles_strip_cmap_on,
                                                bubbles_strip_cmap_kernel,
                                                bubbles_strip_cmap_rep,
                                                bubbles_strip_cmap_unfold,
                                                bubbles_strip_cmap_dir,
                                                bubbles_strip_cmap_color_style);
    }
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}
