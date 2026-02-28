// SPDX-License-Identifier: GPL-2.0-only

#include "Explosion3D.h"

REGISTER_EFFECT_3D(Explosion3D);

#include <QGridLayout>
#include "../EffectHelpers.h"

Explosion3D::Explosion3D(QWidget* parent) : SpatialEffect3D(parent)
{
    intensity_slider = nullptr;
    intensity_label = nullptr;
    type_combo = nullptr;
    explosion_intensity = 75;
    progress = 0.0f;
    explosion_type = 0;

    SetFrequency(50);
    SetRainbowMode(true);

    std::vector<RGBColor> default_colors;
    default_colors.push_back(0x000000FF);
    default_colors.push_back(0x0000FFFF);
    default_colors.push_back(0x00FF0000);
    SetColors(default_colors);
}

EffectInfo3D Explosion3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;
    info.effect_name = "3D Explosion";
    info.effect_description = "Expanding shockwave from origin";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_EXPLOSION;
    info.is_reversible = false;
    info.supports_random = true;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = true;

    info.default_speed_scale = 35.0f;
    info.default_frequency_scale = 60.0f;
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

void Explosion3D::SetupCustomUI(QWidget* parent)
{
    QWidget* explosion_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(explosion_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(new QLabel("Intensity:"), 0, 0);
    intensity_slider = new QSlider(Qt::Horizontal);
    intensity_slider->setRange(10, 200);
    intensity_slider->setValue(explosion_intensity);
    intensity_slider->setToolTip("Explosion energy (affects radius and wave thickness)");
    layout->addWidget(intensity_slider, 0, 1);
    intensity_label = new QLabel(QString::number(explosion_intensity));
    intensity_label->setMinimumWidth(30);
    layout->addWidget(intensity_label, 0, 2);

    layout->addWidget(new QLabel("Type:"), 1, 0);
    type_combo = new QComboBox();
    type_combo->setToolTip("Explosion type behavior");
    type_combo->addItem("Standard");
    type_combo->addItem("Nuke");
    type_combo->addItem("Land Mine");
    type_combo->addItem("Bomb");
    type_combo->addItem("Wall Bounce");
    type_combo->setCurrentIndex(explosion_type);
    layout->addWidget(type_combo, 1, 1);

    AddWidgetToParent(explosion_widget, parent);

    connect(intensity_slider, &QSlider::valueChanged, this, &Explosion3D::OnExplosionParameterChanged);
    connect(intensity_slider, &QSlider::valueChanged, intensity_label, [this](int value) {
        intensity_label->setText(QString::number(value));
    });
    connect(type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Explosion3D::OnExplosionParameterChanged);
}

void Explosion3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_EXPLOSION;
}

void Explosion3D::OnExplosionParameterChanged()
{
    if(intensity_slider)
    {
        explosion_intensity = intensity_slider->value();
        if(intensity_label) intensity_label->setText(QString::number(explosion_intensity));
    }
    if(type_combo) explosion_type = type_combo->currentIndex();
    emit ParametersChanged();
}

RGBColor Explosion3D::CalculateColor(float, float, float, float)
{
    return 0x00000000;
}

nlohmann::json Explosion3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["explosion_intensity"] = explosion_intensity;
    j["explosion_type"] = explosion_type;
    return j;
}

void Explosion3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("explosion_intensity")) explosion_intensity = settings["explosion_intensity"];
    if(settings.contains("explosion_type")) explosion_type = settings["explosion_type"];

    if(intensity_slider) intensity_slider->setValue(explosion_intensity);
    if(type_combo) type_combo->setCurrentIndex(explosion_type);
}

