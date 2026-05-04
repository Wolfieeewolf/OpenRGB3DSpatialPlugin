// SPDX-License-Identifier: GPL-2.0-only

#include "LightningKernelFlash.h"
#include "SpatialKernelColormap.h"
#include "StripKernelColormapPanel.h"
#include "SpatialLayerCore.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

LightningKernelFlash::LightningKernelFlash(QWidget* parent)
    : SpatialEffect3D(parent)
{
    SetRainbowMode(false);
    std::vector<RGBColor> cols;
    cols.push_back(0x00FFFFFF);
    SetColors(cols);
}

EffectInfo3D LightningKernelFlash::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 1;
    info.effect_name = "Lightning Kernel Flash";
    info.effect_description =
        "Bright white ceiling flash with bolt color sampled from a strip kernel along the strike path.";
    info.category = "Spatial";
    info.is_reversible = false;
    info.supports_random = true;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_frequency = true;
    info.default_frequency_scale = 18.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = true;
    return info;
}

void LightningKernelFlash::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* rate_row = new QHBoxLayout();
    rate_row->addWidget(new QLabel("Flash rate:"));
    QSlider* rate_slider = new QSlider(Qt::Horizontal);
    rate_slider->setRange(5, 50);
    rate_slider->setValue((int)(flash_rate * 100.0f));
    QLabel* rate_label = new QLabel(QString::number(flash_rate, 'f', 2));
    rate_row->addWidget(rate_slider);
    rate_row->addWidget(rate_label);
    vbox->addLayout(rate_row);
    connect(rate_slider, &QSlider::valueChanged, this, [this, rate_label](int v) {
        flash_rate = v / 100.0f;
        rate_label->setText(QString::number(flash_rate, 'f', 2));
        emit ParametersChanged();
    });

    QHBoxLayout* dur_row = new QHBoxLayout();
    dur_row->addWidget(new QLabel("Duration (ms):"));
    QSlider* dur_slider = new QSlider(Qt::Horizontal);
    dur_slider->setRange(20, 250);
    dur_slider->setValue((int)(flash_duration * 1000.0f));
    QLabel* dur_label = new QLabel(QString::number((int)(flash_duration * 1000.f)));
    dur_row->addWidget(dur_slider);
    dur_row->addWidget(dur_label);
    vbox->addLayout(dur_row);
    connect(dur_slider, &QSlider::valueChanged, this, [this, dur_label](int v) {
        flash_duration = v / 1000.0f;
        dur_label->setText(QString::number(v));
        emit ParametersChanged();
    });

    QHBoxLayout* br_row = new QHBoxLayout();
    br_row->addWidget(new QLabel("Fork branches:"));
    QSlider* br_slider = new QSlider(Qt::Horizontal);
    br_slider->setRange(0, 6);
    br_slider->setValue(fork_branches);
    QLabel* br_label = new QLabel(QString::number(fork_branches));
    br_row->addWidget(br_slider);
    br_row->addWidget(br_label);
    vbox->addLayout(br_row);
    connect(br_slider, &QSlider::valueChanged, this, [this, br_label](int v) {
        fork_branches = std::clamp(v, 0, 6);
        br_label->setText(QString::number(fork_branches));
        emit ParametersChanged();
    });

    strip_cmap_panel = new StripKernelColormapPanel(w);
    strip_cmap_panel->mirrorStateFromEffect(lkflash_strip_cmap_on,
                                            lkflash_strip_cmap_kernel,
                                            lkflash_strip_cmap_rep,
                                            lkflash_strip_cmap_unfold,
                                            lkflash_strip_cmap_dir,
                                            lkflash_strip_cmap_color_style);
    vbox->addWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &LightningKernelFlash::SyncStripColormapFromPanel);

    AddWidgetToParent(w, parent);
}

void LightningKernelFlash::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    lkflash_strip_cmap_on = strip_cmap_panel->useStripColormap();
    lkflash_strip_cmap_kernel = strip_cmap_panel->kernelId();
    lkflash_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    lkflash_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    lkflash_strip_cmap_dir = strip_cmap_panel->directionDeg();
    lkflash_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
}

float LightningKernelFlash::hash11(float t)
{
    float s = std::sin(t * 12.9898f) * 43758.5453f;
    return s - std::floor(s);
}

float LightningKernelFlash::HashF(unsigned int seed)
{
    seed = (seed * 1103515245u + 12345u) & 0x7FFFFFFFu;
    return (float)seed / 2147483648.0f;
}

