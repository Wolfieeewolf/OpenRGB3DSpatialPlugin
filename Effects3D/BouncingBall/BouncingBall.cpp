// SPDX-License-Identifier: GPL-2.0-only

#include "BouncingBall.h"
#include "SpatialKernelColormap.h"
#include "StripKernelColormapPanel.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"
#include "EffectHelpers.h"
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QSlider>
#include <cmath>
#include <vector>
#include <algorithm>

namespace
{

float HashFloat01(unsigned int seed)
{
    unsigned int value = seed ^ 0x27D4EB2D;
    value = (value ^ 61U) ^ (value >> 16U);
    value = value + (value << 3U);
    value = value ^ (value >> 4U);
    value = value * 0x27D4EB2D;
    value = value ^ (value >> 15U);
    return (value & 0xFFFFU) / 65535.0f;
}

void IntegrateBall(float& pos_x, float& pos_y, float& pos_z,
                   float& vel_x, float& vel_y, float& vel_z,
                   float dt, float gravity, float e,
                   float floor_bounce_vy_up,
                   float xmin, float xmax, float ymin, float ymax, float zmin, float zmax)
{
    vel_y -= gravity * dt;

    pos_x += vel_x * dt;
    pos_y += vel_y * dt;
    pos_z += vel_z * dt;

    if(pos_x <= xmin)
    {
        pos_x = xmin;
        vel_x = -vel_x * e;
    }
    else if(pos_x >= xmax)
    {
        pos_x = xmax;
        vel_x = -vel_x * e;
    }

    if(pos_y <= ymin)
    {
        pos_y = ymin;
        /* Match OpenRGBEffectsPlugin idea: reset upward speed from sqrt(2 g h) so energy
         * does not drain away from friction / float error; balls keep bouncing. */
        vel_y = floor_bounce_vy_up;
    }
    else if(pos_y >= ymax)
    {
        pos_y = ymax;
        vel_y = -vel_y * e;
    }

    if(pos_z <= zmin)
    {
        pos_z = zmin;
        vel_z = -vel_z * e;
    }
    else if(pos_z >= zmax)
    {
        pos_z = zmax;
        vel_z = -vel_z * e;
    }
}

void ClampBallSpeed(float& vx, float& vy, float& vz, float v_max)
{
    const float s2 = vx * vx + vy * vy + vz * vz;
    const float m2 = v_max * v_max;
    if(s2 <= m2 || s2 < 1e-12f) return;
    const float inv = v_max / sqrtf(s2);
    vx *= inv;
    vy *= inv;
    vz *= inv;
}

void SeedBall(unsigned int k,
              float xmin, float ymin, float zmin,
              float span_x, float span_y, float span_z,
              float motion, float gravity, float radius_basis,
              CachedBall3D& b)
{
    const float hy = HashFloat01(k * 313U + 5U);
    const float hx = HashFloat01(k * 131U);
    const float hz = HashFloat01(k * 919U);

    b.px = xmin + hx * span_x;
    b.py = ymin + (0.08f + hy * 0.88f) * span_y;
    b.pz = zmin + hz * span_z;

    const float drop_h = fmaxf(span_y * (0.14f + 0.72f * HashFloat01(k * 419U + 11U)),
                               radius_basis * 0.04f);
    b.floor_bounce_vy = sqrtf(2.0f * gravity * drop_h) * 0.996f;

    const float horiz = (0.30f + 1.20f * motion) * radius_basis;
    b.vx = (HashFloat01(k * 733U) * 2.0f - 1.0f) * horiz;
    b.vz = (HashFloat01(k * 829U) * 2.0f - 1.0f) * horiz;
    b.vy = (0.40f + HashFloat01(k * 577U) * 0.60f) * radius_basis * (0.50f + 1.05f * motion);
}

} // namespace

BouncingBall::BouncingBall(QWidget* parent) : SpatialEffect3D(parent)
{
    count_slider = nullptr;
    count_label = nullptr;
    ball_count = 1;
    SetRainbowMode(true);
}

