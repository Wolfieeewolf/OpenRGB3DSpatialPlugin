// SPDX-License-Identifier: GPL-2.0-only

#include "WireframeCube.h"
#include "SpatialKernelColormap.h"
#include "StripKernelColormapPanel.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"
#include "EffectHelpers.h"
#include <cmath>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

REGISTER_EFFECT_3D(WireframeCube);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void rotate_axis_angle(float& x, float& y, float& z, float ax, float ay, float az, float angle_rad)
{
    float c = cosf(angle_rad);
    float s = sinf(angle_rad);
    float dot = ax*x + ay*y + az*z;
    float nx = x*c + (ay*z - az*y)*s + ax*dot*(1.0f - c);
    float ny = y*c + (az*x - ax*z)*s + ay*dot*(1.0f - c);
    float nz = z*c + (ax*y - ay*x)*s + az*dot*(1.0f - c);
    x = nx; y = ny; z = nz;
}

static void FillRotatedCubeCorners(float corners_out[8][3], float angle_deg)
{
    float a = fmodf(angle_deg, 360.0f * 6.0f);
    if(a < 0.0f) a += 360.0f * 6.0f;
    float angle_rad = a * (float)(M_PI / 180.0);

    float ax = 0.0f, ay = 0.0f, az = 1.0f;
    if(a > 4.0f * 360.0f)
    {
        ax = 0.0f; ay = 1.0f; az = 0.0f;
    }
    else if(a > 2.0f * 360.0f)
    {
        ax = ay = az = 1.0f / sqrtf(3.0f);
    }

    float corners[8][3] = {
        {-1,-1,-1}, {+1,-1,-1}, {-1,+1,-1}, {+1,+1,-1},
        {-1,-1,+1}, {+1,-1,+1}, {-1,+1,+1}, {+1,+1,+1}
    };
    for(int i = 0; i < 8; i++)
    {
        corners_out[i][0] = corners[i][0];
        corners_out[i][1] = corners[i][1];
        corners_out[i][2] = corners[i][2];
        rotate_axis_angle(corners_out[i][0], corners_out[i][1], corners_out[i][2], ax, ay, az, angle_rad);
    }
}

float WireframeCube::PointToSegmentDistance(float px, float py, float pz,
                                               float ax, float ay, float az,
                                               float bx, float by, float bz)
{
    float dx = bx - ax, dy = by - ay, dz = bz - az;
    float len2 = dx*dx + dy*dy + dz*dz;
    if(len2 < 1e-10f) return sqrtf((px-ax)*(px-ax) + (py-ay)*(py-ay) + (pz-az)*(pz-az));
    float t = ((px-ax)*dx + (py-ay)*dy + (pz-az)*dz) / len2;
    t = fmaxf(0.0f, fminf(1.0f, t));
    float qx = ax + t*dx, qy = ay + t*dy, qz = az + t*dz;
    return sqrtf((px-qx)*(px-qx) + (py-qy)*(py-qy) + (pz-qz)*(pz-qz));
}

WireframeCube::WireframeCube(QWidget* parent) : SpatialEffect3D(parent) {}

EffectInfo3D WireframeCube::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 2;
    info.effect_name = "Wireframe Cube";
    info.effect_description = "Rotating wireframe cube (Mega-Cube style); soft glow along edges";
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
    cube_cache_time = -1e9f;
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

    Vector3D rot = TransformPointByRotation(x, y, z, origin);
    float coord2 = NormalizeGridAxis01(rot.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(stratum_layout_mode, sw, stratum_tuning_);
    const bool strat_on = (stratum_layout_mode == 1);
    const float tm = std::max(0.25f, bb.tight_mul);

    float corners_use[8][3];
    float angle_deg_for_hue;

    if(!strat_on)
    {
        if(fabsf(time - cube_cache_time) > 0.001f)
        {
            cube_cache_time = time;
            float progress_val = CalculateProgress(time);
            float angle_deg = fmodf(progress_val * 360.0f, 360.0f * 6.0f);
            if(angle_deg < 0.0f) angle_deg += 360.0f * 6.0f;
            cached_angle_deg = angle_deg;
            FillRotatedCubeCorners(cube_corners, angle_deg);
        }
        for(int i = 0; i < 8; i++)
        {
            corners_use[i][0] = cube_corners[i][0];
            corners_use[i][1] = cube_corners[i][1];
            corners_use[i][2] = cube_corners[i][2];
        }
        angle_deg_for_hue = cached_angle_deg;
    }
    else
    {
        float progress_val = CalculateProgress(time * bb.speed_mul);
        angle_deg_for_hue = fmodf(progress_val * 360.0f, 360.0f * 6.0f);
        if(angle_deg_for_hue < 0.0f) angle_deg_for_hue += 360.0f * 6.0f;
        FillRotatedCubeCorners(corners_use, angle_deg_for_hue);
    }

    EffectGridAxisHalfExtents e = MakeEffectGridAxisHalfExtents(grid, GetNormalizedScale());
    float lx = (rot.x - origin.x) / e.hw;
    float ly = (rot.y - origin.y) / e.hh;
    float lz = (rot.z - origin.z) / e.hd;

    const int edges[12][2] = {
        {0,1},{2,3},{0,2},{1,3},{4,5},{6,7},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}
    };
    float sigma = std::max(thickness, 0.02f);
    float sigma_sq = sigma * sigma / (tm * tm);
    const float d2_cutoff = 9.0f * sigma_sq;
    float total = 0.0f;
    for(int ei = 0; ei < 12; ei++)
    {
        int i = edges[ei][0], j = edges[ei][1];
        float d = PointToSegmentDistance(lx, ly, lz,
            corners_use[i][0], corners_use[i][1], corners_use[i][2],
            corners_use[j][0], corners_use[j][1], corners_use[j][2]);
        float d2 = d * d;
        if(d2 > d2_cutoff) continue;
        total += expf(-d2 / sigma_sq);
    }
    total = fminf(1.0f, total * 0.35f);
    total *= std::max(0.0f, std::min(1.0f, line_brightness));

    float hue = fmodf(angle_deg_for_hue * 0.1f, 360.0f);
    if(hue < 0.0f) hue += 360.0f;
    float detail = std::max(0.05f, GetScaledDetail());
    hue = fmodf(hue * (0.6f + 0.4f * detail) + time * GetScaledFrequency() * 12.0f * bb.speed_mul
                  + lx * 42.0f + ly * 38.0f + lz * 46.0f + bb.phase_deg,
              360.0f);
    if(hue < 0.0f) hue += 360.0f;
    float pal01 = 0.5f;
    if(wireframecube_strip_cmap_on)
    {
        const float size_m = GetNormalizedSize();
        const float ph01 =
            std::fmod(CalculateProgress(time) * 0.2f + time * GetScaledFrequency() * 12.0f * bb.speed_mul * (1.f / 360.f) +
                          bb.phase_deg * (1.f / 360.f) + lx * 0.03f + ly * 0.03f + lz * 0.03f + 1.f,
                      1.f);
        pal01 = SampleStripKernelPalette01(wireframecube_strip_cmap_kernel,
                                           wireframecube_strip_cmap_rep,
                                           wireframecube_strip_cmap_unfold,
                                           wireframecube_strip_cmap_dir,
                                           ph01,
                                           time,
                                           grid,
                                           size_m,
                                           origin,
                                           rot);
        hue = pal01 * 360.f;
    }
    RGBColor c = GetRainbowMode() ? GetRainbowColor(hue) : GetColorAtPosition(pal01);
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
    cube_cache_time = -1e9f;
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}
