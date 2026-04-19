// SPDX-License-Identifier: GPL-2.0-only

#include "DNAHelix.h"

REGISTER_EFFECT_3D(DNAHelix);

#include <QGridLayout>
#include "../EffectHelpers.h"

DNAHelix::DNAHelix(QWidget* parent) : SpatialEffect3D(parent)
{
    radius_slider = nullptr;
    radius_label = nullptr;
    helix_radius = 180;
    progress = 0.0f;

    std::vector<RGBColor> dna_colors = {
        0x000000FF,
        0x0000FFFF,
        0x0000FF00,
        0x00FF0000
    };
    if(GetColors().empty())
    {
        SetColors(dna_colors);
    }
    SetFrequency(50);
    SetRainbowMode(false);
}

DNAHelix::~DNAHelix() = default;

EffectInfo3D DNAHelix::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;
    info.effect_name = "DNA Helix";
    info.effect_description = "Double helix with base pairs and rainbow colors";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_DNA_HELIX;
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

    info.default_speed_scale = 10.0f;
    info.default_frequency_scale = 100.0f;
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

void DNAHelix::SetupCustomUI(QWidget* parent)
{
    QWidget* dna_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(dna_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(new QLabel("Helix Radius:"), 0, 0);
    radius_slider = new QSlider(Qt::Horizontal);
    radius_slider->setRange(20, 150);
    radius_slider->setToolTip("How far strands sit from the helix axis (pairs with Scale and room bounds).");
    radius_slider->setValue(helix_radius);
    layout->addWidget(radius_slider, 0, 1);
    radius_label = new QLabel(QString::number(helix_radius));
    radius_label->setMinimumWidth(30);
    layout->addWidget(radius_label, 0, 2);

    AddWidgetToParent(dna_widget, parent);

    connect(radius_slider, &QSlider::valueChanged, this, &DNAHelix::OnDNAParameterChanged);
}

void DNAHelix::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_DNA_HELIX;
}

void DNAHelix::OnDNAParameterChanged()
{
    if(radius_slider)
    {
        helix_radius = radius_slider->value();
        if(radius_label) radius_label->setText(QString::number(helix_radius));
    }
    emit ParametersChanged();
}


