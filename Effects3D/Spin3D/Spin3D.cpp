// SPDX-License-Identifier: GPL-2.0-only

#include "Spin3D.h"

/*---------------------------------------------------------*\
| Register this effect with the effect manager             |
\*---------------------------------------------------------*/
REGISTER_EFFECT_3D(Spin3D);
#include <QGridLayout>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Spin3D::Spin3D(QWidget* parent) : SpatialEffect3D(parent)
{
    surface_combo = nullptr;
    arms_slider = nullptr;
    surface_type = 0;   // Default to Floor
    num_arms = 3;
    progress = 0.0f;

    SetFrequency(50);
    SetRainbowMode(true);

    std::vector<RGBColor> default_colors;
    default_colors.push_back(0x000000FF);  // Red
    default_colors.push_back(0x0000FF00);  // Green
    default_colors.push_back(0x00FF0000);  // Blue
    SetColors(default_colors);
}

Spin3D::~Spin3D()
{
}

EffectInfo3D Spin3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;  // Using new standardized system
    info.effect_name = "3D Spin";
    info.effect_description = "Rotating patterns on room surfaces with multiple arm configurations";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_WAVE;  // Reuse WAVE type for now
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 0;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = false;
    info.needs_frequency = false;

    // Standardized parameter scaling
    // Room-scale rotation defaults: faster spin, broader patterns
    info.default_speed_scale = 25.0f;       // (speed/100)² * 25
    info.default_frequency_scale = 6.0f;
    info.use_size_parameter = true;

    // Control visibility (show all controls except frequency)
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;     // Show standard frequency control
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_axis_control = true;          // Show standard axis control as well
    info.show_color_controls = true;

    return info;
}

void Spin3D::SetupCustomUI(QWidget* parent)
{
    QWidget* spin_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(spin_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(new QLabel("Surface:"), 0, 0);
    surface_combo = new QComboBox();
    surface_combo->addItem("Floor");
    surface_combo->addItem("Ceiling");
    surface_combo->addItem("Left Wall");
    surface_combo->addItem("Right Wall");
    surface_combo->addItem("Front Wall");
    surface_combo->addItem("Back Wall");
    surface_combo->addItem("Floor & Ceiling");
    surface_combo->addItem("Left & Right Walls");
    surface_combo->addItem("Front & Back Walls");
    surface_combo->addItem("All Walls");
    surface_combo->addItem("Entire Room");
    surface_combo->addItem("Origin (Room/User Center)");
    surface_combo->setCurrentIndex(surface_type);
    surface_combo->setToolTip("Select which surfaces to spin on");
    layout->addWidget(surface_combo, 0, 1);

    layout->addWidget(new QLabel("Arms:"), 1, 0);
    arms_slider = new QSlider(Qt::Horizontal);
    arms_slider->setRange(1, 8);
    arms_slider->setValue(num_arms);
    arms_slider->setToolTip("Number of spinning arms");
    layout->addWidget(arms_slider, 1, 1);

    if(parent && parent->layout())
    {
        parent->layout()->addWidget(spin_widget);
    }

    connect(surface_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Spin3D::OnSpinParameterChanged);
    connect(arms_slider, &QSlider::valueChanged, this, &Spin3D::OnSpinParameterChanged);
}

void Spin3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_WAVE;
}

void Spin3D::OnSpinParameterChanged()
{
    if(surface_combo) surface_type = surface_combo->currentIndex();
    if(arms_slider) num_arms = arms_slider->value();
    emit ParametersChanged();
}

RGBColor Spin3D::CalculateColor(float, float, float, float)
{
    return 0x00000000;
}

float Spin3D::Clamp01(float value)
{
    if(value < 0.0f)
    {
        return 0.0f;
    }
    if(value > 1.0f)
    {
        return 1.0f;
    }
    return value;
}

