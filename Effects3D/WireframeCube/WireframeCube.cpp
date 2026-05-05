// SPDX-License-Identifier: GPL-2.0-only

#include "WireframeCube.h"
#include "SpatialKernelColormap.h"
#include "StripShellPattern/StripShellPatternKernels.h"
#include "StripKernelColormapPanel.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"
#include "EffectHelpers.h"
#include <algorithm>
#include <cmath>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

REGISTER_EFFECT_3D(WireframeCube);

float WireframeCube::PointToSegmentDistance(float px, float py, float pz,
                                            float ax, float ay, float az,
                                            float bx, float by, float bz,
                                            float* out_t01)
{
    float dx = bx - ax, dy = by - ay, dz = bz - az;
    float len2 = dx * dx + dy * dy + dz * dz;
    if(len2 < 1e-10f)
    {
        if(out_t01)
        {
            *out_t01 = 0.0f;
        }
        return sqrtf((px - ax) * (px - ax) + (py - ay) * (py - ay) + (pz - az) * (pz - az));
    }
    float t = ((px - ax) * dx + (py - ay) * dy + (pz - az) * dz) / len2;
    t = fmaxf(0.0f, fminf(1.0f, t));
    if(out_t01)
    {
        *out_t01 = t;
    }
    float qx = ax + t * dx, qy = ay + t * dy, qz = az + t * dz;
    return sqrtf((px - qx) * (px - qx) + (py - qy) * (py - qy) + (pz - qz) * (pz - qz));
}

void WireframeCube::RebuildRoomWireframeCache(const GridContext3D& grid)
{
    if(room_wf_cache_seq == grid.render_sequence
       && room_wf_min_x == grid.min_x && room_wf_max_x == grid.max_x
       && room_wf_min_y == grid.min_y && room_wf_max_y == grid.max_y
       && room_wf_min_z == grid.min_z && room_wf_max_z == grid.max_z)
    {
        return;
    }

    room_wf_cache_seq = grid.render_sequence;
    room_wf_min_x = grid.min_x;
    room_wf_max_x = grid.max_x;
    room_wf_min_y = grid.min_y;
    room_wf_max_y = grid.max_y;
    room_wf_min_z = grid.min_z;
    room_wf_max_z = grid.max_z;

    const float mx = grid.min_x;
    const float Mx = grid.max_x;
    const float my = grid.min_y;
    const float My = grid.max_y;
    const float mz = grid.min_z;
    const float Mz = grid.max_z;

    // Bottom face (y = my), then top face (y = My), then four verticals — matches LEDViewport3D room box.
    const float c000[3] = {mx, my, mz};
    const float c100[3] = {Mx, my, mz};
    const float c110[3] = {Mx, my, Mz};
    const float c010[3] = {mx, my, Mz};
    const float c001[3] = {mx, My, mz};
    const float c101[3] = {Mx, My, mz};
    const float c111[3] = {Mx, My, Mz};
    const float c011[3] = {mx, My, Mz};

    const float* edge_corners[12][2] = {
        {c000, c100}, {c100, c110}, {c110, c010}, {c010, c000},
        {c001, c101}, {c101, c111}, {c111, c011}, {c011, c001},
        {c000, c001}, {c100, c101}, {c110, c111}, {c010, c011},
    };

    room_wf_prefix[0] = 0.0f;
    float total = 0.0f;
    for(int e = 0; e < 12; e++)
    {
        const float* a = edge_corners[e][0];
        const float* b = edge_corners[e][1];
        room_wf_ax[e] = a[0];
        room_wf_ay[e] = a[1];
        room_wf_az[e] = a[2];
        room_wf_bx[e] = b[0];
        room_wf_by[e] = b[1];
        room_wf_bz[e] = b[2];
        float dx = b[0] - a[0], dy = b[1] - a[1], dz = b[2] - a[2];
        float L = sqrtf(dx * dx + dy * dy + dz * dz);
        if(L < 1e-6f)
        {
            L = 1e-6f;
        }
        room_wf_edge_len[e] = L;
        total += L;
        room_wf_prefix[e + 1] = total;
    }
    room_wf_total_len = (total > 1e-8f) ? total : 1.0f;
}

void WireframeCube::ClosestOnRoomWireframe(float x, float y, float z,
                                           float& out_dist, float& out_path01) const
{
    float best_d2 = 1e30f;
    float best_arc = 0.0f;
    const float inv_tot = 1.0f / room_wf_total_len;

    for(int e = 0; e < 12; e++)
    {
        float t01 = 0.0f;
        float d = PointToSegmentDistance(x, y, z,
                                         room_wf_ax[e], room_wf_ay[e], room_wf_az[e],
                                         room_wf_bx[e], room_wf_by[e], room_wf_bz[e],
                                         &t01);
        float d2 = d * d;
        float arc = room_wf_prefix[e] + t01 * room_wf_edge_len[e];
        if(d2 < best_d2 || (d2 <= best_d2 + 1e-12f && arc < best_arc))
        {
            best_d2 = d2;
            best_arc = arc;
        }
    }
    out_dist = sqrtf(best_d2);
    out_path01 = std::clamp(best_arc * inv_tot, 0.0f, 1.0f);
}

