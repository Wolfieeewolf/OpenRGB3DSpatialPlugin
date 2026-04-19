// SPDX-License-Identifier: GPL-2.0-only

#include "BouncingBall.h"
#include <QGridLayout>
#include <cmath>
#include <utility>
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
        vel_y = -vel_y * e;
        if(fabsf(vel_y) < 0.025f && pos_y <= ymin + 0.02f)
        {
            vel_y = 0.35f + 0.55f * e;
        }
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

void ResolveBallPair(float& pxi, float& pyi, float& pzi, float& vxi, float& vyi, float& vzi,
                     float& pxj, float& pyj, float& pzj, float& vxj, float& vyj, float& vzj,
                     float collision_r, float e_pair)
{
    float dx = pxj - pxi;
    float dy = pyj - pyi;
    float dz = pzj - pzi;
    float dist_sq = dx * dx + dy * dy + dz * dz;
    const float min_d = 2.0f * collision_r * 1.004f;

    if(dist_sq < 1e-12f)
    {
        const float j = 0.001f * collision_r;
        pxi -= j;
        pxj += j;
        dx = pxj - pxi;
        dy = pyj - pyi;
        dz = pzj - pzi;
        dist_sq = dx * dx + dy * dy + dz * dz;
    }

    float dist = sqrtf(dist_sq);
    if(dist < 1e-6f) return;

    float nx = dx / dist;
    float ny = dy / dist;
    float nz = dz / dist;

    if(dist < min_d)
    {
        float pen = 0.5f * (min_d - dist);
        pxi -= nx * pen;
        pyi -= ny * pen;
        pzi -= nz * pen;
        pxj += nx * pen;
        pyj += ny * pen;
        pzj += nz * pen;
    }

    float vn = (vxj - vxi) * nx + (vyj - vyi) * ny + (vzj - vzi) * nz;
    if(vn >= -1e-5f) return;

    float jimp = -(1.0f + e_pair) * vn * 0.5f;
    vxi -= jimp * nx;
    vyi -= jimp * ny;
    vzi -= jimp * nz;
    vxj += jimp * nx;
    vyj += jimp * ny;
    vzj += jimp * nz;
}

void ResolveAllPairs(std::vector<float>& px, std::vector<float>& py, std::vector<float>& pz,
                     std::vector<float>& vx, std::vector<float>& vy, std::vector<float>& vz,
                     unsigned int N, float collision_r, float e_pair, int passes)
{
    for(int pass = 0; pass < passes; pass++)
    {
        for(unsigned int i = 0; i < N; i++)
        {
            for(unsigned int j = i + 1; j < N; j++)
            {
                ResolveBallPair(px[i], py[i], pz[i], vx[i], vy[i], vz[i],
                                px[j], py[j], pz[j], vx[j], vy[j], vz[j],
                                collision_r, e_pair);
            }
        }
    }
}

float WallRestitutionFromElasticity(unsigned int elasticity_ui)
{
    float t = (float(elasticity_ui) - 10.0f) / 90.0f;
    t = fmaxf(0.0f, fminf(1.0f, t));
    return 0.30f + t * t * 0.95f;
}

float PairRestitutionFromElasticity(unsigned int elasticity_ui)
{
    float t = (float(elasticity_ui) - 10.0f) / 90.0f;
    t = fmaxf(0.0f, fminf(1.0f, t));
    return 0.52f + t * t * 0.46f;
}

} // namespace

BouncingBall::BouncingBall(QWidget* parent) : SpatialEffect3D(parent)
{
    elasticity_slider = nullptr;
    elasticity_label = nullptr;
    count_slider = nullptr;
    count_label = nullptr;
    elasticity = 70;
    ball_count = 1;
    SetRainbowMode(true);
}

BouncingBall::~BouncingBall() = default;

EffectInfo3D BouncingBall::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;
    info.effect_name = "Bouncing Ball";
    info.effect_description = "One or more balls with elastic walls, ball–ball collisions, and glow; max Speed and Elasticity are tuned for strong ceiling-height bounces";
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
    info.show_fps_control = true;
    info.show_color_controls = true;
    return info;
}

void BouncingBall::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(new QLabel("Elasticity:"), 0, 0);
    elasticity_slider = new QSlider(Qt::Horizontal);
    elasticity_slider->setRange(10, 100);
    elasticity_slider->setValue(elasticity);
    elasticity_slider->setToolTip(
        "Wall and ball–ball bounciness. Low = damped; high approaches super-ball (very lively at maximum).");
    layout->addWidget(elasticity_slider, 0, 1);
    elasticity_label = new QLabel(QString::number(elasticity));
    elasticity_label->setMinimumWidth(30);
    layout->addWidget(elasticity_label, 0, 2);

    layout->addWidget(new QLabel("Balls:"), 1, 0);
    count_slider = new QSlider(Qt::Horizontal);
    count_slider->setRange(1, 50);
    count_slider->setValue(ball_count);
    count_slider->setToolTip("Number of balls (1..50). Balls collide and push each other.");
    layout->addWidget(count_slider, 1, 1);
    count_label = new QLabel(QString::number(ball_count));
    count_label->setMinimumWidth(30);
    layout->addWidget(count_label, 1, 2);

    AddWidgetToParent(w, parent);

    connect(elasticity_slider, &QSlider::valueChanged, this, &BouncingBall::OnBallParameterChanged);
    connect(count_slider, &QSlider::valueChanged, this, &BouncingBall::OnBallParameterChanged);
}

