// SPDX-License-Identifier: GPL-2.0-only

#include "Explosion3D.h"

REGISTER_EFFECT_3D(Explosion3D);

#include <QGridLayout>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QCheckBox>
#include "../EffectHelpers.h"

static const int TYPE_STANDARD = 0;
static const int TYPE_NUKE = 1;
static const int TYPE_LANDMINE = 2;
static const int TYPE_BOMB = 3;
static const int TYPE_WALL_BOUNCE = 4;

float Explosion3D::explosionHash(unsigned int seed, unsigned int salt)
{
    unsigned int v = seed * 73856093u ^ salt * 19349663u;
    v = (v << 13u) ^ v;
    v = v * (v * v * 15731u + 789221u) + 1376312589u;
    return ((v & 0xFFFFu) / 65535.0f) * 2.0f - 1.0f;
}

Explosion3D::Explosion3D(QWidget* parent) : SpatialEffect3D(parent)
{
    intensity_slider = nullptr;
    intensity_label = nullptr;
    type_combo = nullptr;
    burst_count_spin = nullptr;
    loop_check = nullptr;
    particle_slider = nullptr;
    particle_label = nullptr;
    explosion_intensity = 75;
    progress = 0.0f;
    explosion_type = 0;
    burst_count = 0;   /* 0 = infinite */
    loop = true;
    particle_amount = 40;

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
    info.effect_name = "Explosion";
    info.effect_description = "Expanding shockwave from origin";
    info.category = "Spatial";
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
    int row = 0;

    layout->addWidget(new QLabel("Type:"), row, 0);
    type_combo = new QComboBox();
    type_combo->setToolTip("Explosion style: Standard (shockwave + debris), Nuke (mushroom), Land Mine (ground burst), Bomb (symmetric + debris), Wall Bounce");
    type_combo->addItem("Standard");
    type_combo->addItem("Nuke");
    type_combo->addItem("Land Mine");
    type_combo->addItem("Bomb");
    type_combo->addItem("Wall Bounce");
    type_combo->setCurrentIndex(explosion_type);
    layout->addWidget(type_combo, row, 1, 1, 2);
    row++;

    layout->addWidget(new QLabel("Intensity:"), row, 0);
    intensity_slider = new QSlider(Qt::Horizontal);
    intensity_slider->setRange(10, 200);
    intensity_slider->setValue(explosion_intensity);
    intensity_slider->setToolTip("Explosion energy (radius and wave thickness)");
    layout->addWidget(intensity_slider, row, 1);
    intensity_label = new QLabel(QString::number(explosion_intensity));
    intensity_label->setMinimumWidth(30);
    layout->addWidget(intensity_label, row, 2);
    row++;

    layout->addWidget(new QLabel("Particles:"), row, 0);
    particle_slider = new QSlider(Qt::Horizontal);
    particle_slider->setRange(0, 100);
    particle_slider->setValue(particle_amount);
    particle_slider->setToolTip("Debris/spark density for Standard and Bomb (0 = wave only)");
    layout->addWidget(particle_slider, row, 1);
    particle_label = new QLabel(QString::number(particle_amount) + "%");
    particle_label->setMinimumWidth(36);
    layout->addWidget(particle_label, row, 2);
    row++;

    layout->addWidget(new QLabel("Burst count:"), row, 0);
    burst_count_spin = new QSpinBox();
    burst_count_spin->setRange(0, 10);
    burst_count_spin->setValue(burst_count);
    burst_count_spin->setSpecialValueText("Infinite");
    burst_count_spin->setToolTip("Number of explosions per cycle (0 = never stop)");
    layout->addWidget(burst_count_spin, row, 1, 1, 2);
    row++;

    loop_check = new QCheckBox("Loop (repeat; ties to color change)");
    loop_check->setChecked(loop);
    loop_check->setToolTip("When on, after burst count repeats and hue advances. When off, effect ends after that many explosions.");
    layout->addWidget(loop_check, row, 0, 1, 3);
    row++;

    AddWidgetToParent(explosion_widget, parent);

    connect(intensity_slider, &QSlider::valueChanged, this, &Explosion3D::OnExplosionParameterChanged);
    connect(intensity_slider, &QSlider::valueChanged, intensity_label, [this](int value) {
        intensity_label->setText(QString::number(value));
    });
    connect(type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Explosion3D::OnExplosionParameterChanged);
    connect(particle_slider, &QSlider::valueChanged, this, [this](int value) {
        particle_amount = value;
        if(particle_label) particle_label->setText(QString::number(value) + "%");
        emit ParametersChanged();
    });
    connect(burst_count_spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &Explosion3D::OnExplosionParameterChanged);
    connect(loop_check, &QCheckBox::toggled, this, [this](bool checked) { loop = checked; emit ParametersChanged(); });
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
    if(burst_count_spin) burst_count = burst_count_spin->value();
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
    j["burst_count"] = burst_count;
    j["loop"] = loop;
    j["particle_amount"] = particle_amount;
    return j;
}