RGBColor DNAHelix::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
    {
        return 0x00000000;
    }

    float rate = GetScaledFrequency();
    float detail = std::max(0.05f, GetScaledDetail());
    progress = CalculateProgress(time);

    float size_multiplier = GetNormalizedSize();
    float freq_scale = detail * 4.0f / fmax(0.1f, size_multiplier);
    
    float max_distance = EffectGridMedianHalfExtent(grid, GetNormalizedScale()) * 1.7320508f;
    float radius_scale_normalized = (helix_radius / 200.0f) * size_multiplier;
    float radius_scale = max_distance * radius_scale_normalized * 0.9f;
    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float rot_rel_x = rotated_pos.x - origin.x;
    float rot_rel_z = rotated_pos.z - origin.z;

    float radial_distance = sqrt(rot_rel_x*rot_rel_x + rot_rel_z*rot_rel_z);
    float angle = atan2(rot_rel_z, rot_rel_x);
    float coord_along_helix = NormalizeGridAxis01(rotated_pos.y, grid.min_y, grid.max_y);
    float helix_height = coord_along_helix * freq_scale + progress;
    float coord1 = rot_rel_x;
    float coord2 = rot_rel_z;

    float helix1_angle = angle + helix_height;
    float helix1_c1 = radius_scale * cos(helix1_angle);
    float helix1_c2 = radius_scale * sin(helix1_angle);
    float helix1_distance = sqrt((coord1 - helix1_c1)*(coord1 - helix1_c1) + (coord2 - helix1_c2)*(coord2 - helix1_c2));

    float helix2_angle = angle + helix_height + 3.14159f;
    float helix2_c1 = radius_scale * cos(helix2_angle);
    float helix2_c2 = radius_scale * sin(helix2_angle);
    float helix2_distance = sqrt((coord1 - helix2_c1)*(coord1 - helix2_c1) + (coord2 - helix2_c2)*(coord2 - helix2_c2));

    float strand_core_thickness = 6.0f + radius_scale * 0.25f;
    float strand_glow_thickness = 16.0f + radius_scale * 0.5f;

    float helix1_core = 1.0f - smoothstep(0.0f, strand_core_thickness, helix1_distance);
    float helix2_core = 1.0f - smoothstep(0.0f, strand_core_thickness, helix2_distance);
    float helix1_glow = (1.0f - smoothstep(strand_core_thickness, strand_glow_thickness, helix1_distance)) * 0.5f;
    float helix2_glow = (1.0f - smoothstep(strand_core_thickness, strand_glow_thickness, helix2_distance)) * 0.5f;

    float helix1_intensity = helix1_core + helix1_glow;
    float helix2_intensity = helix2_core + helix2_glow;

    float base_pair_frequency = freq_scale * 1.2f;
    float base_pair_phase = fmod(coord_along_helix * base_pair_frequency + progress * 0.5f, 6.28318f);
    float base_pair_active = exp(-fmod(base_pair_phase, 6.28318f / 3.0f) * 8.0f);
    float base_pair_connection = 0.0f;

    if(base_pair_active > 0.1f && radial_distance < radius_scale * 1.8f)
    {
        float rung_distance = fabs(radial_distance - radius_scale);
        float rung_thickness = 1.5f + radius_scale * 0.2f;
        float rung_intensity = 1.0f - smoothstep(0.0f, rung_thickness, rung_distance);
        float rung_glow = (1.0f - smoothstep(rung_thickness, rung_thickness * 2.0f, rung_distance)) * 0.4f;
        base_pair_connection = (rung_intensity + rung_glow) * base_pair_active;
    }

    float groove_angle = fmod(angle - helix_height * 0.5f, 6.28318f);
    float major_groove = exp(-fabs(groove_angle - 3.14159f) * 2.0f) * 0.15f;
    float minor_groove = exp(-fabs(groove_angle) * 3.0f) * 0.1f;
    float groove_effect = 1.0f - (major_groove + minor_groove);

    float strand_intensity = fmax(helix1_intensity, helix2_intensity);
    
    float ambient_glow = 0.08f * (1.0f - fmin(1.0f, radial_distance / (radius_scale * 4.0f)));
    
    float total_intensity = (strand_intensity + base_pair_connection) * groove_effect;
    float energy_pulse = 0.15f * sin(helix_height * 4.0f - progress * 3.0f) * strand_intensity;
    total_intensity = total_intensity + energy_pulse + ambient_glow;
    total_intensity = fmax(0.0f, fmin(1.0f, total_intensity * 1.3f));

    RGBColor final_color;
    if(GetRainbowMode())
    {
        float hue = helix_height * 50.0f + angle * 20.0f + time * rate * 12.0f;
        if(base_pair_connection > 0.3f)
        {
            hue += 180.0f;
        }
        final_color = GetRainbowColor(hue);
    }
    else
    {
        if(base_pair_connection > strand_intensity * 0.5f)
        {
            float position = (GetColors().size() > 1) ? 0.7f : 0.5f;
            float pos = fmodf(position + time * rate * 0.02f, 1.0f);
            if(pos < 0.0f) pos += 1.0f;
            final_color = GetColorAtPosition(pos);
        }
        else
        {
            float position = fmod(helix_height * 0.3f, 1.0f);
            float pos = fmodf(position + time * rate * 0.02f, 1.0f);
            if(pos < 0.0f) pos += 1.0f;
            final_color = GetColorAtPosition(pos);
        }
    }

    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    r = (unsigned char)(r * total_intensity);
    g = (unsigned char)(g * total_intensity);
    b = (unsigned char)(b * total_intensity);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json DNAHelix::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["helix_radius"] = helix_radius;
    return j;
}

void DNAHelix::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("helix_radius")) helix_radius = settings["helix_radius"];

    if(radius_slider) radius_slider->setValue(helix_radius);
}