WireframeCube::WireframeCube(QWidget* parent) : SpatialEffect3D(parent) {}

EffectInfo3D WireframeCube::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Wire Frame";
    info.effect_description = "Soft glow on the room’s axis-aligned outline (floor, ceiling, corners). "
                             "Strip kernels run as a single 1D path around the perimeter — "
                             "suited to LEDs along skirting, ceiling edges, and vertical corners.";
    info.category = "Spatial";
    info.effect_type = (SpatialEffectType)0;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 200;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.default_speed_scale = 40.0f;
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

void WireframeCube::SetupCustomUI(QWidget* parent)
{
    QWidget* outer = new QWidget();
    QVBoxLayout* vbox = new QVBoxLayout(outer);
    vbox->setContentsMargins(0, 0, 0, 0);
    QWidget* w = new QWidget();
    vbox->addWidget(w);
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel("Edge glow:"), 0, 0);
    QSlider* thick_slider = new QSlider(Qt::Horizontal);
    thick_slider->setRange(2, 100);
    thick_slider->setToolTip("How wide the lit region around each edge (halo thickness).");
    thick_slider->setValue((int)(thickness * 100.0f));
    QLabel* thick_label = new QLabel(QString::number((int)(thickness * 100)) + "%");
    thick_label->setMinimumWidth(36);
    layout->addWidget(thick_slider, 0, 1);
    layout->addWidget(thick_label, 0, 2);
    connect(thick_slider, &QSlider::valueChanged, this, [this, thick_label](int v){
        thickness = v / 100.0f;
        if(thick_label) thick_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });
    layout->addWidget(new QLabel("Line brightness:"), 1, 0);
    QSlider* bright_slider = new QSlider(Qt::Horizontal);
    bright_slider->setRange(0, 100);
    bright_slider->setToolTip("Peak brightness along the wireframe lines.");
    bright_slider->setValue((int)(line_brightness * 100.0f));
    QLabel* bright_label = new QLabel(QString::number((int)(line_brightness * 100)) + "%");
    bright_label->setMinimumWidth(36);
    layout->addWidget(bright_slider, 1, 1);
    layout->addWidget(bright_label, 1, 2);
    connect(bright_slider, &QSlider::valueChanged, this, [this, bright_label](int v){
        line_brightness = v / 100.0f;
        if(bright_label) bright_label->setText(QString::number(v) + "%");
        emit ParametersChanged();
    });

    auto* hint = new QLabel(QStringLiteral(
        "Patterns: strip kernels use one continuous path (floor loop, then ceiling loop, then vertical corners). "
        "Direction and repeats apply to that 1D run."));
    hint->setWordWrap(true);
    vbox->addWidget(hint);

    strip_cmap_panel = new StripKernelColormapPanel(outer);
    strip_cmap_panel->mirrorStateFromEffect(wireframecube_strip_cmap_on,
                                            wireframecube_strip_cmap_kernel,
                                            wireframecube_strip_cmap_rep,
                                            wireframecube_strip_cmap_unfold,
                                            wireframecube_strip_cmap_dir,
                                            wireframecube_strip_cmap_color_style);
    vbox->addWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &WireframeCube::SyncStripColormapFromPanel);

    stratum_panel = new StratumBandPanel(outer);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    vbox->addWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &WireframeCube::OnStratumBandChanged);
    OnStratumBandChanged();
    AddWidgetToParent(outer, parent);
}

void WireframeCube::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    wireframecube_strip_cmap_on = strip_cmap_panel->useStripColormap();
    wireframecube_strip_cmap_kernel = strip_cmap_panel->kernelId();
    wireframecube_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    wireframecube_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    wireframecube_strip_cmap_dir = strip_cmap_panel->directionDeg();
    wireframecube_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
}

void WireframeCube::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
}

void WireframeCube::UpdateParams(SpatialEffectParams& params)
{
    (void)params;
}