void Explosion3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("explosion_intensity")) explosion_intensity = settings["explosion_intensity"];
    if(settings.contains("explosion_type")) explosion_type = settings["explosion_type"];
    if(settings.contains("burst_count")) burst_count = std::max(0, std::min(10, (int)settings["burst_count"]));
    if(settings.contains("loop")) loop = settings["loop"];
    if(settings.contains("particle_amount")) particle_amount = std::max(0, std::min(100, (int)settings["particle_amount"]));

    if(intensity_slider) intensity_slider->setValue(explosion_intensity);
    if(type_combo) type_combo->setCurrentIndex(explosion_type);
    if(burst_count_spin) burst_count_spin->setValue(burst_count);
    if(loop_check) loop_check->setChecked(loop);
    if(particle_slider) particle_slider->setValue(particle_amount);
    if(particle_label) particle_label->setText(QString::number(particle_amount) + "%");
}

float Explosion3D::particleDebrisAt(float x, float y, float z, float burst_phase, float distance, float radius, int type_id) const
{
    if(particle_amount <= 0) return 0.0f;
    unsigned int sx = (unsigned int)((x + 1e6f) * 100.0f);
    unsigned int sy = (unsigned int)((y + 1e6f) * 100.0f);
    unsigned int sz = (unsigned int)((z + 1e6f) * 100.0f);
    float h = (explosionHash(sx ^ sy ^ sz, (unsigned int)(burst_phase * 1000.0f) + (unsigned int)type_id * 100u) + 1.0f) * 0.5f;
    if(h > (float)particle_amount / 100.0f) return 0.0f;
    float spread = radius * (0.3f + 0.7f * burst_phase);
    float falloff = expf(-fabsf(distance - spread * 0.7f) * 0.15f);
    return falloff * (0.5f + 0.5f * h) * ((float)particle_amount / 100.0f);
}