float LightningKernelFlash::DistToSegment(float px, float py, float pz, float ax, float ay, float az, float bx, float by, float bz)
{
    float dx = bx - ax, dy = by - ay, dz = bz - az;
    float len_sq = dx * dx + dy * dy + dz * dz;
    if(len_sq < 1e-12f)
        return std::sqrt((px - ax) * (px - ax) + (py - ay) * (py - ay) + (pz - az) * (pz - az));

    float t = ((px - ax) * dx + (py - ay) * dy + (pz - az) * dz) / len_sq;
    t = std::max(0.0f, std::min(1.0f, t));
    float qx = ax + t * dx, qy = ay + t * dy, qz = az + t * dz;
    float ddx = px - qx, ddy = py - qy, ddz = pz - qz;
    return std::sqrt(ddx * ddx + ddy * ddy + ddz * ddz);
}

void LightningKernelFlash::UpdateParams(SpatialEffectParams& /*params*/) {}

RGBColor LightningKernelFlash::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
        return 0x00000000;

    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float coord2 = NormalizeGridAxis01(y, grid.min_y, grid.max_y);
    float norm_y = coord2;
    float sky_factor = 0.5f + 0.5f * norm_y;

    float time_m = time * (0.5f + 0.5f * GetScaledSpeed() * 0.02f);
    float rate = std::max(0.05f, std::min(0.5f, flash_rate));
    float interval = 1.0f / rate;
    float dur = std::max(0.02f, std::min(0.25f, flash_duration));

    float cycle = std::floor(time_m / interval);
    float flash_offset = hash11(cycle) * interval * 0.6f;
    float phase_in_cycle = time_m - cycle * interval;
    float flash_phase = phase_in_cycle - flash_offset;

    float flash_env = 0.0f;
    if(flash_phase >= 0.0f && flash_phase < dur)
    {
        float rise = (flash_phase < dur * 0.15f) ? (flash_phase / (dur * 0.15f)) : 1.0f;
        float fall = (flash_phase > dur * 0.6f) ? (1.0f - (flash_phase - dur * 0.6f) / (dur * 0.4f)) : 1.0f;
        flash_env = rise * fall;
    }

    if(flash_env <= 0.001f)
        return 0x00000000;

    float room_avg = EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f;
    float bolt_core = room_avg * (0.010f + 0.008f * GetNormalizedSize());
    float bolt_glow = room_avg * (0.035f + 0.020f * GetNormalizedSize());

    unsigned int cyc = (unsigned int)std::max(0.0f, cycle);
    unsigned int seed = cyc * 9781u ^ 1337u;

    float sx = grid.min_x + HashF(seed + 1u) * grid.width;
    float sy = grid.max_y;
    float sz = grid.min_z + HashF(seed + 2u) * grid.depth;
    float ex = sx + (HashF(seed + 3u) - 0.5f) * grid.width * 0.35f;
    float ey = grid.min_y + (0.10f + 0.45f * HashF(seed + 4u)) * grid.height;
    float ez = sz + (HashF(seed + 5u) - 0.5f) * grid.depth * 0.35f;

    float d_main = DistToSegment(x, y, z, sx, sy, sz, ex, ey, ez);
    float main_core = std::max(0.0f, 1.0f - d_main / (bolt_core + 1e-6f));
    float main_glow = std::max(0.0f, 1.0f - d_main / (bolt_glow + 1e-6f)) * 0.65f;
    float bolt_intensity = main_core + main_glow;

    for(int br = 0; br < fork_branches; br++)
    {
        unsigned int bseed = seed ^ (unsigned int)(br * 3571u + 17u);
        float t = 0.20f + 0.70f * HashF(bseed + 1u);
        float bx = sx + (ex - sx) * t;
        float by = sy + (ey - sy) * t;
        float bz = sz + (ez - sz) * t;
        float bdx = (HashF(bseed + 2u) - 0.5f) * grid.width * 0.25f;
        float bdy = -(0.08f + 0.20f * HashF(bseed + 3u)) * grid.height;
        float bdz = (HashF(bseed + 4u) - 0.5f) * grid.depth * 0.25f;
        float tx = bx + bdx;
        float ty = by + bdy;
        float tz = bz + bdz;
        float d_branch = DistToSegment(x, y, z, bx, by, bz, tx, ty, tz);
        float branch_core = std::max(0.0f, 1.0f - d_branch / (bolt_core * 0.75f + 1e-6f));
        float branch_glow = std::max(0.0f, 1.0f - d_branch / (bolt_glow * 0.85f + 1e-6f)) * 0.45f;
        bolt_intensity = std::max(bolt_intensity, branch_core + branch_glow);
    }

    float flash_white = flash_env * sky_factor;
    float bolt_part = flash_env * bolt_intensity * sky_factor;

    const float size_m = GetNormalizedSize();
    float t_line = 0.0f;
    {
        float lx = ex - sx, ly = ey - sy, lz = ez - sz;
        float len_sq = lx * lx + ly * ly + lz * lz;
        if(len_sq > 1e-12f)
        {
            t_line = ((x - sx) * lx + (y - sy) * ly + (z - sz) * lz) / len_sq;
            t_line = std::max(0.0f, std::min(1.0f, t_line));
        }
    }
    Vector3D on_line{sx + (ex - sx) * t_line, sy + (ey - sy) * t_line, sz + (ez - sz) * t_line};

    RGBColor bolt_rgb = 0x00FFFFFF;
    if(lkflash_strip_cmap_on && bolt_part > 0.02f)
    {
        float ph = std::fmod(t_line * 0.97f + flash_env * 0.08f + time * 0.02f, 1.0f);
        float p01 = SampleStripKernelPalette01(lkflash_strip_cmap_kernel,
                                               lkflash_strip_cmap_rep,
                                               lkflash_strip_cmap_unfold,
                                               lkflash_strip_cmap_dir,
                                               ph,
                                               time,
                                               grid,
                                               size_m,
                                               origin,
                                               on_line);
        p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
        bolt_rgb = ResolveStripKernelFinalColor(*this, lkflash_strip_cmap_kernel, p01, lkflash_strip_cmap_color_style, time,
                                                GetScaledFrequency() * 12.0f);
    }
    else if(GetRainbowMode() && bolt_part > 0.02f)
    {
        bolt_rgb = GetRainbowColor(t_line * 300.0f + time * 40.0f);
    }
    else if(bolt_part > 0.02f)
    {
        const std::vector<RGBColor>& cols = GetColors();
        bolt_rgb = (!cols.empty()) ? cols[0] : 0x00FFFFFF;
    }

    int rw = (int)(255 * flash_white);
    int gw = (int)(255 * flash_white);
    int bw = (int)(255 * flash_white);

    int r = (bolt_rgb & 0xFF), g = ((bolt_rgb >> 8) & 0xFF), b = ((bolt_rgb >> 16) & 0xFF);
    r = std::min(255, rw + (int)(r * bolt_part));
    g = std::min(255, gw + (int)(g * bolt_part));
    b = std::min(255, bw + (int)(b * bolt_part));

    return PostProcessColorGrid((RGBColor)((b << 16) | (g << 8) | r));
}

