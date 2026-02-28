// SPDX-License-Identifier: GPL-2.0-only

#include "Lightning3D.h"
#include "../EffectHelpers.h"
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <algorithm>
#include <cmath>

REGISTER_EFFECT_3D(Lightning3D);

Lightning3D::Lightning3D(QWidget* parent) : SpatialEffect3D(parent)
{
    strike_rate_slider = nullptr;
    strike_rate_label = nullptr;
    branch_slider = nullptr;
    branch_label = nullptr;
    strike_rate = 5;
    branches = 6;
    cache_time = -1e9f;
    cache_grid_hash = 0.0f;
    SetRainbowMode(false);
    std::vector<RGBColor> cols;
    cols.push_back(0x00FF64B4);
    SetColors(cols);
}

Lightning3D::~Lightning3D()
{
}

EffectInfo3D Lightning3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;
    info.effect_name = "Plasma Ball";
    info.effect_description = "Plasma ball in the center with lightning arcs to the glass (room boundary)";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_LIGHTNING;
    info.is_reversible = false;
    info.supports_random = true;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 1;
    info.has_custom_settings = true;
    info.needs_3d_origin = true;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = false;

    info.default_speed_scale = 1.0f;
    info.default_frequency_scale = 1.0f;
    info.use_size_parameter = true;

    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = false;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_color_controls = true;
    return info;
}

void Lightning3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(new QLabel("Arches/sec:"), 0, 0);
    strike_rate_slider = new QSlider(Qt::Horizontal);
    strike_rate_slider->setRange(1, 20);
    strike_rate_slider->setValue(strike_rate);
    strike_rate_slider->setToolTip("Number of new lightning arcs per second");
    layout->addWidget(strike_rate_slider, 0, 1);
    strike_rate_label = new QLabel(QString::number(strike_rate));
    strike_rate_label->setMinimumWidth(30);
    layout->addWidget(strike_rate_label, 0, 2);

    layout->addWidget(new QLabel("Max arcs:"), 1, 0);
    branch_slider = new QSlider(Qt::Horizontal);
    branch_slider->setRange(2, 12);
    branch_slider->setValue(branches);
    branch_slider->setToolTip("Maximum simultaneous arcs (lower = better FPS)");
    layout->addWidget(branch_slider, 1, 1);
    branch_label = new QLabel(QString::number(branches));
    branch_label->setMinimumWidth(30);
    layout->addWidget(branch_label, 1, 2);

    AddWidgetToParent(w, parent);

    connect(strike_rate_slider, &QSlider::valueChanged, this, &Lightning3D::OnLightningParameterChanged);
    connect(branch_slider, &QSlider::valueChanged, this, &Lightning3D::OnLightningParameterChanged);
}

void Lightning3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_LIGHTNING;
}

void Lightning3D::OnLightningParameterChanged()
{
    if(strike_rate_slider)
    {
        strike_rate = strike_rate_slider->value();
        if(strike_rate_label) strike_rate_label->setText(QString::number(strike_rate));
    }
    if(branch_slider)
    {
        branches = branch_slider->value();
        if(branch_label) branch_label->setText(QString::number(branches));
    }
    cache_time = -1e9f;
    emit ParametersChanged();
}

float Lightning3D::HashF(unsigned int seed)
{
    seed = (seed * 1103515245u + 12345u) & 0x7FFFFFFFu;
    return (float)seed / 2147483648.0f;
}

Vector3D Lightning3D::RandomPointOnGlass(const GridContext3D& grid, unsigned int seed)
{
    float cx = (grid.min_x + grid.max_x) * 0.5f;
    float cy = (grid.min_y + grid.max_y) * 0.5f;
    float cz = (grid.min_z + grid.max_z) * 0.5f;

    int face = (int)(HashF(seed) * 6.0f) % 6;
    float u = HashF(seed + 1);
    float v = HashF(seed + 2);

    Vector3D p;
    switch(face)
    {
    case 0: p.x = grid.min_x; p.y = cy + (u - 0.5f) * grid.height; p.z = cz + (v - 0.5f) * grid.depth; break;
    case 1: p.x = grid.max_x; p.y = cy + (u - 0.5f) * grid.height; p.z = cz + (v - 0.5f) * grid.depth; break;
    case 2: p.x = cx + (u - 0.5f) * grid.width; p.y = grid.min_y; p.z = cz + (v - 0.5f) * grid.depth; break;
    case 3: p.x = cx + (u - 0.5f) * grid.width; p.y = grid.max_y; p.z = cz + (v - 0.5f) * grid.depth; break;
    case 4: p.x = cx + (u - 0.5f) * grid.width; p.y = cy + (v - 0.5f) * grid.height; p.z = grid.min_z; break;
    default: p.x = cx + (u - 0.5f) * grid.width; p.y = cy + (v - 0.5f) * grid.height; p.z = grid.max_z; break;
    }
    return p;
}

float Lightning3D::DistToSegment(float px, float py, float pz,
                                 float ax, float ay, float az,
                                 float bx, float by, float bz)
{
    float dx = bx - ax, dy = by - ay, dz = bz - az;
    float len_sq = dx*dx + dy*dy + dz*dz;
    if(len_sq < 1e-12f) return sqrtf((px-ax)*(px-ax) + (py-ay)*(py-ay) + (pz-az)*(pz-az));

    float t = ((px-ax)*dx + (py-ay)*dy + (pz-az)*dz) / len_sq;
    t = std::max(0.0f, std::min(1.0f, t));
    float qx = ax + t * dx, qy = ay + t * dy, qz = az + t * dz;
    float ddx = px - qx, ddy = py - qy, ddz = pz - qz;
    return sqrtf(ddx*ddx + ddy*ddy + ddz*ddz);
}

