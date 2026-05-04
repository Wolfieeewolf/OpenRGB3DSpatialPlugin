// SPDX-License-Identifier: GPL-2.0-only

#include "ParticleKernelTrail.h"
#include "SpatialKernelColormap.h"
#include "StripKernelColormapPanel.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace {
float Hash01(unsigned int s)
{
    s = (s * 1103515245u + 12345u) & 0x7FFFFFFFu;
    return (float)s / 2147483648.0f;
}
} // namespace

ParticleKernelTrail::ParticleKernelTrail(QWidget* parent)
    : SpatialEffect3D(parent)
{
    SetRainbowMode(false);
}

EffectInfo3D ParticleKernelTrail::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 1;
    info.effect_name = "Particle Kernel Trail";
    info.effect_description =
        "Drifting particles smear strip-kernel palette samples—temporal coherence without a full 3D history grid.";
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

void ParticleKernelTrail::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* pc_row = new QHBoxLayout();
    pc_row->addWidget(new QLabel("Particles:"));
    QSlider* pc_slider = new QSlider(Qt::Horizontal);
    pc_slider->setRange(4, 48);
    pc_slider->setValue(particle_count);
    QLabel* pc_label = new QLabel(QString::number(particle_count));
    pc_row->addWidget(pc_slider);
    pc_row->addWidget(pc_label);
    vbox->addLayout(pc_row);
    connect(pc_slider, &QSlider::valueChanged, this, [this, pc_label](int v) {
        particle_count = std::clamp(v, 4, 48);
        particles.clear();
        pc_label->setText(QString::number(particle_count));
        emit ParametersChanged();
    });

    QHBoxLayout* rad_row = new QHBoxLayout();
    rad_row->addWidget(new QLabel("Glow radius:"));
    QSlider* rad_slider = new QSlider(Qt::Horizontal);
    rad_slider->setRange(5, 80);
    rad_slider->setValue((int)(particle_radius * 100.0f));
    QLabel* rad_label = new QLabel(QString::number(particle_radius, 'f', 2));
    rad_row->addWidget(rad_slider);
    rad_row->addWidget(rad_label);
    vbox->addLayout(rad_row);
    connect(rad_slider, &QSlider::valueChanged, this, [this, rad_label](int v) {
        particle_radius = v / 100.0f;
        rad_label->setText(QString::number(particle_radius, 'f', 2));
        emit ParametersChanged();
    });

    QHBoxLayout* tr_row = new QHBoxLayout();
    tr_row->addWidget(new QLabel("Trail smoothing:"));
    QSlider* tr_slider = new QSlider(Qt::Horizontal);
    tr_slider->setRange(2, 95);
    tr_slider->setValue((int)(trail_lerp * 100.0f));
    QLabel* tr_label = new QLabel(QString::number(trail_lerp, 'f', 2));
    tr_row->addWidget(tr_slider);
    tr_row->addWidget(tr_label);
    vbox->addLayout(tr_row);
    connect(tr_slider, &QSlider::valueChanged, this, [this, tr_label](int v) {
        trail_lerp = v / 100.0f;
        tr_label->setText(QString::number(trail_lerp, 'f', 2));
        emit ParametersChanged();
    });

    strip_cmap_panel = new StripKernelColormapPanel(w);
    strip_cmap_panel->mirrorStateFromEffect(pktrail_strip_cmap_on,
                                            pktrail_strip_cmap_kernel,
                                            pktrail_strip_cmap_rep,
                                            pktrail_strip_cmap_unfold,
                                            pktrail_strip_cmap_dir,
                                            pktrail_strip_cmap_color_style);
    vbox->addWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &ParticleKernelTrail::SyncStripColormapFromPanel);

    stratum_panel = new StratumBandPanel(w);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    vbox->addWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &ParticleKernelTrail::OnStratumBandChanged);
    OnStratumBandChanged();

    AddWidgetToParent(w, parent);
}

void ParticleKernelTrail::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    pktrail_strip_cmap_on = strip_cmap_panel->useStripColormap();
    pktrail_strip_cmap_kernel = strip_cmap_panel->kernelId();
    pktrail_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    pktrail_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    pktrail_strip_cmap_dir = strip_cmap_panel->directionDeg();
    pktrail_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
}

