// SPDX-License-Identifier: GPL-2.0-only

#include "Bouncer3D.h"

#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

REGISTER_EFFECT_3D(Bouncer3D);

namespace
{
float Hash01(unsigned int seed)
{
    unsigned int v = seed ^ 0xA3C59AC3u;
    v ^= (v >> 17);
    v *= 0xED5AD4BBu;
    v ^= (v >> 11);
    v *= 0xAC4C1B51u;
    v ^= (v >> 15);
    v *= 0x31848BABu;
    v ^= (v >> 14);
    return (float)(v & 0x00FFFFFFu) / 16777215.0f;
}

RGBColor Hsv01ToBgr(float h, float s, float v)
{
    h = std::fmod(h, 1.0f);
    if(h < 0.0f)
    {
        h += 1.0f;
    }
    s = std::clamp(s, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);

    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;

    const float hf = h * 6.0f;
    const int i = (int)std::floor(hf) % 6;
    const float f = hf - std::floor(hf);
    const float p = v * (1.0f - s);
    const float q = v * (1.0f - f * s);
    const float t = v * (1.0f - (1.0f - f) * s);

    switch(i)
    {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }

    const int ri = std::clamp((int)std::lround(r * 255.0f), 0, 255);
    const int gi = std::clamp((int)std::lround(g * 255.0f), 0, 255);
    const int bi = std::clamp((int)std::lround(b * 255.0f), 0, 255);
    return (RGBColor)((bi << 16) | (gi << 8) | ri);
}
} // namespace

Bouncer3D::Bouncer3D(QWidget* parent) : SpatialEffect3D(parent)
{
    SetRainbowMode(false);
    SetSpeed(50);
    SetFrequency(50);
}

Bouncer3D::~Bouncer3D() = default;

EffectInfo3D Bouncer3D::GetEffectInfo()
{
    EffectInfo3D info{};
    info.info_version = 1;
    info.effect_name = "Orbit Bounce 3D";
    info.effect_description = "Up to 20 hue-coded orbs bounce in a normalized room volume.";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_BOUNCER_3D;
    info.is_reversible = false;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_frequency = false;
    info.default_speed_scale = 60.0f;
    info.use_size_parameter = true;
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_color_controls = false;
    return info;
}

void Bouncer3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QVBoxLayout* outer = new QVBoxLayout(w);
    outer->setContentsMargins(0, 0, 0, 0);
    QGridLayout* grid = new QGridLayout();
    grid->setContentsMargins(0, 0, 0, 0);
    outer->addLayout(grid);

    grid->addWidget(new QLabel("Balls:"), 0, 0);
    ball_count_slider = new QSlider(Qt::Horizontal);
    ball_count_slider->setRange(1, 20);
    ball_count_slider->setValue((int)ball_count);
    ball_count_slider->setToolTip("Number of bouncing orbs.");
    grid->addWidget(ball_count_slider, 0, 1);

    ball_count_label = new QLabel(QString::number(ball_count));
    ball_count_label->setMinimumWidth(28);
    grid->addWidget(ball_count_label, 0, 2);

    connect(ball_count_slider, &QSlider::valueChanged, this, &Bouncer3D::OnBallCountChanged);
    AddWidgetToParent(w, parent);
}

void Bouncer3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_BOUNCER_3D;
}

void Bouncer3D::OnBallCountChanged()
{
    if(ball_count_slider)
    {
        ball_count = (unsigned int)std::clamp(ball_count_slider->value(), 1, 20);
        if(ball_count_label)
        {
            ball_count_label->setText(QString::number(ball_count));
        }
        needs_reset_ = true;
        emit ParametersChanged();
    }
}

void Bouncer3D::EnsureBallStates(unsigned int count)
{
    if(balls_.size() == count && !needs_reset_)
    {
        return;
    }
    balls_.assign(count, BallState{});
    ResetBallStates();
    needs_reset_ = false;
}