float Spin3D::ComputeSpinXZ(float rel_x,
                            float rel_y,
                            float rel_z,
                            float y_bias,
                            float half_width,
                            float half_depth,
                            float half_height) const
{
    unsigned int arms = (num_arms == 0U) ? 1U : num_arms;
    float angle = atan2(rel_z, rel_x);
    float radius = sqrtf(rel_x*rel_x + rel_z*rel_z);
    float spin_angle = angle * (float)arms - progress;
    float period = 6.28318f / (float)arms;
    float arm_position = fmod(spin_angle, period);
    if(arm_position < 0.0f)
    {
        arm_position += period;
    }
    float blade_width = 0.4f * period;
    if(arm_position >= blade_width)
    {
        return 0.0f;
    }
    float blade = 1.0f - (arm_position / blade_width);
    float radial_fade = fmax(0.3f, 1.0f - (radius / (half_width + half_depth)) * 0.8f);
    float y_distance = fabs((rel_y - y_bias) / (half_height * 0.8f));
    float y_fade = Clamp01(1.0f - y_distance);
    return blade * radial_fade * y_fade;
}

float Spin3D::ComputeSpinYZ(float rel_x,
                            float rel_y,
                            float rel_z,
                            float bias_sign,
                            float half_width,
                            float half_depth,
                            float half_height) const
{
    unsigned int arms = (num_arms == 0U) ? 1U : num_arms;
    float angle = atan2(rel_z, rel_y);
    float radius = sqrtf(rel_y*rel_y + rel_z*rel_z);
    float spin_angle = angle * (float)arms - progress;
    float period = 6.28318f / (float)arms;
    float arm_position = fmod(spin_angle, period);
    if(arm_position < 0.0f)
    {
        arm_position += period;
    }
    float blade_width = 0.4f * period;
    if(arm_position >= blade_width)
    {
        return 0.0f;
    }
    float blade = 1.0f - (arm_position / blade_width);
    float radial_fade = fmax(0.3f, 1.0f - (radius / (half_depth + half_height)) * 0.8f);
    float wall_distance = (bias_sign > 0.0f)
        ? (half_width - rel_x)
        : (half_width + rel_x);
    float wall_ratio = wall_distance / (half_width * 0.8f);
    float wall_fade = Clamp01(1.0f - wall_ratio);
    return blade * radial_fade * wall_fade;
}

float Spin3D::ComputeSpinXY(float rel_x,
                            float rel_y,
                            float rel_z,
                            float bias_sign,
                            float half_width,
                            float half_depth,
                            float half_height) const
{
    unsigned int arms = (num_arms == 0U) ? 1U : num_arms;
    float angle = atan2(rel_y, rel_x);
    float radius = sqrtf(rel_x*rel_x + rel_y*rel_y);
    float spin_angle = angle * (float)arms - progress;
    float period = 6.28318f / (float)arms;
    float arm_position = fmod(spin_angle, period);
    if(arm_position < 0.0f)
    {
        arm_position += period;
    }
    float blade_width = 0.4f * period;
    if(arm_position >= blade_width)
    {
        return 0.0f;
    }
    float blade = 1.0f - (arm_position / blade_width);
    float radial_fade = fmax(0.3f, 1.0f - (radius / (half_width + half_height)) * 0.8f);
    float wall_distance = (bias_sign > 0.0f)
        ? (half_depth - rel_z)
        : (half_depth + rel_z);
    float wall_ratio = wall_distance / (half_depth * 0.8f);
    float wall_fade = Clamp01(1.0f - wall_ratio);
    return blade * radial_fade * wall_fade;
}

