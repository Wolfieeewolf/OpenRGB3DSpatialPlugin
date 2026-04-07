// SPDX-License-Identifier: GPL-2.0-only

#include "Plasma.h"

REGISTER_EFFECT_3D(Plasma);
#include <QGridLayout>
#include <cmath>

Plasma::Plasma(QWidget* parent) : SpatialEffect3D(parent)
{
    pattern_combo = nullptr;
    pattern_type = 0;
    progress = 0.0f;

    std::vector<RGBColor> plasma_colors = {
        0x0000FF00,
        0x00FF00FF,
        0x00FFFF00
    };
    if(GetColors().empty())
    {
        SetColors(plasma_colors);
    }
    SetFrequency(60);
    SetRainbowMode(false);
}

Plasma::~Plasma() = default;

EffectInfo3D Plasma::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;
    info.effect_name = "Plasma";
    info.effect_description = "Animated plasma clouds with configurable pattern and speed";
    info.category = "Spatial";
    info.effect_type = SPATIAL_EFFECT_PLASMA;
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

    info.default_speed_scale = 8.0f;
    info.default_frequency_scale = 8.0f;
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

void Plasma::SetupCustomUI(QWidget* parent)
{
    QWidget* plasma_widget = new QWidget();
    QGridLayout* layout = new QGridLayout(plasma_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    layout->addWidget(new QLabel("Pattern:"), 0, 0);
    pattern_combo = new QComboBox();
    pattern_combo->addItem("Classic");
    pattern_combo->addItem("Swirl");
    pattern_combo->addItem("Ripple");
    pattern_combo->addItem("Organic");
    pattern_combo->addItem("Noise");
    pattern_combo->addItem("CubeFire");
    pattern_combo->setCurrentIndex(pattern_type);
    pattern_combo->setToolTip("Plasma pattern variant");
    layout->addWidget(pattern_combo, 0, 1);

    AddWidgetToParent(plasma_widget, parent);

    connect(pattern_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Plasma::OnPlasmaParameterChanged);
}

void Plasma::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_PLASMA;
}

void Plasma::OnPlasmaParameterChanged()
{
    if(pattern_combo) pattern_type = pattern_combo->currentIndex();
    emit ParametersChanged();
}


