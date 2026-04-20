// SPDX-License-Identifier: GPL-2.0-only

#include "BouncingBall.h"
#include <QGridLayout>
#include <cmath>
#include <vector>

REGISTER_EFFECT_3D(BouncingBall);

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
    info.info_version = 2;
    info.effect_name = "Bouncing Ball";
    info.effect_description = "Independent balls bouncing in the room (no ball–ball physics). Physics runs forward without looping; use Speed for motion rate.";
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
    QGridLayout* layout = new QGridLayout(w);
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

    AddWidgetToParent(w, parent);

    connect(count_slider, &QSlider::valueChanged, this, &BouncingBall::OnBallParameterChanged);
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
    /* Linear 0..1 from main effect Speed slider (not squared GetNormalizedSpeed) so the control is obvious. */
    const float speed_lin = fmaxf(0.02f, fminf(1.0f, GetSpeed() / 200.0f));
    const float motion = speed_lin * speed_lin * 0.28f + speed_lin * 0.72f;
    constexpr float k_wall_e = 0.987f;

    const float size_m = GetNormalizedSize();
    const float color_cycle = time * GetScaledFrequency() * 12.0f;
    const float detail = std::max(0.05f, GetScaledDetail());
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

    const float glow_radius = R * 2.0f;
    const float glow_radius_sq = glow_radius * glow_radius;
    const float core_radius = R * 0.8f;

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
            hue_for_max = fmodf(color_cycle + hue_ball + hue_spatial + hue_vel, 360.0f);
            if(hue_for_max < 0.0f) hue_for_max += 360.0f;
        }
    }

    RGBColor final_color;
    if(GetRainbowMode())
    {
        final_color = GetRainbowColor(hue_for_max);
    }
    else
    {
        const float pos = fmodf(0.28f + hue_for_max * (1.0f / 360.0f), 1.0f);
        final_color = GetColorAtPosition(pos < 0.0f ? pos + 1.0f : pos);
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
    j["ball_count"] = ball_count;
    return j;
}

void BouncingBall::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("ball_count")) ball_count = settings["ball_count"];

    if(count_slider) count_slider->setValue(ball_count);
}