void Bouncer3D::ResetBallStates()
{
    const float speed = std::clamp(GetSpeed() / 100.0f, 0.0f, 1.0f) * 0.15f;
    for(unsigned int i = 0; i < balls_.size(); i++)
    {
        BallState& b = balls_[i];
        b.x = Hash01(1000u + i * 17u);
        b.y = Hash01(2000u + i * 31u);
        b.z = Hash01(3000u + i * 47u);
        b.vx = std::max(0.0005f, Hash01(4000u + i * 13u) * speed);
        b.vy = std::max(0.0005f, Hash01(5000u + i * 23u) * speed);
        b.vz = std::max(0.0005f, Hash01(6000u + i * 37u) * speed);
        b.hue01 = Hash01(7000u + i * 53u);
    }
}

void Bouncer3D::StepSimulation(float dt_seconds)
{
    if(dt_seconds <= 0.0f)
    {
        return;
    }
    // Original source updates per frame. We approximate frame steps at 60 Hz.
    const float step_dt = 1.0f / 60.0f;
    int steps = (int)std::ceil(dt_seconds / step_dt);
    steps = std::clamp(steps, 1, 200);

    for(int s = 0; s < steps; s++)
    {
        for(unsigned int i = 0; i < balls_.size(); i++)
        {
            BallState& b = balls_[i];
            b.x += b.vx;
            b.y += b.vy;
            b.z += b.vz;

            // Keep the original bounce order: bounce and continue to next orb.
            if(b.x < 0.0f) { b.x = 0.0f; b.vx = -b.vx; continue; }
            if(b.y < 0.0f) { b.y = 0.0f; b.vy = -b.vy; continue; }
            if(b.z < 0.0f) { b.z = 0.0f; b.vz = -b.vz; continue; }
            if(b.x > 1.0f) { b.x = 1.0f; b.vx = -b.vx; continue; }
            if(b.y > 1.0f) { b.y = 1.0f; b.vy = -b.vy; continue; }
            if(b.z > 1.0f) { b.z = 1.0f; b.vz = -b.vz; continue; }
        }
    }
}

RGBColor Bouncer3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
    {
        return 0x00000000;
    }

    EnsureBallStates(std::max(1u, ball_count));

    if(last_time_ < -1e8f || time + 1e-4f < last_time_)
    {
        ResetBallStates();
        last_time_ = time;
    }

    const float dt = std::clamp(time - last_time_, 0.0f, 1.5f);
    StepSimulation(dt);
    last_time_ = time;

    // Normalize to 0..1 room coordinates.
    const float nx = NormalizeGridAxis01(x, grid.min_x, grid.max_x);
    const float ny = NormalizeGridAxis01(y, grid.min_y, grid.max_y);
    const float nz = NormalizeGridAxis01(z, grid.min_z, grid.max_z);

    const float ball_size = 0.2f * std::clamp(GetNormalizedSize(), 0.0f, 1.0f);
    const float ball_size_3d = std::max(1e-4f, ball_size * 4.0f);

    float out_h = 0.0f;
    float out_s = 1.0f;
    float out_v = 0.0f;

    for(unsigned int i = 0; i < balls_.size(); i++)
    {
        const BallState& b = balls_[i];
        const float dx = std::fabs(b.x - nx);
        if(dx > ball_size_3d) continue;
        const float dy = std::fabs(b.y - ny);
        if(dy > ball_size_3d) continue;
        const float dz = std::fabs(b.z - nz);
        if(dz > ball_size_3d) continue;

        float v = (dx + dy + dz) / ball_size_3d;
        v = v * v;
        float s = std::clamp(v * 4.0f, 0.0f, 1.0f);
        v = std::clamp(1.0f - v, 0.0f, 1.0f);

        out_h = b.hue01;
        out_s = s;
        out_v = v;
        break;
    }

    // Respect common brightness control.
    out_v *= std::clamp(GetBrightness() / 100.0f, 0.0f, 1.0f);
    return Hsv01ToBgr(out_h, out_s, out_v);
}

nlohmann::json Bouncer3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["bouncer3d_ball_count"] = ball_count;
    return j;
}

void Bouncer3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("bouncer3d_ball_count") && settings["bouncer3d_ball_count"].is_number_integer())
    {
        ball_count = (unsigned int)std::clamp(settings["bouncer3d_ball_count"].get<int>(), 1, 20);
    }
    if(ball_count_slider)
    {
        ball_count_slider->setValue((int)ball_count);
    }
    needs_reset_ = true;
    last_time_ = -1e9f;
}