void ParticleKernelTrail::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    emit ParametersChanged();
}

void ParticleKernelTrail::EnsureParticles(int count, unsigned int seed)
{
    if((int)particles.size() == count && particle_seed == seed)
        return;
    particle_seed = seed;
    particles.resize((size_t)count);
    for(int i = 0; i < count; i++)
    {
        unsigned int s = seed ^ (unsigned int)(i * 1664525u + 1013904223u);
        Particle& p = particles[(size_t)i];
        p.x = Hash01(s);
        p.y = Hash01(s + 1u);
        p.z = Hash01(s + 2u);
        p.vx = (Hash01(s + 3u) - 0.5f) * 0.35f;
        p.vy = (Hash01(s + 4u) - 0.5f) * 0.28f;
        p.vz = (Hash01(s + 5u) - 0.5f) * 0.35f;
        p.trail_p01 = Hash01(s + 6u);
    }
}

void ParticleKernelTrail::TickParticles(float time, const GridContext3D& grid)
{
    EnsureParticles(particle_count, 0xC001D00Du);

    float dt = 0.016f;
    if(last_tick_time > -1e8f)
        dt = std::clamp(time - last_tick_time, 0.0f, 0.05f);
    last_tick_time = time;

    Vector3D origin = GetEffectOriginGrid(grid);
    const float size_m = GetNormalizedSize();
    float speed = (0.35f + GetScaledSpeed() * 0.02f);

    for(Particle& p : particles)
    {
        p.x = std::clamp(p.x + p.vx * dt * speed, 0.0f, 1.0f);
        p.y = std::clamp(p.y + p.vy * dt * speed, 0.0f, 1.0f);
        p.z = std::clamp(p.z + p.vz * dt * speed, 0.0f, 1.0f);
        if(p.x <= 0.0f || p.x >= 1.0f)
            p.vx = -p.vx;
        if(p.y <= 0.0f || p.y >= 1.0f)
            p.vy = -p.vy;
        if(p.z <= 0.0f || p.z >= 1.0f)
            p.vz = -p.vz;

        Vector3D pos{grid.min_x + p.x * grid.width, grid.min_y + p.y * grid.height, grid.min_z + p.z * grid.depth};
        float target = 0.5f;
        if(pktrail_strip_cmap_on)
        {
            float ph = std::fmod(time * 0.06f + p.x * 0.1f + p.y * 0.07f, 1.0f);
            target = SampleStripKernelPalette01(pktrail_strip_cmap_kernel,
                                                pktrail_strip_cmap_rep,
                                                pktrail_strip_cmap_unfold,
                                                pktrail_strip_cmap_dir,
                                                ph,
                                                time,
                                                grid,
                                                size_m,
                                                origin,
                                                pos);
        }
        float a = std::clamp(trail_lerp, 0.02f, 0.98f);
        p.trail_p01 = p.trail_p01 * a + target * (1.0f - a);
    }
}

void ParticleKernelTrail::UpdateParams(SpatialEffectParams& /*params*/) {}