BouncingBall::~BouncingBall() = default;

EffectInfo3D BouncingBall::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 3;
    info.effect_name = "Bouncing Ball";
    info.effect_description =
        "Independent balls bouncing in the room (no ball–ball physics). Physics runs forward without looping; use Speed for motion rate; optional floor/mid/ceiling band tuning";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_BOUNCING_BALL;
    info.is_reversible = false;
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

    info.default_speed_scale = 16.0f;
    info.default_frequency_scale = 20.0f;
    info.use_size_parameter = true;

    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = true;
    return info;
}

void BouncingBall::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* outer = new QVBoxLayout(w);
    outer->setContentsMargins(0, 0, 0, 0);
    QGridLayout* layout = new QGridLayout();
    outer->addLayout(layout);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(new QLabel("Balls:"), 0, 0);
    count_slider = new QSlider(Qt::Horizontal);
    count_slider->setRange(1, 50);
    count_slider->setValue(ball_count);
    count_slider->setToolTip("Number of balls. Each follows its own path inside the room.");
    layout->addWidget(count_slider, 0, 1);
    count_label = new QLabel(QString::number(ball_count));
    count_label->setMinimumWidth(30);
    layout->addWidget(count_label, 0, 2);

    strip_cmap_panel = new StripKernelColormapPanel(w);
    strip_cmap_panel->mirrorStateFromEffect(bouncingball_strip_cmap_on,
                                            bouncingball_strip_cmap_kernel,
                                            bouncingball_strip_cmap_rep,
                                            bouncingball_strip_cmap_unfold,
                                            bouncingball_strip_cmap_dir,
                                            bouncingball_strip_cmap_color_style);
    AddColorPatternWidget(strip_cmap_panel);
    connect(strip_cmap_panel, &StripKernelColormapPanel::colormapChanged, this, &BouncingBall::SyncStripColormapFromPanel);

    stratum_panel = new StratumBandPanel(w);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    AddBandModulationWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &BouncingBall::OnStratumBandChanged);
    OnStratumBandChanged();

    AddWidgetToParent(w, parent);

    connect(count_slider, &QSlider::valueChanged, this, &BouncingBall::OnBallParameterChanged);
}

void BouncingBall::SyncStripColormapFromPanel()
{
    if(!strip_cmap_panel)
        return;
    bouncingball_strip_cmap_on = strip_cmap_panel->useStripColormap();
    bouncingball_strip_cmap_kernel = strip_cmap_panel->kernelId();
    bouncingball_strip_cmap_rep = strip_cmap_panel->kernelRepeats();
    bouncingball_strip_cmap_unfold = strip_cmap_panel->unfoldMode();
    bouncingball_strip_cmap_dir = strip_cmap_panel->directionDeg();
    bouncingball_strip_cmap_color_style = strip_cmap_panel->colorStyle();
    emit ParametersChanged();
}

void BouncingBall::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
    ball_last_integrated_wall_time = -1e9f;
    emit ParametersChanged();
}

void BouncingBall::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_BOUNCING_BALL;
}

void BouncingBall::OnBallParameterChanged()
{
    if(count_slider)
    {
        ball_count = count_slider->value();
        if(count_label) count_label->setText(QString::number(ball_count));
    }
    emit ParametersChanged();
}


