// SPDX-License-Identifier: GPL-2.0-only

#include "Tornado3D.h"
#include <QGridLayout>
#include <cmath>

REGISTER_EFFECT_3D(Tornado3D);

Tornado3D::Tornado3D(QWidget* parent) : SpatialEffect3D(parent)
{
    core_radius_slider = nullptr;
    core_radius_label = nullptr;
    height_slider = nullptr;
    height_label = nullptr;
    core_radius = 80;
    tornado_height = 250;
    SetRainbowMode(true);
    SetFrequency(50);
}

Tornado3D::~Tornado3D()
{
}

EffectInfo3D Tornado3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;
    info.effect_name = "3D Tornado";
    info.effect_description = "Spinning vortex around the origin";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_TORNADO;
    info.is_reversible = true;
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

    info.default_speed_scale = 25.0f;
    info.default_frequency_scale = 6.0f;
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

void Tornado3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0,0,0,0);

    layout->addWidget(new QLabel("Core Radius:"), 0, 0);
    core_radius_slider = new QSlider(Qt::Horizontal);
    core_radius_slider->setRange(20, 300);
    core_radius_slider->setValue(core_radius);
    core_radius_slider->setToolTip("Tornado core radius (affects base funnel size)");
    layout->addWidget(core_radius_slider, 0, 1);
    core_radius_label = new QLabel(QString::number(core_radius));
    core_radius_label->setMinimumWidth(30);
    layout->addWidget(core_radius_label, 0, 2);

    layout->addWidget(new QLabel("Height:"), 1, 0);
    height_slider = new QSlider(Qt::Horizontal);
    height_slider->setRange(50, 500);
    height_slider->setValue(tornado_height);
    height_slider->setToolTip("Tornado height (relative to room height)");
    layout->addWidget(height_slider, 1, 1);
    height_label = new QLabel(QString::number(tornado_height));
    height_label->setMinimumWidth(30);
    layout->addWidget(height_label, 1, 2);

    AddWidgetToParent(w, parent);

    connect(core_radius_slider, &QSlider::valueChanged, this, &Tornado3D::OnTornadoParameterChanged);
    connect(core_radius_slider, &QSlider::valueChanged, core_radius_label, [this](int value) {
        core_radius_label->setText(QString::number(value));
    });
    connect(height_slider, &QSlider::valueChanged, this, &Tornado3D::OnTornadoParameterChanged);
    connect(height_slider, &QSlider::valueChanged, height_label, [this](int value) {
        height_label->setText(QString::number(value));
    });
}

void Tornado3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_TORNADO;
}

void Tornado3D::OnTornadoParameterChanged()
{
    if(core_radius_slider)
    {
        core_radius = core_radius_slider->value();
        if(core_radius_label) core_radius_label->setText(QString::number(core_radius));
    }
    if(height_slider)
    {
        tornado_height = height_slider->value();
        if(height_label) height_label->setText(QString::number(tornado_height));
    }
    emit ParametersChanged();
}

RGBColor Tornado3D::CalculateColor(float, float, float, float)
{
    return 0x00000000;
}

RGBColor Tornado3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);

    float speed = GetScaledSpeed();
    float freq = GetScaledFrequency();
    float size_m = GetNormalizedSize();
    float progress = CalculateProgress(time);

    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float rot_rel_x = rotated_pos.x - origin.x;
    float rot_rel_y = rotated_pos.y - origin.y;
    float rot_rel_z = rotated_pos.z - origin.z;

    float axial = 0.0f;
    if(grid.height > 0.001f)
    {
        axial = (rot_rel_y + grid.height * 0.5f) / grid.height;
    }
    axial = fmaxf(0.0f, fminf(1.0f, axial));
    
    float height_center = 0.5f;
    float height_range_val = (tornado_height / 500.0f) * 0.5f;
    float h_norm = fmax(0.0f, fmin(1.0f, (axial - (height_center - height_range_val)) / (2.0f * height_range_val + 0.0001f)));
    float base_radius = 0.5f * fmin(grid.width, grid.depth);
    float core_scale = 0.04f + (core_radius / 300.0f) * 0.56f;
    float funnel_radius = (base_radius * core_scale) * (0.6f + 0.4f * h_norm) * size_m;

    float a = atan2f(rot_rel_z, rot_rel_x);
    float rad = sqrtf(rot_rel_x*rot_rel_x + rot_rel_z*rot_rel_z);
    float along = rot_rel_y;
    float swirl = a + along * (0.015f * freq) - time * speed * 0.25f;

    float ring = fabsf(rad - funnel_radius);
    float thickness_base = (grid.width + grid.depth) * 0.01f;
    float ring_thickness = thickness_base * (0.6f + 1.2f * size_m);
    float ring_intensity = fmax(0.0f, 1.0f - ring / ring_thickness);

    float arms = 4.0f + 4.0f * size_m;
    float band = 0.5f * (1.0f + cosf(swirl * arms));
    float band2 = 0.2f * (1.0f + cosf(swirl * arms * 2.0f + progress));
    band = fmin(1.0f, band + band2);

    float y_fade = fmax(0.0f, 1.0f - fabsf(axial - 0.5f) / (height_range_val + 0.001f));
    
    float radial_glow = 0.15f * (1.0f - fmin(1.0f, rad / (funnel_radius * 3.0f)));

    float intensity = ring_intensity * (0.5f + 0.5f * band) * y_fade + radial_glow;
    intensity = fmax(0.0f, fmin(1.0f, intensity));

    RGBColor final_color;
    if(GetRainbowMode())
    {
        float hue = 200.0f + swirl * 57.2958f * 0.2f + h_norm * 80.0f;
        final_color = GetRainbowColor(hue);
    }
    else
    {
        final_color = GetColorAtPosition(0.5f + 0.5f * intensity);
    }

    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    r = (unsigned char)(r * intensity);
    g = (unsigned char)(g * intensity);
    b = (unsigned char)(b * intensity);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json Tornado3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["core_radius"] = core_radius;
    j["tornado_height"] = tornado_height;
    return j;
}

void Tornado3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("core_radius")) core_radius = settings["core_radius"];
    if(settings.contains("tornado_height")) tornado_height = settings["tornado_height"];

    if(core_radius_slider) core_radius_slider->setValue(core_radius);
    if(height_slider) height_slider->setValue(tornado_height);
}
