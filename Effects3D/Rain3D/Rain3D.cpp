/*---------------------------------------------------------*\
| Rain3D.cpp                                               |
|                                                          |
|   Room-scale volumetric rain effect                      |
|                                                          |
|   SPDX-License-Identifier: GPL-2.0-only                  |
\*---------------------------------------------------------*/

#include "Rain3D.h"
#include <QGridLayout>
#include <cmath>

REGISTER_EFFECT_3D(Rain3D);

Rain3D::Rain3D(QWidget* parent) : SpatialEffect3D(parent)
{
    density_slider = nullptr;
    wind_slider = nullptr;
    rain_density = 50;
    wind = 0;

    SetRainbowMode(false);
    SetFrequency(50);
}

Rain3D::~Rain3D()
{
}

EffectInfo3D Rain3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;
    info.effect_name = "3D Rain";
    info.effect_description = "Volumetric rain with wind drift";
    info.category = "3D Spatial";
    info.effect_type = SPATIAL_EFFECT_RAIN;
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

    // Room-scale defaults: visible motion and spacing
    info.default_speed_scale = 30.0f;
    info.default_frequency_scale = 8.0f;
    info.use_size_parameter = true;

    // Show standard controls
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_axis_control = true;
    info.show_color_controls = true;

    return info;
}

void Rain3D::SetupCustomUI(QWidget* parent)
{
    QWidget* w = new QWidget();
    QGridLayout* layout = new QGridLayout(w);
    layout->setContentsMargins(0,0,0,0);

    layout->addWidget(new QLabel("Density:"), 0, 0);
    density_slider = new QSlider(Qt::Horizontal);
    density_slider->setRange(5, 100);
    density_slider->setValue(rain_density);
    layout->addWidget(density_slider, 0, 1);

    layout->addWidget(new QLabel("Wind:"), 1, 0);
    wind_slider = new QSlider(Qt::Horizontal);
    wind_slider->setRange(-50, 50);
    wind_slider->setValue(wind);
    layout->addWidget(wind_slider, 1, 1);

    if(parent && parent->layout())
    {
        parent->layout()->addWidget(w);
    }

    connect(density_slider, &QSlider::valueChanged, this, &Rain3D::OnRainParameterChanged);
    connect(wind_slider, &QSlider::valueChanged, this, &Rain3D::OnRainParameterChanged);
}

void Rain3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_RAIN;
}

void Rain3D::OnRainParameterChanged()
{
    if(density_slider) rain_density = density_slider->value();
    if(wind_slider) wind = wind_slider->value();
    emit ParametersChanged();
}

static inline float hash31(int x, int y, int z)
{
    // Small integer hash to pseudo-randomize per-LED behavior deterministically
    int n = x * 73856093 ^ y * 19349663 ^ z * 83492791;
    n = (n << 13) ^ n;
    return 0.5f * (1.0f + (float)((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

RGBColor Rain3D::CalculateColor(float x, float y, float z, float time)
{
    // Legacy path not used - grid-aware version preferred
    (void)x; (void)y; (void)z; (void)time;
    return 0x00000000;
}

RGBColor Rain3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    Vector3D origin = GetEffectOriginGrid(grid);

    float speed = GetScaledSpeed();
    float freq = GetScaledFrequency();
    float size_m = GetNormalizedSize();

    // Map density (5..100) to drop spacing; higher density => smaller spacing
    float base_spacing = 2.0f + (100.0f - rain_density) * 0.06f; // ~2..8 grid units

    // Lateral wind drift (X/Z) in grid units per second
    float wind_drift = (float)wind * 0.02f;

    // Compute LED-relative coords from origin (room center/user)
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;

    // Axis selection: choose fall axis and lateral plane
    float fall_axis, lat1, lat2;
    EffectAxis use_axis = axis_none ? AXIS_Y : effect_axis;
    switch(use_axis)
    {
        case AXIS_X: fall_axis = rel_x; lat1 = rel_y; lat2 = rel_z; break;
        case AXIS_Y: fall_axis = rel_y; lat1 = rel_x; lat2 = rel_z; break;
        case AXIS_Z: fall_axis = rel_z; lat1 = rel_x; lat2 = rel_y; break;
        case AXIS_RADIAL:
        default:
            // For radial, simulate concentric rainy waves expanding
            fall_axis = sqrtf(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
            lat1 = rel_x; lat2 = rel_z;
            break;
    }

    // Normalize world to grid spacing bands
    float fall = time * speed * 0.5f; // fall speed factor
    float drop_phase = fall_axis + fall;  // falling along chosen axis

    // Wrap phase by spacing to create repeating drop fronts
    float band = fmodf(drop_phase, base_spacing);
    if(band < 0) band += base_spacing;

    // Distance to the center of the drop band
    float band_center = base_spacing * 0.25f; // highlight position
    float band_dist = fabsf(band - band_center);

    // Lateral modulation: create streaks with wind
    float lateral = fmodf(lat1 + lat2 * 0.5f + time * wind_drift, base_spacing);
    if(lateral < 0) lateral += base_spacing;
    float lateral_center = base_spacing * 0.15f;
    float lateral_dist = fabsf(lateral - lateral_center);

    // Combine to get intensity; sharper near drops
    // Broaden bands so corners light up as well
    float band_intensity = fmax(0.0f, 1.0f - (band_dist / (0.3f * size_m + 0.15f)));
    float lateral_intensity = fmax(0.0f, 1.0f - (lateral_dist / (0.35f * size_m + 0.15f)));
    float intensity = band_intensity * lateral_intensity;

    // Slight depth fade with distance from origin to reduce wall saturation
    float room_radius = sqrt((grid.width*grid.width + grid.depth*grid.depth + grid.height*grid.height)) * 0.5f;
    float dist_from_center = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);
    float depth_fade = fmax(0.4f, 1.0f - dist_from_center / (room_radius + 0.001f));
    intensity *= depth_fade;

    intensity = fmax(0.0f, fmin(1.0f, intensity));

    RGBColor final_color;
    if(GetRainbowMode())
    {
        float hue = 200.0f + freq * 3.0f + band * 30.0f; // cool blues by default
        final_color = GetRainbowColor(hue);
    }
    else
    {
        final_color = GetColorAtPosition(0.6f + 0.4f * intensity);
    }

    unsigned char r = final_color & 0xFF;
    unsigned char g = (final_color >> 8) & 0xFF;
    unsigned char b = (final_color >> 16) & 0xFF;
    float brightness_factor = (GetBrightness() / 100.0f) * intensity;
    r = (unsigned char)(r * brightness_factor);
    g = (unsigned char)(g * brightness_factor);
    b = (unsigned char)(b * brightness_factor);
    return (b << 16) | (g << 8) | r;
}