RGBColor BouncingBall::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    if(EffectGridSampleOutsideVolume(x, y, z, grid))
    {
        return 0x00000000;
    }
    Vector3D origin = GetEffectOriginGrid(grid);
    Vector3D rp = TransformPointByRotation(x, y, z, origin);
    float coord2 = NormalizeGridAxis01(rp.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float sw[3];
    EffectStratumBlend::WeightsForYNorm(coord2, strat_st, sw);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(stratum_layout_mode, sw, stratum_tuning_);
    /* Linear 0..1 from main effect Speed slider (not squared GetNormalizedSpeed) so the control is obvious. */
    const float speed_lin = fmaxf(0.02f, fminf(1.0f, GetSpeed() / 200.0f));
    const float motion = speed_lin * speed_lin * 0.28f + speed_lin * 0.72f;
    constexpr float k_wall_e = 0.987f;

    const float size_m = GetNormalizedSize();
    const float color_cycle = time * GetScaledFrequency() * 12.0f * bb.speed_mul;
    const float tm = std::max(0.25f, bb.tight_mul);
    const float detail = std::max(0.05f, GetScaledDetail()) * tm;
    const float radius_basis = EffectGridMedianHalfExtent(grid, GetNormalizedScale());
    const float R = radius_basis * (0.04f + 0.24f) * size_m;

    float xmin = grid.min_x + R;
    float xmax = grid.max_x - R;
    float ymin = grid.min_y + R;
    float ymax = grid.max_y - R;
    float zmin = grid.min_z + R;
    float zmax = grid.max_z - R;

    if(xmin > xmax) std::swap(xmin, xmax);
    if(ymin > ymax) std::swap(ymin, ymax);
    if(zmin > zmax) std::swap(zmin, zmax);

    const float span_x = xmax - xmin;
    const float span_y = ymax - ymin;
    const float span_z = zmax - zmin;
    if(span_x < 1e-3f || span_y < 1e-3f || span_z < 1e-3f)
    {
        return 0x00000000;
    }

    const float gravity = fmaxf(1e-5f, radius_basis * (0.092f + 0.070f * motion));

    const unsigned int N = ball_count == 0 ? 1u : ball_count;

    const float grid_hash = grid.min_x + grid.max_x * 31.0f + grid.min_y * 31.0f * 31.0f + grid.max_y * 31.0f * 31.0f * 31.0f;
    const int phys_key = (int)ball_count * 100000 + (int)(size_m * 500.f + 0.5f) + (int)(speed_lin * 2000.f + 0.5f);

    const bool count_changed = (ball_positions_cached.size() != N);
    const bool grid_changed = fabsf(grid_hash - ball_cache_grid_hash) > 0.01f;
    const bool phys_changed = (phys_key != ball_cache_phys_key);
    const bool structural = count_changed || grid_changed || phys_changed;
    const bool time_dirty = fabsf(time - ball_last_integrated_wall_time) > 0.001f;

    const float sim_phase_rate = 0.28f + motion * 2.35f;
    const float target_sim_t = time * sim_phase_rate;
    const float v_cap = radius_basis * (1.22f + 2.05f * motion);
    constexpr float k_sim_dt = 0.028f;
    constexpr float k_max_sim_advance = 6.0f;

    if(structural)
    {
        ball_cache_grid_hash = grid_hash;
        ball_cache_phys_key = phys_key;
        ball_positions_cached.resize(N);
        ball_physics_sim_t = 0.f;
        ball_last_integrated_wall_time = -1e9f;
        for(unsigned int k = 0; k < N; k++)
        {
            SeedBall(k, xmin, ymin, zmin, span_x, span_y, span_z, motion, gravity, radius_basis, ball_positions_cached[k]);
        }
    }

    if(structural || time_dirty)
    {
        float sim_remain = target_sim_t - ball_physics_sim_t;
        bool just_seeded = structural;

        if(!structural && (time + 0.0005f < ball_last_integrated_wall_time))
        {
            ball_physics_sim_t = 0.f;
            for(unsigned int k = 0; k < N; k++)
            {
                SeedBall(k, xmin, ymin, zmin, span_x, span_y, span_z, motion, gravity, radius_basis, ball_positions_cached[k]);
            }
            just_seeded = true;
            sim_remain = fminf(target_sim_t, k_max_sim_advance);
        }
        else if(sim_remain > k_max_sim_advance)
        {
            ball_physics_sim_t = 0.f;
            if(!just_seeded)
            {
                for(unsigned int k = 0; k < N; k++)
                {
                    SeedBall(k, xmin, ymin, zmin, span_x, span_y, span_z, motion, gravity, radius_basis, ball_positions_cached[k]);
                }
            }
            sim_remain = k_max_sim_advance;
        }

        int safety = 0;
        while(sim_remain > 1e-7f && safety < 32000)
        {
            const float h = fminf(k_sim_dt, sim_remain);
            for(unsigned int k = 0; k < N; k++)
            {
                CachedBall3D& ball = ball_positions_cached[k];
                IntegrateBall(ball.px, ball.py, ball.pz, ball.vx, ball.vy, ball.vz, h, gravity, k_wall_e, ball.floor_bounce_vy,
                              xmin, xmax, ymin, ymax, zmin, zmax);
                ClampBallSpeed(ball.vx, ball.vy, ball.vz, v_cap);
            }
            sim_remain -= h;
            safety++;
        }

        ball_physics_sim_t = target_sim_t;
        ball_last_integrated_wall_time = time;
    }

    const float rad_mul = 1.0f / std::max(0.25f, bb.tight_mul);
    const float glow_radius = R * 2.0f * rad_mul;
    const float glow_radius_sq = glow_radius * glow_radius;
    const float core_radius = R * 0.8f * rad_mul;

    float max_intensity = 0.0f;
    float hue_for_max = 120.0f;

    for(unsigned int k = 0; k < N; k++)
    {
        const CachedBall3D& ball = ball_positions_cached[k];
        const float dx = (x - ball.px);
        const float dy = (y - ball.py);
        const float dz = (z - ball.pz);
        const float dist_sq = dx * dx + dy * dy + dz * dz;
        if(dist_sq > glow_radius_sq) continue;

        const float dist = sqrtf(dist_sq);
        const float core_glow = fmax(0.0f, 1.0f - dist / (core_radius + 0.001f));
        const float outer_glow = 0.7f * fmax(0.0f, 1.0f - dist / (glow_radius + 0.001f));
        float intensity = powf(core_glow, 0.9f) + outer_glow;
        if(intensity < 0.05f) intensity = 0.05f;

        intensity *= 1.6f;
        intensity = fmax(0.0f, fmin(1.0f, intensity));

        if(intensity > max_intensity)
        {
            max_intensity = intensity;
            const float cx = (ball.px - grid.min_x) / fmaxf(1e-4f, grid.width);
            const float cy = (ball.py - grid.min_y) / fmaxf(1e-4f, grid.height);
            const float cz = (ball.pz - grid.min_z) / fmaxf(1e-4f, grid.depth);
            const float hue_spatial = (cx * 140.0f + cy * 200.0f + cz * 120.0f) * (0.55f + 0.45f * detail);
            const float hue_ball = float(k) * 38.0f;
            const float vh = hypotf(ball.vx, hypotf(ball.vy, ball.vz));
            const float vel_w = fminf(1.0f, vh / fmaxf(1e-4f, radius_basis * 0.35f));
            const float hue_vel = atan2f(ball.vz, ball.vx) * 57.2958f * 0.12f * vel_w;
            hue_for_max = fmodf(color_cycle + hue_ball + hue_spatial + hue_vel + bb.phase_deg, 360.0f);
            if(hue_for_max < 0.0f) hue_for_max += 360.0f;
        }
    }

    float strip_p01_bb = 0.f;
    if(bouncingball_strip_cmap_on)
    {
        const float ph01 =
            std::fmod(color_cycle * (1.f / 360.f) + bb.phase_deg * (1.f / 360.f) + 1.f, 1.f);
        strip_p01_bb = SampleStripKernelPalette01(bouncingball_strip_cmap_kernel,
                                                  bouncingball_strip_cmap_rep,
                                                  bouncingball_strip_cmap_unfold,
                                                  bouncingball_strip_cmap_dir,
                                                  ph01,
                                                  time,
                                                  grid,
                                                  size_m,
                                                  origin,
                                                  rp);
    }
    const float pos_driver = fmodf(0.28f + hue_for_max * (1.0f / 360.0f) + 1.0f, 1.0f);

    SpatialLayerCore::Basis basis;
    SpatialLayerCore::MakeBasisFromEffectEulerDegrees(GetRotationYaw(), GetRotationPitch(), GetRotationRoll(), basis);
    SpatialLayerCore::MapperSettings map;
    EffectStratumBlend::InitStratumBreaks(map);
    map.blend_softness = std::clamp(0.10f, 0.05f, 0.22f);
    map.center_size = std::clamp(0.11f + 0.24f * GetNormalizedScale(), 0.06f, 0.52f);
    map.directional_sharpness = std::clamp(1.05f + std::max(0.05f, detail) * 0.08f, 0.9f, 2.3f);

    SpatialLayerCore::SamplePoint sp{};
    sp.grid_x = x;
    sp.grid_y = y;
    sp.grid_z = z;
    sp.origin_x = origin.x;
    sp.origin_y = origin.y;
    sp.origin_z = origin.z;
    sp.y_norm = coord2;

    RGBColor final_color;
    if(bouncingball_strip_cmap_on)
    {
        float p01v = ApplyVoxelDriveToPalette01(strip_p01_bb, x, y, z, time, grid);
        const float rbow_mul = GetScaledFrequency() * 12.0f * bb.speed_mul;
        final_color = ResolveStripKernelFinalColor(*this, bouncingball_strip_cmap_kernel, p01v,
                                                   bouncingball_strip_cmap_color_style, time, rbow_mul);
    }
    else if(GetRainbowMode())
    {
        float hue = ApplySpatialRainbowHue(hue_for_max, pos_driver, basis, sp, map, time, &grid);
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
        float p = ApplySpatialPalette01(pos_driver, basis, sp, map, time, &grid);
        p = ApplyVoxelDriveToPalette01(p, x, y, z, time, grid);
        final_color = GetColorAtPosition(p);
    }
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    r = (unsigned char)(r * max_intensity);
    g = (unsigned char)(g * max_intensity);
    b = (unsigned char)(b * max_intensity);
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json BouncingBall::SaveSettings() const
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
                                           "bouncingball_stratum_layout_mode",
                                           sm,
                                           st,
                                           "bouncingball_stratum_band_speed_pct",
                                           "bouncingball_stratum_band_tight_pct",
                                           "bouncingball_stratum_band_phase_deg");
    j["ball_count"] = ball_count;
    StripColormapSaveJson(j,
                          "bouncingball",
                          bouncingball_strip_cmap_on,
                          bouncingball_strip_cmap_kernel,
                          bouncingball_strip_cmap_rep,
                          bouncingball_strip_cmap_unfold,
                          bouncingball_strip_cmap_dir,
                          bouncingball_strip_cmap_color_style);
    return j;
}

void BouncingBall::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "bouncingball_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "bouncingball_stratum_band_speed_pct",
                                            "bouncingball_stratum_band_tight_pct",
                                            "bouncingball_stratum_band_phase_deg");
    if(settings.contains("ball_count")) ball_count = settings["ball_count"];

    StripColormapLoadJson(settings,
                          "bouncingball",
                          bouncingball_strip_cmap_on,
                          bouncingball_strip_cmap_kernel,
                          bouncingball_strip_cmap_rep,
                          bouncingball_strip_cmap_unfold,
                          bouncingball_strip_cmap_dir,
                          bouncingball_strip_cmap_color_style,
                          GetRainbowMode());
    if(strip_cmap_panel)
    {
        strip_cmap_panel->mirrorStateFromEffect(bouncingball_strip_cmap_on,
                                                bouncingball_strip_cmap_kernel,
                                                bouncingball_strip_cmap_rep,
                                                bouncingball_strip_cmap_unfold,
                                                bouncingball_strip_cmap_dir,
                                                bouncingball_strip_cmap_color_style);
    }

    if(count_slider) count_slider->setValue(ball_count);
    ball_last_integrated_wall_time = -1e9f;
    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}

REGISTER_EFFECT_3D(BouncingBall);