RGBColor Explosion3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);

    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
        return 0x00000000;

    float raw_progress = time * GetScaledSpeed();
    float progress_in_cycle = fmodf(raw_progress, CYCLE_DURATION);
    float burst_phase = progress_in_cycle / CYCLE_DURATION;
    int burst_index = (int)(raw_progress / CYCLE_DURATION);

    int max_bursts = (burst_count <= 0) ? 999999 : burst_count;
    if(!loop && burst_index >= max_bursts)
        return 0x00000000;

    int effective_burst = (burst_count <= 0) ? burst_index : (loop ? (burst_index % max_bursts) : burst_index);
    progress = burst_phase;
    float hue_offset = (float)effective_burst * 45.0f;

    float actual_frequency = GetScaledFrequency();
    float size_multiplier = GetNormalizedSize();
    float freq_scale = actual_frequency * 0.01f / (size_multiplier > 0.001f ? size_multiplier : 1.0f);

    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float rot_rel_x = rotated_pos.x - origin.x;
    float rot_rel_y = rotated_pos.y - origin.y;
    float rot_rel_z = rotated_pos.z - origin.z;

    float horiz = sqrtf(rot_rel_x * rot_rel_x + rot_rel_z * rot_rel_z);
    float distance = sqrtf(rot_rel_x*rot_rel_x + rot_rel_y*rot_rel_y + rot_rel_z*rot_rel_z);

    float explosion_radius = burst_phase * (explosion_intensity * 0.25f) * size_multiplier;
    float wave_thickness = (8.0f + explosion_intensity * 0.08f) * size_multiplier;
    float explosion_intensity_final = 0.0f;
    float hue_base = 60.0f;

    if(explosion_type == TYPE_NUKE)
    {
        float stem_height = explosion_radius * 0.85f;
        float stem_radius = explosion_radius * 0.12f;
        float cap_bottom = stem_height * 0.7f;
        float cap_top = stem_height * 1.4f;
        float cap_h_radius = explosion_radius * 0.55f;
        float dist_to_stem_axis = horiz;
        float in_stem = (rot_rel_y >= 0.0f && rot_rel_y <= cap_bottom && dist_to_stem_axis < stem_radius * (1.0f + burst_phase * 0.3f))
            ? (1.0f - smoothstep(0.0f, stem_radius, dist_to_stem_axis)) * (1.0f - rot_rel_y / cap_bottom * 0.3f) : 0.0f;
        float in_cap = 0.0f;
        if(rot_rel_y >= cap_bottom && rot_rel_y <= cap_top)
        {
            float cap_center_y = cap_bottom + (cap_top - cap_bottom) * 0.5f;
            float r_at_y = cap_h_radius * (0.3f + 0.7f * (rot_rel_y - cap_bottom) / (cap_top - cap_bottom));
            float d_cap = sqrtf(horiz * horiz + (rot_rel_y - cap_center_y) * (rot_rel_y - cap_center_y) * 0.3f);
            in_cap = (1.0f - smoothstep(r_at_y - wave_thickness * 0.5f, r_at_y + wave_thickness, d_cap)) * expf(-d_cap * 0.05f);
        }
        float flash_core = (distance < explosion_radius * 0.2f) ? (1.0f - distance / (explosion_radius * 0.2f)) : 0.0f;
        explosion_intensity_final = fmax(fmax(in_stem, in_cap), flash_core * 1.2f);
        explosion_intensity_final = fmin(1.0f, explosion_intensity_final + flash_core * 0.5f);
        hue_base = 35.0f;
    }
    else if(explosion_type == TYPE_LANDMINE)
    {
        float ground_dist = sqrtf(rot_rel_x*rot_rel_x + rot_rel_z*rot_rel_z + (rot_rel_y * 0.25f) * (rot_rel_y * 0.25f));
        float cone_up = rot_rel_y > 0.0f ? (1.0f - smoothstep(0.0f, explosion_radius * 0.8f, rot_rel_y)) : 1.0f;
        explosion_radius *= 0.9f;
        float primary = 1.0f - smoothstep(explosion_radius - wave_thickness, explosion_radius + wave_thickness, ground_dist);
        primary *= expf(-fabsf(ground_dist - explosion_radius) * 0.1f);
        primary *= (0.7f + 0.3f * cone_up);
        float secondary_radius = explosion_radius * 0.65f;
        float secondary = 1.0f - smoothstep(secondary_radius - wave_thickness * 0.4f, secondary_radius + wave_thickness * 0.5f, ground_dist);
        secondary *= expf(-fabsf(ground_dist - secondary_radius) * 0.15f) * 0.6f * cone_up;
        float dust = 0.12f * sinf(ground_dist * freq_scale * 6.0f - burst_phase * 6.0f) * expf(-ground_dist * 0.06f) * cone_up;
        float core = (ground_dist < explosion_radius * 0.25f && rot_rel_y < explosion_radius * 0.3f)
            ? (1.0f - ground_dist / (explosion_radius * 0.25f)) * 0.9f : 0.0f;
        explosion_intensity_final = fmin(1.0f, primary + secondary + dust + core);
    }
    else if(explosion_type == TYPE_BOMB)
    {
        float primary = 1.0f - smoothstep(explosion_radius - wave_thickness, explosion_radius + wave_thickness, distance);
        primary *= expf(-fabsf(distance - explosion_radius) * 0.08f);
        float secondary_radius = explosion_radius * 0.7f;
        float secondary = 1.0f - smoothstep(secondary_radius - wave_thickness * 0.5f, secondary_radius + wave_thickness * 0.5f, distance);
        secondary *= expf(-fabsf(distance - secondary_radius) * 0.12f) * 0.7f;
        float ang = atan2f(rot_rel_y, horiz + 0.001f);
        float lobe = 0.65f + 0.35f * fabsf(cosf(ang * 3.0f));
        float shock = (0.2f * sinf(distance * freq_scale * 8.0f - burst_phase * 4.0f) + 0.1f * sinf(distance * freq_scale * 12.0f - burst_phase * 6.0f)) * expf(-distance * 0.07f) * lobe;
        float core = (distance < explosion_radius * 0.28f) ? (1.0f - distance / (explosion_radius * 0.28f)) * 0.85f : 0.0f;
        explosion_intensity_final = fmin(1.0f, primary + secondary + shock + core);
        explosion_intensity_final = fmax(explosion_intensity_final, particleDebrisAt(x, y, z, burst_phase, distance, explosion_radius, TYPE_BOMB));
    }
    else if(explosion_type == TYPE_WALL_BOUNCE)
    {
        float max_extent = sqrtf(grid.width*grid.width + grid.depth*grid.depth + grid.height*grid.height) * 0.5f;
        float period = fmax(0.1f, max_extent);
        float travel = burst_phase * (explosion_intensity * 0.25f) * size_multiplier;
        float t = fmodf(travel, 2.0f * period);
        explosion_radius = (t <= period) ? t : (2.0f * period - t);
        float primary = 1.0f - smoothstep(explosion_radius - wave_thickness, explosion_radius + wave_thickness, distance);
        primary *= expf(-fabsf(distance - explosion_radius) * 0.08f);
        float core = (distance < explosion_radius * 0.3f) ? (1.0f - distance / (explosion_radius * 0.3f)) * 0.8f : 0.0f;
        explosion_intensity_final = fmin(1.0f, primary + core);
    }
    else
    {
        float primary = 1.0f - smoothstep(explosion_radius - wave_thickness, explosion_radius + wave_thickness, distance);
        primary *= expf(-fabsf(distance - explosion_radius) * 0.08f);
        float secondary_radius = explosion_radius * 0.7f;
        float secondary = 1.0f - smoothstep(secondary_radius - wave_thickness * 0.5f, secondary_radius + wave_thickness * 0.5f, distance);
        secondary *= expf(-fabsf(distance - secondary_radius) * 0.12f) * 0.7f;
        float shock = 0.25f * sinf(distance * freq_scale * 8.0f - burst_phase * 4.0f * 6.28f) + 0.15f * sinf(distance * freq_scale * 12.0f - burst_phase * 6.0f * 6.28f);
        shock *= expf(-distance * 0.08f);
        float core = (distance < explosion_radius * 0.3f) ? (1.0f - distance / (explosion_radius * 0.3f)) * 0.85f : 0.0f;
        explosion_intensity_final = fmin(1.0f, primary + secondary + shock + core);
        float debris = particleDebrisAt(x, y, z, burst_phase, distance, explosion_radius, TYPE_STANDARD);
        explosion_intensity_final = fmax(explosion_intensity_final, debris);
    }

    float ambient = 0.08f * (1.0f - fmin(1.0f, distance / (explosion_radius * 2.0f + 1.0f)));
    explosion_intensity_final = fmin(1.0f, explosion_intensity_final + ambient);

    RGBColor final_color = GetRainbowMode()
        ? GetRainbowColor(hue_base + hue_offset - (explosion_intensity_final * 50.0f) + burst_phase * 15.0f)
        : GetColorAtPosition(explosion_intensity_final);

    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;

    r = (unsigned char)(r * explosion_intensity_final);
    g = (unsigned char)(g * explosion_intensity_final);
    b = (unsigned char)(b * explosion_intensity_final);

    return (b << 16) | (g << 8) | r;
}