RGBColor Explosion3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);

    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
    {
        return 0x00000000;
    }

    float actual_frequency = GetScaledFrequency();
    progress = CalculateProgress(time);

    float size_multiplier = GetNormalizedSize();
    float freq_scale = actual_frequency * 0.01f / size_multiplier;

    /*---------------------------------------------------------*\
    | Apply rotation transformation to LED position            |
    | This rotates the effect pattern around the origin       |
    \*---------------------------------------------------------*/
    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float rot_rel_x = rotated_pos.x - origin.x;
    float rot_rel_y = rotated_pos.y - origin.y;
    float rot_rel_z = rotated_pos.z - origin.z;

    float distance = sqrtf(rot_rel_x*rot_rel_x + rot_rel_y*rot_rel_y + rot_rel_z*rot_rel_z);

    if(explosion_type == 2)
    {
        float vz = rot_rel_z * 0.35f;
        distance = sqrtf(rot_rel_x*rot_rel_x + rot_rel_y*rot_rel_y + vz*vz);
    }

    float explosion_radius = progress * (explosion_intensity * 0.25f) * size_multiplier;
    float wave_thickness = (8.0f + explosion_intensity * 0.08f) * size_multiplier;
    if(explosion_type == 1)
    {
        explosion_radius *= 1.8f;
        wave_thickness   *= 1.5f;
    }
    if(explosion_type == 4)
    {
        float max_extent = sqrtf(grid.width*grid.width + grid.depth*grid.depth + grid.height*grid.height) * 0.5f;
        float travel = explosion_radius;
        float period = fmax(0.1f, max_extent);
        float t = fmodf(travel, 2.0f * period);
        explosion_radius = (t <= period) ? t : (2.0f * period - t);
    }

    float primary_wave = 1.0f - smoothstep(explosion_radius - wave_thickness, explosion_radius + wave_thickness, distance);
    primary_wave *= exp(-fabs(distance - explosion_radius) * 0.08f);

    float secondary_radius = explosion_radius * 0.7f;
    float secondary_wave = 1.0f - smoothstep(secondary_radius - wave_thickness * 0.5f, secondary_radius + wave_thickness * 0.5f, distance);
    secondary_wave *= exp(-fabs(distance - secondary_radius) * 0.12f) * 0.7f;

    float shock_detail = 0.25f * sinf(distance * freq_scale * 8.0f - progress * 4.0f);
    float shock_detail2 = 0.15f * sinf(distance * freq_scale * 12.0f - progress * 6.0f);
    if(explosion_type == 3)
    {
        float ang = atan2f(rot_rel_y, rot_rel_x);
        float lobe_factor = 0.6f + 0.4f * fabsf(cosf(ang * 4.0f));
        shock_detail *= lobe_factor;
        shock_detail2 *= lobe_factor;
    }
    shock_detail = (shock_detail + shock_detail2) * expf(-distance * 0.08f);

    float explosion_intensity_final = primary_wave + secondary_wave + shock_detail;
    explosion_intensity_final = fmax(0.0f, fmin(1.0f, explosion_intensity_final));

    if(distance < explosion_radius * 0.3f)
    {
        float core_intensity = 1.0f - (distance / (explosion_radius * 0.3f));
        float core_glow = 0.4f * core_intensity;
        explosion_intensity_final = fmax(explosion_intensity_final, core_intensity * 0.85f + core_glow);
    }
    
    float ambient = 0.1f * (1.0f - fmin(1.0f, distance / (explosion_radius * 2.0f)));
    explosion_intensity_final = fmin(1.0f, explosion_intensity_final + ambient);

    float hue_base = (explosion_type == 1 ? 30.0f : 60.0f);
    RGBColor final_color = GetRainbowMode()
        ? GetRainbowColor(fmax(0.0f, hue_base - (explosion_intensity_final * 60.0f) + progress * 10.0f))
        : GetColorAtPosition(explosion_intensity_final);

    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;

    r = (unsigned char)(r * explosion_intensity_final);
    g = (unsigned char)(g * explosion_intensity_final);
    b = (unsigned char)(b * explosion_intensity_final);

    return (b << 16) | (g << 8) | r;
}
