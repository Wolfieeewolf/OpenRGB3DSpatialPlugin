// SPDX-License-Identifier: GPL-2.0-only

#include "Spiral.h"

REGISTER_EFFECT_3D(Spiral);
#include <algorithm>
#include <QGridLayout>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Spiral::Spiral(QWidget* parent) : SpatialEffect3D(parent)
{
    arms_slider = nullptr;
    arms_label = nullptr;
    pattern_combo = nullptr;
    gap_slider = nullptr;
    gap_label = nullptr;
    num_arms = 3;
    pattern_type = 0;
    gap_size = 30;
    progress = 0.0f;

    SetFrequency(50);
    SetRainbowMode(true);

    std::vector<RGBColor> default_colors;
    default_colors.push_back(0x000000FF);
    default_colors.push_back(0x0000FF00);
    default_colors.push_back(0x00FF0000);
    SetColors(default_colors);
}

Spiral::~Spiral() = default;

EffectInfo3D Spiral::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;
    info.effect_name = "Spiral";
    info.effect_description = "Spiral pattern with configurable arms and gap";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_SPIRAL;
    info.is_reversible = true;
    info.supports_random = false;
    info.max_speed = 100;
    info.min_speed = 1;
    info.user_colors = 2;
    info.has_custom_settings = true;
    info.needs_3d_origin = false;
    info.needs_direction = false;
    info.needs_thickness = false;
    info.needs_arms = true;
    info.needs_frequency = false;

    info.default_speed_scale = 35.0f;
    info.default_frequency_scale = 40.0f;
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

void Spiral::SetupCustomUI(QWidget* parent)
{
    QWidget* spiral_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(spiral_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(new QLabel("Pattern:"), 0, 0);
    pattern_combo = new QComboBox();
    pattern_combo->addItem("Smooth Spiral");
    pattern_combo->addItem("Pinwheel");
    pattern_combo->addItem("Sharp Blades");
    pattern_combo->addItem("Swirl Circles");
    pattern_combo->addItem("Hypnotic");
    pattern_combo->addItem("Simple Spin");
    pattern_combo->setCurrentIndex(pattern_type);
    pattern_combo->setToolTip("Spiral pattern style");
    layout->addWidget(pattern_combo, 0, 1);

    layout->addWidget(new QLabel("Arms:"), 1, 0);
    arms_slider = new QSlider(Qt::Horizontal);
    arms_slider->setRange(2, 8);
    arms_slider->setValue(num_arms);
    arms_slider->setToolTip("Number of spiral arms");
    layout->addWidget(arms_slider, 1, 1);
    arms_label = new QLabel(QString::number(num_arms));
    arms_label->setMinimumWidth(30);
    layout->addWidget(arms_label, 1, 2);

    layout->addWidget(new QLabel("Gap Size:"), 2, 0);
    gap_slider = new QSlider(Qt::Horizontal);
    gap_slider->setRange(10, 80);
    gap_slider->setValue(gap_size);
    gap_slider->setToolTip("Gap size between blades");
    layout->addWidget(gap_slider, 2, 1);
    gap_label = new QLabel(QString::number(gap_size));
    gap_label->setMinimumWidth(30);
    layout->addWidget(gap_label, 2, 2);

    AddWidgetToParent(spiral_widget, parent);

    connect(pattern_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Spiral::OnSpiralParameterChanged);
    connect(arms_slider, &QSlider::valueChanged, this, &Spiral::OnSpiralParameterChanged);
    connect(arms_slider, &QSlider::valueChanged, arms_label, [this](int value) {
        arms_label->setText(QString::number(value));
    });
    connect(gap_slider, &QSlider::valueChanged, this, &Spiral::OnSpiralParameterChanged);
    connect(gap_slider, &QSlider::valueChanged, gap_label, [this](int value) {
        gap_label->setText(QString::number(value));
    });
}

void Spiral::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_SPIRAL;
}

void Spiral::OnSpiralParameterChanged()
{
    if(pattern_combo) pattern_type = pattern_combo->currentIndex();
    if(arms_slider)
    {
        num_arms = arms_slider->value();
        if(arms_label) arms_label->setText(QString::number(num_arms));
    }
    if(gap_slider)
    {
        gap_size = gap_slider->value();
        if(gap_label) gap_label->setText(QString::number(gap_size));
    }
    emit ParametersChanged();
}