void Lightning3D::UpdateArchCache(float time, const GridContext3D& grid)
{
    float grid_hash = grid.min_x + grid.max_x * 31.0f + grid.min_y * 31.0f*31.0f + grid.max_y * 31.0f*31.0f*31.0f;
    if(fabsf(time - cache_time) < 0.008f && fabsf(grid_hash - cache_grid_hash) < 0.01f)
        return;

    cache_time = time;
    cache_grid_hash = grid_hash;
    cached_arches.clear();

    Vector3D origin = GetEffectOriginGrid(grid);
    float speed_factor = 0.5f + 0.5f * GetScaledSpeed();
    float arch_duration = (0.35f + 0.25f / (float)std::max(1u, strike_rate)) / std::max(0.3f, speed_factor);
    int max_arches = (int)branches;

    for(int i = 0; i < max_arches; i++)
    {
        float phase = 0.1f + 0.85f * (float)i / (float)std::max(1, max_arches);
        float birth = time - arch_duration * phase;

        PlasmaArc3D arc;
        arc.start = origin;
        arc.end = RandomPointOnGlass(grid, (unsigned int)(i * 7919 + (int)(floorf(time * 5.0f))));
        arc.birth_time = birth;
        arc.duration = arch_duration;
        arc.seed = (unsigned int)(i * 7919 + (int)(time * 100.0f));
        cached_arches.push_back(arc);
    }
}

RGBColor Lightning3D::CalculateColor(float, float, float, float)
{
    return 0x00000000;
}

RGBColor Lightning3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rx = x - origin.x, ry = y - origin.y, rz = z - origin.z;
    float dist_from_center = sqrtf(rx*rx + ry*ry + rz*rz);

    float size_m = GetNormalizedSize();
    float room_avg = (grid.width + grid.depth + grid.height) / 3.0f;
    float core_radius = room_avg * (0.04f + 0.06f * size_m);
    float core_glow = room_avg * (0.08f + 0.10f * size_m);
    float arc_core = room_avg * (0.015f + 0.02f * size_m);
    float arc_glow = room_avg * (0.06f + 0.08f * size_m);

    float center_intensity = 0.0f;
    if(dist_from_center < core_glow * 2.0f)
    {
        float t = dist_from_center / (core_radius + 1e-6f);
        center_intensity = (t < 1.0f) ? (1.0f - t * 0.5f) : 0.0f;
        float glow_t = dist_from_center / (core_glow + 1e-6f);
        center_intensity += 0.6f * std::max(0.0f, 1.0f - glow_t);
        center_intensity = std::min(1.0f, center_intensity);
    }

    UpdateArchCache(time, grid);

    float arc_intensity = 0.0f;
    RGBColor arc_color = GetRainbowMode() ? GetRainbowColor(220.0f) : GetColorAtPosition(0.5f);

    for(size_t i = 0; i < cached_arches.size(); i++)
    {
        const PlasmaArc3D& arc = cached_arches[i];
        float age = time - arc.birth_time;
        float decay = std::max(0.0f, 1.0f - age / arc.duration);
        float flicker = 0.7f + 0.3f * sinf(age * 80.0f + (float)arc.seed);
        float a_int = decay * flicker;
        if(a_int < 0.02f) continue;

        float d = DistToSegment(rx + origin.x, ry + origin.y, rz + origin.z,
                               arc.start.x, arc.start.y, arc.start.z,
                               arc.end.x, arc.end.y, arc.end.z);

        float core = std::max(0.0f, 1.0f - d / (arc_core + 1e-6f));
        float glow = std::max(0.0f, 1.0f - d / (arc_glow + 1e-6f)) * 0.7f;
        float contrib = (core + glow) * a_int;
        if(contrib > arc_intensity)
        {
            arc_intensity = contrib;
            if(GetRainbowMode())
                arc_color = GetRainbowColor(220.0f + age * 100.0f + (float)arc.seed * 0.1f);
        }
    }

    float total = std::min(1.0f, center_intensity + arc_intensity);
    total *= GetNormalizedScale();

    int r = (int)((arc_color & 0xFF) * total);
    int g = (int)(((arc_color >> 8) & 0xFF) * total);
    int b = (int)(((arc_color >> 16) & 0xFF) * total);
    r = std::min(255, std::max(0, r));
    g = std::min(255, std::max(0, g));
    b = std::min(255, std::max(0, b));
    return (RGBColor)((b << 16) | (g << 8) | r);
}

nlohmann::json Lightning3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["strike_rate"] = strike_rate;
    j["branches"] = branches;
    return j;
}

void Lightning3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("strike_rate") && settings["strike_rate"].is_number_integer())
        strike_rate = std::max(1u, std::min(20u, (unsigned int)settings["strike_rate"].get<int>()));
    if(settings.contains("branches") && settings["branches"].is_number_integer())
        branches = std::max(2u, std::min(12u, (unsigned int)settings["branches"].get<int>()));

    if(strike_rate_slider) { strike_rate_slider->setValue((int)strike_rate); }
    if(branch_slider) { branch_slider->setValue((int)branches); }
    cache_time = -1e9f;
}