RGBColor WireframeCube::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    RebuildRoomWireframeCache(grid);
    if(room_wf_total_len < 1e-7f)
        return 0x00000000;

    float dist_edge = 0.0f;
    float path01 = 0.0f;
    ClosestOnRoomWireframe(x, y, z, dist_edge, path01);

    float coord2 = NormalizeGridAxis01(y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(stratum_layout_mode, sw, stratum_tuning_);
    const float tm = std::max(0.25f, bb.tight_mul);

    float sigma = std::max(thickness, 0.02f);
    float sigma_sq = sigma * sigma / (tm * tm);
    const float d2_cutoff = 9.0f * sigma_sq;
    float d = dist_edge;
    float d2 = d * d;
    float total = 0.0f;
    if(d2 <= d2_cutoff)
    {
        total = expf(-d2 / sigma_sq);
    }
    total = fminf(1.0f, total * 0.35f);
    total *= std::max(0.0f, std::min(1.0f, line_brightness));

    float detail = std::max(0.05f, GetScaledDetail());
    const float speed_phase = GetScaledFrequency() * 12.0f * bb.speed_mul;

    float hue = std::fmod(path01 * 360.0f * (0.6f + 0.4f * detail) + time * speed_phase + bb.phase_deg,
                          360.0f);
    if(hue < 0.0f)
        hue += 360.0f;

    float pal01 = 0.5f;
    RGBColor c;
    if(wireframecube_strip_cmap_on)
    {
        const float dir01 = std::fmod(wireframecube_strip_cmap_dir * (1.f / 360.f), 1.f);
        float s01_kernel = std::fmod(path01 + dir01 + 1.f, 1.f);

        const float phase01 =
            std::fmod(CalculateProgress(time) * 0.2f + time * speed_phase * (1.f / 360.f) +
                          bb.phase_deg * (1.f / 360.f) + 1.f,
                      1.f);

        const int kid = StripShellKernelClamp(wireframecube_strip_cmap_kernel);
        const float k = EvalStripShellKernel(kid,
                                             s01_kernel,
                                             phase01,
                                             wireframecube_strip_cmap_rep,
                                             time);
        pal01 = std::clamp((k + 1.0f) * 0.5f, 0.0f, 1.0f);

        pal01 = ApplyVoxelDriveToPalette01(pal01, x, y, z, time, grid);
        c = ResolveStripKernelFinalColor(*this,
                                         kid,
                                         pal01,
                                         wireframecube_strip_cmap_color_style,
                                         time,
                                         speed_phase);
    }
    else
    {
        c = GetRainbowMode() ? GetRainbowColor(hue) : GetColorAtPosition(pal01);
    }
    int r = (int)((c & 0xFF) * total);
    int g = (int)(((c >> 8) & 0xFF) * total);
    int b = (int)(((c >> 16) & 0xFF) * total);
    r = std::min(255, std::max(0, r));
    g = std::min(255, std::max(0, g));
    b = std::min(255, std::max(0, b));
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json WireframeCube::SaveSettings() const
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
                                           "wireframe_cube_stratum_layout_mode",
                                           sm,
                                           st,
                                           "wireframe_cube_stratum_band_speed_pct",
                                           "wireframe_cube_stratum_band_tight_pct",
                                           "wireframe_cube_stratum_band_phase_deg");
    j["thickness"] = thickness;
    j["line_brightness"] = line_brightness;
    StripColormapSaveJson(j,
                          "wireframecube",
                          wireframecube_strip_cmap_on,
                          wireframecube_strip_cmap_kernel,
                          wireframecube_strip_cmap_rep,
                          wireframecube_strip_cmap_unfold,
                          wireframecube_strip_cmap_dir,
                          wireframecube_strip_cmap_color_style);
    return j;
}

void WireframeCube::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "wireframe_cube_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "wireframe_cube_stratum_band_speed_pct",
                                            "wireframe_cube_stratum_band_tight_pct",
                                            "wireframe_cube_stratum_band_phase_deg");
    if(settings.contains("thickness") && settings["thickness"].is_number())
    {
        float v = settings["thickness"].get<float>();
        thickness = std::max(0.02f, std::min(1.0f, v));
    }
    if(settings.contains("line_brightness") && settings["line_brightness"].is_number())
    {
        float v = settings["line_brightness"].get<float>();
        line_brightness = std::max(0.0f, std::min(1.0f, v));
    }
    StripColormapLoadJson(settings,
                          "wireframecube",
                          wireframecube_strip_cmap_on,
                          wireframecube_strip_cmap_kernel,
                          wireframecube_strip_cmap_rep,
                          wireframecube_strip_cmap_unfold,
                          wireframecube_strip_cmap_dir,
                          wireframecube_strip_cmap_color_style,
                          GetRainbowMode());
    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(wireframecube_strip_cmap_on,
                                                wireframecube_strip_cmap_kernel,
                                                wireframecube_strip_cmap_rep,
                                                wireframecube_strip_cmap_unfold,
                                                wireframecube_strip_cmap_dir,
                                                wireframecube_strip_cmap_color_style);
    }
    room_wf_cache_seq = 0;
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}