RGBColor Spiral::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
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
    float freq_scale = detail * 0.15f / fmax(0.1f, size_multiplier);

    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float rot_rel_x = rotated_pos.x - origin.x;
    float rot_rel_z = rotated_pos.z - origin.z;

    float angle = atan2(rot_rel_z, rot_rel_x);
    EffectGridAxisHalfExtents ex = MakeEffectGridAxisHalfExtents(grid, GetNormalizedScale());
    float r_xz = EffectGridHorizontalRadialNormXZ(rot_rel_x, rot_rel_z, ex.hw, ex.hd);
    float norm_radius = EffectGridHorizontalRadialNorm01(r_xz);
    norm_radius = fmaxf(0.0f, fminf(1.0f, norm_radius));

    float norm_twist = 0.0f;
    if(grid.height > 0.001f)
    {
        norm_twist = (rotated_pos.y - grid.min_y) / grid.height;
    }
    norm_twist = fmaxf(0.0f, fminf(1.0f, norm_twist));
    
    float z_twist = norm_twist * freq_scale * 3.0f;
    float spiral_angle = angle * num_arms + norm_radius * (detail * 6.0f) + z_twist - progress;

    float spiral_value;
    float gap_factor = gap_size / 100.0f;

    switch(pattern_type)
    {
        case 0:
            spiral_value = sin(spiral_angle) * (1.0f + 0.4f * cos(norm_twist * freq_scale * 3.0f + progress * 0.7f));
            spiral_value += 0.3f * cos(spiral_angle * 0.5f + norm_twist * freq_scale * 4.5f + progress * 1.2f);
            spiral_value = (spiral_value + 1.5f) / 3.0f;
            spiral_value = fmax(0.0f, fmin(1.0f, spiral_value));
            break;
        case 1:
            {
                float arm_angle = fmod(spiral_angle, 6.28318f / num_arms);
                if(arm_angle < 0) arm_angle += 6.28318f / num_arms;
                float blade_width = (1.0f - gap_factor) * (6.28318f / num_arms);
                if(arm_angle < blade_width)
                {
                    float blade_position = arm_angle / blade_width;
                    spiral_value = 0.5f + 0.5f * cos(blade_position * 3.14159f);
                }
                else
                {
                    spiral_value = 0.0f;
                }
                float radial_fade = 0.4f + 0.6f * (1.0f - exp(-norm_radius * (detail * 0.8f)));
                spiral_value = spiral_value * radial_fade + 0.1f * radial_fade;
            }
            break;
        case 2:
            {
                float arm_angle = fmod(spiral_angle, 6.28318f / num_arms);
                if(arm_angle < 0) arm_angle += 6.28318f / num_arms;
                float blade_width = (1.0f - gap_factor) * (6.28318f / num_arms);
                if(arm_angle < blade_width)
                {
                    float blade_position = fabs(arm_angle - blade_width * 0.5f) / (blade_width * 0.5f);
                    spiral_value = 1.0f - blade_position * blade_position;
                }
                else
                {
                    spiral_value = 0.0f;
                }
                float energy_pulse = 0.2f * sin(norm_radius * (detail * 1.2f) - progress * 2.0f);
                spiral_value = fmax(0.0f, spiral_value + energy_pulse);
                float radial_fade = 0.4f + 0.6f * (1.0f - exp(-norm_radius * (detail * 0.8f)));
                spiral_value *= radial_fade;
            }
            break;
        case 3:
            {
                float circle_angle = atan2(rot_rel_z, rot_rel_x) + progress * 2.0f;
                float ring_phase = norm_radius * (detail * 8.0f) * num_arms - circle_angle * num_arms;
                spiral_value = 0.5f + 0.5f * sin(ring_phase) * (1.0f - norm_radius * 0.3f);
                spiral_value = fmax(0.0f, fmin(1.0f, spiral_value));
            }
            break;
        case 4:
            {
                float hyp_angle = atan2(rot_rel_z, rot_rel_x) - progress * 3.0f;
                float hyp_radius = norm_radius * (detail * 4.0f);
                spiral_value = 0.5f + 0.5f * sin(hyp_angle * 2.0f + hyp_radius - progress * 2.0f) * cos(norm_twist * freq_scale * 3.0f + progress);
                spiral_value = fmax(0.0f, fmin(1.0f, spiral_value));
            }
            break;
        case 5:
            {
                float period = 6.28318f / (float)num_arms;
                float arm_angle = fmod(spiral_angle, period);
                if(arm_angle < 0.0f) arm_angle += period;
                float blade_width = 0.4f * period;
                float blade_core = (arm_angle < blade_width) ? (1.0f - arm_angle / blade_width) : 0.0f;
                float blade_glow = 0.0f;
                if(arm_angle < blade_width * 1.5f)
                {
                    float glow_dist = fabsf(arm_angle - blade_width * 0.5f) / (blade_width * 0.5f);
                    blade_glow = 0.3f * (1.0f - glow_dist);
                }
                spiral_value = fmin(1.0f, blade_core + blade_glow);
                float radial_fade = 0.35f + 0.65f * (1.0f - fmin(1.0f, norm_radius) * 0.6f);
                spiral_value = spiral_value * radial_fade + 0.08f * radial_fade;
            }
            break;
        default:
            spiral_value = 0.5f;
            break;
    }

    RGBColor final_color;
    if((pattern_type == 1 || pattern_type == 2 || pattern_type == 5) && !GetRainbowMode())
    {
        float arm_index = fmod(spiral_angle / (6.28318f / num_arms), (float)num_arms);
        if(arm_index < 0) arm_index += num_arms;
        float pos = fmodf((arm_index / (float)num_arms) + time * rate * 0.02f, 1.0f);
        if(pos < 0.0f) pos += 1.0f;
        final_color = GetColorAtPosition(pos);
    }
    else if(GetRainbowMode())
    {
        float hue = spiral_angle * 57.2958f + time * rate * 12.0f;
        final_color = GetRainbowColor(hue);
    }
    else
    {
        float pos = fmodf(spiral_value + time * rate * 0.02f, 1.0f);
        if(pos < 0.0f) pos += 1.0f;
        final_color = GetColorAtPosition(pos);
    }

    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    return (b << 16) | (g << 8) | r;
}

nlohmann::json Spiral::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["num_arms"] = num_arms;
    j["pattern_type"] = pattern_type;
    j["gap_size"] = gap_size;
    return j;
}

void Spiral::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("num_arms"))
    {
        num_arms = settings["num_arms"].get<unsigned int>();
        if(arms_slider)
        {
            arms_slider->setValue(num_arms);
        }
    }
    if(settings.contains("pattern_type") && settings["pattern_type"].is_number_integer())
    {
        pattern_type = std::max(0, std::min(settings["pattern_type"].get<int>(), 5));
        if(pattern_combo)
        {
            pattern_combo->setCurrentIndex(pattern_type);
        }
    }
    if(settings.contains("gap_size"))
    {
        gap_size = settings["gap_size"].get<unsigned int>();
        if(gap_slider)
        {
            gap_slider->setValue(gap_size);
        }
    }
}