// Grid-aware version with room-relative fades and radii
RGBColor Spin3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z, grid))
    {
        return 0x00000000;
    }

    progress = CalculateProgress(time);

    // Room-aware distances
    float half_w = grid.width * 0.5f;
    float half_d = grid.depth * 0.5f;
    float half_h = grid.height * 0.5f;

    float intensity = 0.0f;

    switch(surface_type)
    {
        case 0: intensity = ComputeSpinXZ(rel_x, rel_y, rel_z, 0.0f, half_w, half_d, half_h); break;                   // Floor
        case 1: intensity = ComputeSpinXZ(rel_x, rel_y, rel_z, half_h, half_w, half_d, half_h); break;                  // Ceiling bias upwards
        case 2: intensity = ComputeSpinYZ(rel_x, rel_y, rel_z, -1.0f, half_w, half_d, half_h); break;                   // Left wall
        case 3: intensity = ComputeSpinYZ(rel_x, rel_y, rel_z, 1.0f, half_w, half_d, half_h); break;                    // Right wall
        case 4: intensity = ComputeSpinXY(rel_x, rel_y, rel_z, -1.0f, half_w, half_d, half_h); break;                   // Front wall
        case 5: intensity = ComputeSpinXY(rel_x, rel_y, rel_z, 1.0f, half_w, half_d, half_h); break;                    // Back wall
        case 6:
            intensity = fmax(ComputeSpinXZ(rel_x, rel_y, rel_z, 0.0f, half_w, half_d, half_h),
                             ComputeSpinXZ(rel_x, rel_y, rel_z, half_h, half_w, half_d, half_h));
            break; // Floor & Ceiling
        case 7:
            intensity = fmax(ComputeSpinYZ(rel_x, rel_y, rel_z, -1.0f, half_w, half_d, half_h),
                             ComputeSpinYZ(rel_x, rel_y, rel_z, 1.0f, half_w, half_d, half_h));
            break; // Left & Right
        case 8:
            intensity = fmax(ComputeSpinXY(rel_x, rel_y, rel_z, -1.0f, half_w, half_d, half_h),
                             ComputeSpinXY(rel_x, rel_y, rel_z, 1.0f, half_w, half_d, half_h));
            break; // Front & Back
        case 9:
        {
            float yz_neg = ComputeSpinYZ(rel_x, rel_y, rel_z, -1.0f, half_w, half_d, half_h);
            float yz_pos = ComputeSpinYZ(rel_x, rel_y, rel_z, 1.0f, half_w, half_d, half_h);
            float xy_neg = ComputeSpinXY(rel_x, rel_y, rel_z, -1.0f, half_w, half_d, half_h);
            float xy_pos = ComputeSpinXY(rel_x, rel_y, rel_z, 1.0f, half_w, half_d, half_h);
            intensity = fmax(fmax(yz_neg, yz_pos), fmax(xy_neg, xy_pos));
            break;
        }
        case 10: // Entire room (max of all)
        default:
        {
            float xz_floor = ComputeSpinXZ(rel_x, rel_y, rel_z, 0.0f, half_w, half_d, half_h);
            float xz_ceiling = ComputeSpinXZ(rel_x, rel_y, rel_z, half_h, half_w, half_d, half_h);
            float yz_neg = ComputeSpinYZ(rel_x, rel_y, rel_z, -1.0f, half_w, half_d, half_h);
            float yz_pos = ComputeSpinYZ(rel_x, rel_y, rel_z, 1.0f, half_w, half_d, half_h);
            float xy_neg = ComputeSpinXY(rel_x, rel_y, rel_z, -1.0f, half_w, half_d, half_h);
            float xy_pos = ComputeSpinXY(rel_x, rel_y, rel_z, 1.0f, half_w, half_d, half_h);
            intensity = fmax(xz_floor,
                             fmax(xz_ceiling,
                                  fmax(fmax(yz_neg, yz_pos), fmax(xy_neg, xy_pos))));
            break;
        }
    }

    intensity = fmax(0.0f, fmin(1.0f, intensity));

    RGBColor final_color = GetRainbowMode() ? GetRainbowColor(progress * 57.2958f + intensity * 120.0f)
                                            : GetColorAtPosition(intensity);
    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    r = (unsigned char)(r * intensity);
    g = (unsigned char)(g * intensity);
    b = (unsigned char)(b * intensity);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json Spin3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["surface_type"] = surface_type;
    j["num_arms"] = num_arms;
    return j;
}

void Spin3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("surface_type")) surface_type = settings["surface_type"];
    if(settings.contains("num_arms")) num_arms = settings["num_arms"];

    if(surface_combo) surface_combo->setCurrentIndex(surface_type);
    if(arms_slider) arms_slider->setValue(num_arms);
}