RGBColor ParticleKernelTrail::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
        return 0x00000000;

    TickParticles(time, grid);

    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x, rel_y = y - origin.y, rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float nx = (x - grid.min_x) / std::max(1e-5f, grid.width);
    float ny = (y - grid.min_y) / std::max(1e-5f, grid.height);
    float nz = (z - grid.min_z) / std::max(1e-5f, grid.depth);
    nx = std::clamp(nx, 0.0f, 1.0f);
    ny = std::clamp(ny, 0.0f, 1.0f);
    nz = std::clamp(nz, 0.0f, 1.0f);

    float coord2 = NormalizeGridAxis01(y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(stratum_layout_mode, sw, stratum_tuning_);

    float r_acc = 0.0f, g_acc = 0.0f, b_acc = 0.0f, w_acc = 0.0f;
    float span = std::max(grid.width, std::max(grid.height, grid.depth));
    float rad = particle_radius * (0.4f + 0.6f * GetNormalizedSize());
    float rad_n = rad / std::max(1e-5f, span);

    for(const Particle& p : particles)
    {
        float dx = nx - p.x, dy = ny - p.y, dz = nz - p.z;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        float w = std::exp(-(dist * dist) / std::max(1e-6f, rad_n * rad_n * 0.18f));
        if(w < 1e-4f)
            continue;
        RGBColor col;
        if(pktrail_strip_cmap_on)
        {
            float p01 = p.trail_p01;
            p01 = ApplyVoxelDriveToPalette01(p01, x, y, z, time, grid);
            col = ResolveStripKernelFinalColor(*this, pktrail_strip_cmap_kernel, p01, pktrail_strip_cmap_color_style, time,
                                               GetScaledFrequency() * 12.0f * bb.speed_mul);
        }
        else if(GetRainbowMode())
        {
            col = GetRainbowColor(p.trail_p01 * 360.0f + bb.phase_deg);
        }
        else
        {
            col = GetColorAtPosition(p.trail_p01);
        }
        r_acc += w * (float)(col & 0xFF);
        g_acc += w * (float)((col >> 8) & 0xFF);
        b_acc += w * (float)((col >> 16) & 0xFF);
        w_acc += w;
    }

    if(w_acc < 1e-4f)
        return 0x00000000;

    int ri = (int)(r_acc / w_acc);
    int gi = (int)(g_acc / w_acc);
    int bi = (int)(b_acc / w_acc);
    float gain = 0.35f + 0.65f * std::min(1.0f, w_acc * 1.8f);
    ri = std::clamp((int)(ri * gain), 0, 255);
    gi = std::clamp((int)(gi * gain), 0, 255);
    bi = std::clamp((int)(bi * gain), 0, 255);
    return PostProcessColorGrid((RGBColor)((bi << 16) | (gi << 8) | ri));
}

nlohmann::json ParticleKernelTrail::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["pktrail_particle_count"] = particle_count;
    j["pktrail_particle_radius"] = particle_radius;
    j["pktrail_trail_lerp"] = trail_lerp;
    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(j,
                                           "pktrail_stratum_layout_mode",
                                           sm,
                                           st,
                                           "pktrail_stratum_band_speed_pct",
                                           "pktrail_stratum_band_tight_pct",
                                           "pktrail_stratum_band_phase_deg");
    StripColormapSaveJson(j,
                          "pktrail",
                          pktrail_strip_cmap_on,
                          pktrail_strip_cmap_kernel,
                          pktrail_strip_cmap_rep,
                          pktrail_strip_cmap_unfold,
                          pktrail_strip_cmap_dir,
                          pktrail_strip_cmap_color_style);
    return j;
}

void ParticleKernelTrail::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("pktrail_particle_count"))
        particle_count = std::clamp(settings["pktrail_particle_count"].get<int>(), 4, 48);
    if(settings.contains("pktrail_particle_radius"))
        particle_radius = std::clamp(settings["pktrail_particle_radius"].get<float>(), 0.05f, 0.8f);
    if(settings.contains("pktrail_trail_lerp"))
        trail_lerp = std::clamp(settings["pktrail_trail_lerp"].get<float>(), 0.02f, 0.95f);
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "pktrail_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "pktrail_stratum_band_speed_pct",
                                            "pktrail_stratum_band_tight_pct",
                                            "pktrail_stratum_band_phase_deg");
    StripColormapLoadJson(settings,
                          "pktrail",
                          pktrail_strip_cmap_on,
                          pktrail_strip_cmap_kernel,
                          pktrail_strip_cmap_rep,
                          pktrail_strip_cmap_unfold,
                          pktrail_strip_cmap_dir,
                          pktrail_strip_cmap_color_style,
                          GetRainbowMode());
    particles.clear();
    last_tick_time = -1e9f;
    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(pktrail_strip_cmap_on,
                                                pktrail_strip_cmap_kernel,
                                                pktrail_strip_cmap_rep,
                                                pktrail_strip_cmap_unfold,
                                                pktrail_strip_cmap_dir,
                                                pktrail_strip_cmap_color_style);
    }
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}

REGISTER_EFFECT_3D(ParticleKernelTrail)