nlohmann::json LightningKernelFlash::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["lkflash_flash_rate"] = flash_rate;
    j["lkflash_flash_duration"] = flash_duration;
    j["lkflash_fork_branches"] = fork_branches;
    StripColormapSaveJson(j,
                          "lkflash",
                          lkflash_strip_cmap_on,
                          lkflash_strip_cmap_kernel,
                          lkflash_strip_cmap_rep,
                          lkflash_strip_cmap_unfold,
                          lkflash_strip_cmap_dir,
                          lkflash_strip_cmap_color_style);
    return j;
}

void LightningKernelFlash::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("lkflash_flash_rate"))
        flash_rate = std::clamp(settings["lkflash_flash_rate"].get<float>(), 0.05f, 0.5f);
    if(settings.contains("lkflash_flash_duration"))
        flash_duration = std::clamp(settings["lkflash_flash_duration"].get<float>(), 0.02f, 0.25f);
    if(settings.contains("lkflash_fork_branches"))
        fork_branches = std::clamp(settings["lkflash_fork_branches"].get<int>(), 0, 6);
    StripColormapLoadJson(settings,
                          "lkflash",
                          lkflash_strip_cmap_on,
                          lkflash_strip_cmap_kernel,
                          lkflash_strip_cmap_rep,
                          lkflash_strip_cmap_unfold,
                          lkflash_strip_cmap_dir,
                          lkflash_strip_cmap_color_style,
                          GetRainbowMode());
    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(lkflash_strip_cmap_on,
                                                lkflash_strip_cmap_kernel,
                                                lkflash_strip_cmap_rep,
                                                lkflash_strip_cmap_unfold,
                                                lkflash_strip_cmap_dir,
                                                lkflash_strip_cmap_color_style);
    }
}

REGISTER_EFFECT_3D(LightningKernelFlash)