RGBColor Plasma::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
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
    float freq_scale = detail * 0.8f / fmax(0.1f, size_multiplier);

    Vector3D rotated_pos = TransformPointByRotation(x, y, z, origin);
    float rot_rel_x = rotated_pos.x - origin.x;
    float rot_rel_y = rotated_pos.y - origin.y;
    float rot_rel_z = rotated_pos.z - origin.z;

    float coord1 = (grid.width > 0.001f) ? ((rotated_pos.x - grid.min_x) / grid.width) : 0.5f;
    float coord2 = (grid.height > 0.001f) ? ((rotated_pos.y - grid.min_y) / grid.height) : 0.5f;
    float coord3 = (grid.depth > 0.001f) ? ((rotated_pos.z - grid.min_z) / grid.depth) : 0.5f;
    coord1 = fmaxf(0.0f, fminf(1.0f, coord1));
    coord2 = fmaxf(0.0f, fminf(1.0f, coord2));
    coord3 = fmaxf(0.0f, fminf(1.0f, coord3));

    float plasma_value;
    switch(pattern_type)
    {
        case 0:
            plasma_value =
                sin((coord1 + progress * 2.0f) * freq_scale * 10.0f) +
                sin((coord2 + progress * 1.7f) * freq_scale * 8.0f) +
                sin((coord1 + coord2 + progress * 1.3f) * freq_scale * 6.0f) +
                cos((coord1 - coord2 + progress * 2.2f) * freq_scale * 7.0f) +
                sin(sqrtf(coord1*coord1 + coord2*coord2) * freq_scale * 5.0f + progress * 1.5f) +
                cos(coord3 * freq_scale * 4.0f + progress * 0.9f);
            break;
        case 1:
            {
                float angle = atan2(coord2 - 0.5f, coord1 - 0.5f);
                float radius = sqrtf((coord1 - 0.5f)*(coord1 - 0.5f) + (coord2 - 0.5f)*(coord2 - 0.5f));
                plasma_value =
                    sin(angle * 4.0f + radius * freq_scale * 8.0f + progress * 2.0f) +
                    sin(angle * 3.0f - radius * freq_scale * 6.0f + progress * 1.5f) +
                    cos(angle * 5.0f + radius * freq_scale * 4.0f - progress * 1.8f) +
                    sin(coord3 * freq_scale * 5.0f + progress) +
                    cos((angle * 2.0f + coord3 * freq_scale * 3.0f) + progress * 1.2f);
            }
            break;
        case 2:
            {
                float dist_from_center = sqrtf((coord1 - 0.5f)*(coord1 - 0.5f) + (coord2 - 0.5f)*(coord2 - 0.5f));
                plasma_value =
                    sin(dist_from_center * freq_scale * 10.0f - progress * 3.0f) +
                    sin(dist_from_center * freq_scale * 15.0f - progress * 2.3f) +
                    cos(dist_from_center * freq_scale * 8.0f + progress * 1.8f) +
                    sin((coord1 + coord2) * freq_scale * 6.0f + progress * 1.2f) +
                    cos(coord3 * freq_scale * 5.0f - progress * 0.7f);
            }
            break;
        case 3:
            {
                float flow1 = sin(coord1 * freq_scale * 8.0f + sin(coord2 * freq_scale * 12.0f + progress) + progress * 0.5f);
                float flow2 = cos(coord2 * freq_scale * 9.0f + cos(coord3 * freq_scale * 11.0f + progress * 1.3f));
                float flow3 = sin(coord3 * freq_scale * 7.0f + sin(coord1 * freq_scale * 13.0f + progress * 0.7f));
                float flow4 = cos((coord1 + coord2) * freq_scale * 6.0f + sin(progress * 1.5f));
                float flow5 = sin((coord2 + coord3) * freq_scale * 5.0f + cos(progress * 1.8f));
                plasma_value = flow1 + flow2 + flow3 + flow4 + flow5;
            }
            break;
        case 4:
            {
                float n1 = sin((coord1 + progress * 0.5f) * freq_scale * 40.0f) * sin((coord2 + progress * 0.3f) * freq_scale * 52.0f) * sin((coord3 + progress * 0.7f) * freq_scale * 31.0f);
                float n2 = sin((coord1 * 2.3f + coord2 + progress) * freq_scale * 20.0f) * cos((coord2 * 1.7f + coord3 + progress * 1.2f) * freq_scale * 25.0f);
                float n3 = cos((coord1 + coord2 * 2.1f + coord3) * freq_scale * 15.0f + progress * 2.0f);
                plasma_value = n1 * 0.5f + n2 * 0.35f + n3 * 0.15f;
            }
            break;
        case 5:
            {
                float r = sqrtf((coord1 - 0.5f)*(coord1 - 0.5f) + (coord2 - 0.5f)*(coord2 - 0.5f) + (coord3 - 0.5f)*(coord3 - 0.5f));
                plasma_value =
                    sin(r * freq_scale * 30.0f - progress * 2.0f) +
                    sin((coord1 + coord2) * freq_scale * 20.0f + progress * 1.5f) * 0.6f +
                    cos((coord2 + coord3) * freq_scale * 18.0f - progress * 1.2f) * 0.5f +
                    sin(coord3 * freq_scale * 25.0f + progress * 0.8f) * 0.4f;
            }
            break;
        default:
            plasma_value = 0.5f;
            break;
    }

    plasma_value = (plasma_value + 6.0f) / 12.0f;
    plasma_value = fmax(0.0f, fmin(1.0f, plasma_value));

    float radial_distance = sqrtf(rot_rel_x*rot_rel_x + rot_rel_y*rot_rel_y + rot_rel_z*rot_rel_z);
    float max_radius = EffectGridBoundingRadius(grid, GetNormalizedScale());
    float depth_factor = 1.0f;
    if(max_radius > 0.001f)
    {
        float normalized_dist = fmin(1.0f, radial_distance / max_radius);
        depth_factor = 0.45f + 0.55f * (1.0f - normalized_dist * 0.6f);
    }

    RGBColor final_color = GetRainbowMode() ? GetRainbowColor(plasma_value * 360.0f + time * rate * 12.0f)
                                            : GetColorAtPosition(plasma_value);

    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    r = (unsigned char)(r * depth_factor);
    g = (unsigned char)(g * depth_factor);
    b = (unsigned char)(b * depth_factor);
    return (b << 16) | (g << 8) | r;
}

nlohmann::json Plasma::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["pattern_type"] = pattern_type;
    return j;
}

void Plasma::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("pattern_type") && settings["pattern_type"].is_number_integer())
        pattern_type = std::max(0, std::min(settings["pattern_type"].get<int>(), 5));

    if(pattern_combo) pattern_combo->setCurrentIndex(pattern_type);
}