void BouncingBall::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_BOUNCING_BALL;
}

void BouncingBall::OnBallParameterChanged()
{
    if(elasticity_slider)
    {
        elasticity = elasticity_slider->value();
        if(elasticity_label) elasticity_label->setText(QString::number(elasticity));
    }
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
    const float speed = GetScaledSpeed();
    const float e_wall = WallRestitutionFromElasticity(elasticity);
    const float e_pair = PairRestitutionFromElasticity(elasticity);

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

    const float speed_boost = 0.22f + fminf(3.8f, speed) * 0.34f;
    const float elast_t = fmaxf(0.0f, fminf(1.0f, (float(elasticity) - 10.0f) / 90.0f));
    const float gravity = radius_basis * (0.14f + 0.06f * speed_boost) * (1.05f - 0.28f * elast_t);

    const unsigned int N = ball_count == 0 ? 1u : ball_count;

    const float grid_hash = grid.min_x + grid.max_x * 31.0f + grid.min_y * 31.0f * 31.0f + grid.max_y * 31.0f * 31.0f * 31.0f;
    const float phys_tag = float(elasticity) * 0.17f + float(ball_count) * 2.31f + size_m * 7.9f + speed * 1.03f + R * 0.02f;

    if(ball_positions_cached.size() != N || fabsf(time - ball_cache_time) > 0.001f || fabsf(grid_hash - ball_cache_grid_hash) > 0.01f
       || fabsf(phys_tag - ball_cache_phys_tag) > 1e-4f)
    {
        ball_cache_time = time;
        ball_cache_grid_hash = grid_hash;
        ball_cache_phys_tag = phys_tag;
        ball_positions_cached.resize(N);

        std::vector<float> px(N), py(N), pz(N), vx(N), vy(N), vz(N);

        const float sim_horizon = 28.0f;
        const float wrapped_time = fmodf(time, sim_horizon);
        const float dt = 0.032f;
        int max_steps = (int)ceilf(wrapped_time / dt) + 2;
        if(max_steps > 880) max_steps = 880;

        for(unsigned int k = 0; k < N; k++)
        {
            const float hy = HashFloat01(k * 313U + 5U);
            const float hx = HashFloat01(k * 131U);
            const float hz = HashFloat01(k * 919U);

            px[k] = xmin + hx * span_x;
            py[k] = ymin + (0.08f + hy * 0.88f) * span_y;
            pz[k] = zmin + hz * span_z;

            const float horiz = (0.28f + speed_boost * 0.55f) * radius_basis;
            vx[k] = (HashFloat01(k * 733U) * 2.0f - 1.0f) * horiz;
            vz[k] = (HashFloat01(k * 829U) * 2.0f - 1.0f) * horiz;
            const float vy_up = (0.35f + HashFloat01(k * 577U) * 0.65f) * radius_basis * (0.55f + speed_boost * 0.95f);
            vy[k] = vy_up;
        }

        float sim_time = 0.0f;
        for(int step = 0; step < max_steps && sim_time < wrapped_time; step++)
        {
            const float step_dt = fminf(dt, wrapped_time - sim_time);

            for(unsigned int k = 0; k < N; k++)
            {
                IntegrateBall(px[k], py[k], pz[k], vx[k], vy[k], vz[k], step_dt, gravity, e_wall, xmin, xmax, ymin, ymax, zmin, zmax);
            }

            if(N > 1u)
            {
                ResolveAllPairs(px, py, pz, vx, vy, vz, N, R, e_pair, 5);
            }

            sim_time += step_dt;
        }

        for(unsigned int k = 0; k < N; k++)
        {
            ball_positions_cached[k].px = px[k];
            ball_positions_cached[k].py = py[k];
            ball_positions_cached[k].pz = pz[k];
            ball_positions_cached[k].vx = vx[k];
            ball_positions_cached[k].vy = vy[k];
            ball_positions_cached[k].vz = vz[k];
        }
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
            const float hue_ball = (float(k) * 41.7f);
            float hue = fmodf((atan2f(ball.vz, ball.vx) * 57.2958f) * (0.6f + 0.4f * detail) + color_cycle + hue_ball, 360.0f);
            hue_for_max = hue;
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
    j["elasticity"] = elasticity;
    j["ball_count"] = ball_count;
    return j;
}

void BouncingBall::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("elasticity")) elasticity = settings["elasticity"];
    if(settings.contains("ball_count")) ball_count = settings["ball_count"];

    if(elasticity_slider) elasticity_slider->setValue(elasticity);
    if(count_slider) count_slider->setValue(ball_count);
}
