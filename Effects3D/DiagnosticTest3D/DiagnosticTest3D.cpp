// SPDX-License-Identifier: GPL-2.0-only

#include "DiagnosticTest3D.h"

/*---------------------------------------------------------*\
| Register this effect with the effect manager             |
\*---------------------------------------------------------*/
REGISTER_EFFECT_3D(DiagnosticTest3D);
#include "LogManager.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

DiagnosticTest3D::DiagnosticTest3D(QWidget* parent) : SpatialEffect3D(parent)
{
    test_mode_combo = nullptr;
    log_button = nullptr;
    test_mode = 0;  // X-Axis test by default

    // Set defaults for diagnostic testing
    SetSpeed(50);
    SetBrightness(100);
    SetFrequency(50);
    SetRainbowMode(true);
}

DiagnosticTest3D::~DiagnosticTest3D()
{
}

EffectInfo3D DiagnosticTest3D::GetEffectInfo()
{
    EffectInfo3D info;
    info.info_version = 2;
    info.effect_name = "Diagnostic Test 3D";
    info.effect_description = "Diagnostic tool to test 3D grid positioning and effect calculations";
    info.category = "Diagnostic";
    info.effect_type = SPATIAL_EFFECT_WAVE;
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
    info.needs_frequency = true;

    // Standardized parameter scaling
    info.default_speed_scale = 100.0f;
    info.default_frequency_scale = 10.0f;
    info.use_size_parameter = true;

    // Control visibility
    info.show_speed_control = true;
    info.show_brightness_control = true;
    info.show_frequency_control = true;
    info.show_size_control = true;
    info.show_scale_control = true;
    info.show_fps_control = true;
    info.show_axis_control = true;   // Show standard axis control
    info.show_color_controls = true;

    return info;
}

void DiagnosticTest3D::SetupCustomUI(QWidget* parent)
{
    QWidget* diagnostic_widget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(diagnostic_widget);
    layout->setContentsMargins(0, 0, 0, 0);

    /*---------------------------------------------------------*\
    | Test Mode Selector                                       |
    \*---------------------------------------------------------*/
    QHBoxLayout* test_layout = new QHBoxLayout();
    test_layout->addWidget(new QLabel("Test Mode:"));
    test_mode_combo = new QComboBox();
    test_mode_combo->addItem("X-Axis Gradient (Left->Right)");
    test_mode_combo->addItem("Y-Axis Gradient (Bottom->Top)");
    test_mode_combo->addItem("Z-Axis Gradient (Front->Back)");
    test_mode_combo->addItem("Radial Distance (Center->Out)");
    test_mode_combo->addItem("Grid Corners (8 Points)");
    test_mode_combo->addItem("Distance Rings");
    test_mode_combo->addItem("Axis Planes (XYZ Split)");
    test_mode_combo->addItem("Sequential Flash (Controller Order)");
    test_mode_combo->setCurrentIndex(test_mode);
    test_layout->addWidget(test_mode_combo);
    layout->addLayout(test_layout);

    /*---------------------------------------------------------*\
    | Log Diagnostics Button                                   |
    \*---------------------------------------------------------*/
    log_button = new QPushButton("Log Grid Diagnostics to Console");
    log_button->setStyleSheet("QPushButton { background-color: #2196F3; color: white; font-weight: bold; }");
    layout->addWidget(log_button);

    /*---------------------------------------------------------*\
    | Info Label                                               |
    \*---------------------------------------------------------*/
    QLabel* info_label = new QLabel(
        "This effect visualizes the 3D grid system:\n"
        "- X-Axis: Red (left) -> Green (right)\n"
        "- Y-Axis: Red (bottom) -> Green (top)\n"
        "- Z-Axis: Red (front) -> Green (back)\n"
        "- Radial: Red (center) -> Rainbow (edges)\n"
        "- Corners: Highlights 8 corner positions\n"
        "- Distance Rings: Concentric spheres from center\n"
        "- Axis Planes: Red(X-) Green(Y-) Blue(Z+)\n"
        "- Sequential Flash: Shows Y position order\n"
        "  (bottom controllers flash first, top last)"
    );
    info_label->setWordWrap(true);
    info_label->setStyleSheet("QLabel { background-color: #333; padding: 10px; border-radius: 5px; }");
    layout->addWidget(info_label);

    /*---------------------------------------------------------*\
    | Add to parent layout                                     |
    \*---------------------------------------------------------*/
    if(parent && parent->layout())
    {
        parent->layout()->addWidget(diagnostic_widget);
    }

    /*---------------------------------------------------------*\
    | Connect signals                                          |
    \*---------------------------------------------------------*/
    connect(test_mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &DiagnosticTest3D::OnTestModeChanged);
    connect(log_button, &QPushButton::clicked, this, &DiagnosticTest3D::OnLogDiagnostics);
}

void DiagnosticTest3D::UpdateParams(SpatialEffectParams& params)
{
    params.type = SPATIAL_EFFECT_WAVE;
}

void DiagnosticTest3D::OnTestModeChanged()
{
    if(test_mode_combo)
    {
        test_mode = test_mode_combo->currentIndex();
    }
    emit ParametersChanged();
}

void DiagnosticTest3D::OnLogDiagnostics()
{
    LOG_INFO("[DiagnosticTest3D] speed=%u brightness=%u freq=%u size=%u scale=%u norm_scale=%.2f mode=%d",
             GetSpeed(),
             GetBrightness(),
             GetFrequency(),
             effect_size,
             effect_scale,
             GetNormalizedScale(),
             test_mode);
}

RGBColor DiagnosticTest3D::CalculateColor(float x, float y, float z, float time)
{
    /*---------------------------------------------------------*\
    | NOTE: All coordinates (x, y, z) are in GRID UNITS       |
    | One grid unit equals the configured grid scale          |
    | (default 10mm). LED positions use grid units.           |
    \*---------------------------------------------------------*/

    static float min_x = 99999.0f, max_x = -99999.0f;
    static float min_y = 99999.0f, max_y = -99999.0f;
    static float min_z = 99999.0f, max_z = -99999.0f;
    static float min_dist = 99999.0f, max_dist = -99999.0f;

    // Track min/max values
    if(x < min_x) min_x = x;
    if(x > max_x) max_x = x;
    if(y < min_y) min_y = y;
    if(y > max_y) max_y = y;
    if(z < min_z) min_z = z;
    if(z > max_z) max_z = z;

    Vector3D origin = GetEffectOrigin();
    float rel_x = x - origin.x;
    float rel_y = y - origin.y;
    float rel_z = z - origin.z;
    float distance = sqrt(rel_x*rel_x + rel_y*rel_y + rel_z*rel_z);

    if(distance < min_dist) min_dist = distance;
    if(distance > max_dist) max_dist = distance;

    // Check if within effect boundary
    if(!IsWithinEffectBoundary(rel_x, rel_y, rel_z))
    {
        return 0x00000000;  // Black - outside boundary
    }

    float progress = CalculateProgress(time);

    /*---------------------------------------------------------*\
    | Test Mode 0: X-Axis Gradient (Left to Right)           |
    | Red at min_x, Green at max_x                             |
    | Uses WORLD position (x) not relative position           |
    \*---------------------------------------------------------*/
    if(test_mode == 0)
    {
        float range_x = max_x - min_x;
        if(range_x < 0.01f) range_x = 1.0f;
        float normalized = (x - min_x) / range_x;  // Use world X
        normalized = fmod(normalized + progress * 0.01f, 1.0f);

        unsigned char r = (unsigned char)((1.0f - normalized) * 255);
        unsigned char g = (unsigned char)(normalized * 255);
        unsigned char b = 0;
        return (b << 16) | (g << 8) | r;
    }

    /*---------------------------------------------------------*\
    | Test Mode 1: Y-Axis Gradient (Bottom to Top)           |
    | Red at min_y, Green at max_y                             |
    | Uses WORLD position (y) not relative position           |
    \*---------------------------------------------------------*/
    if(test_mode == 1)
    {
        float range_y = max_y - min_y;
        if(range_y < 0.01f) range_y = 1.0f;
        float normalized = (y - min_y) / range_y;  // Use world Y
        normalized = fmod(normalized + progress * 0.01f, 1.0f);

        unsigned char r = (unsigned char)((1.0f - normalized) * 255);
        unsigned char g = (unsigned char)(normalized * 255);
        unsigned char b = 0;
        return (b << 16) | (g << 8) | r;
    }

    /*---------------------------------------------------------*\
    | Test Mode 2: Z-Axis Gradient (Front to Back)           |
    | Red at min_z, Green at max_z                             |
    | Uses WORLD position (z) not relative position           |
    \*---------------------------------------------------------*/
    if(test_mode == 2)
    {
        float range_z = max_z - min_z;
        if(range_z < 0.01f) range_z = 1.0f;
        float normalized = (z - min_z) / range_z;  // Use world Z
        normalized = fmod(normalized + progress * 0.01f, 1.0f);

        unsigned char r = (unsigned char)((1.0f - normalized) * 255);
        unsigned char g = (unsigned char)(normalized * 255);
        unsigned char b = 0;
        return (b << 16) | (g << 8) | r;
    }

    /*---------------------------------------------------------*\
    | Test Mode 3: Radial Distance (Center to Edges)         |
    | Rainbow based on distance from origin                    |
    \*---------------------------------------------------------*/
    if(test_mode == 3)
    {
        float range_dist = max_dist - min_dist;
        if(range_dist < 0.01f) range_dist = 1.0f;
        float normalized = (distance - min_dist) / range_dist;
        float hue = normalized * 360.0f + progress * 10.0f;
        RGBColor color = GetRainbowColor(hue);

        unsigned char r = (unsigned char)((color & 0xFF));
        unsigned char g = (unsigned char)(((color >> 8) & 0xFF));
        unsigned char b = (unsigned char)(((color >> 16) & 0xFF));
        return (b << 16) | (g << 8) | r;
    }

    /*---------------------------------------------------------*\
    | Test Mode 4: Grid Corners (8 Corner Points)            |
    | Highlights the 8 corners of the bounding box             |
    \*---------------------------------------------------------*/
    if(test_mode == 4)
    {
        float corner_threshold = 0.5f;
        bool near_corner = false;

        // Check if near any of the 8 corners
        if((fabs(rel_x - min_x) < corner_threshold || fabs(rel_x - max_x) < corner_threshold) &&
           (fabs(rel_y - min_y) < corner_threshold || fabs(rel_y - max_y) < corner_threshold) &&
           (fabs(rel_z - min_z) < corner_threshold || fabs(rel_z - max_z) < corner_threshold))
        {
            near_corner = true;
        }

        if(near_corner)
        {
            // Pulsing white at corners
            float pulse = (sin(progress * 0.1f) + 1.0f) * 0.5f;
            unsigned char intensity = (unsigned char)(pulse * 255);
            return (intensity << 16) | (intensity << 8) | intensity;
        }
        else
        {
            // Dim blue everywhere else
            unsigned char b = (unsigned char)(50);
            return (b << 16);
        }
    }

    /*---------------------------------------------------------*\
    | Test Mode 5: Distance Rings (Concentric Spheres)       |
    | Shows concentric rings expanding from center             |
    \*---------------------------------------------------------*/
    if(test_mode == 5)
    {
        float ring_spacing = 2.0f;
        float ring_position = fmod(distance + progress * 0.1f, ring_spacing);
        float ring_intensity = (ring_position < 0.3f) ? 1.0f : 0.1f;

        float hue = (distance / max_dist) * 360.0f;
        RGBColor color = GetRainbowColor(hue);

        unsigned char r = (unsigned char)((color & 0xFF) * ring_intensity);
        unsigned char g = (unsigned char)(((color >> 8) & 0xFF) * ring_intensity);
        unsigned char b = (unsigned char)(((color >> 16) & 0xFF) * ring_intensity);
        return (b << 16) | (g << 8) | r;
    }

    /*---------------------------------------------------------*\
    | Test Mode 6: Axis Planes (XYZ Split View)              |
    | Red for X-, Green for Y-, Blue for Z+                    |
    \*---------------------------------------------------------*/
    if(test_mode == 6)
    {
        unsigned char r = 0, g = 0, b = 0;

        if(rel_x < 0) r = (unsigned char)(fabs(rel_x / min_x) * 255);
        if(rel_y > 0) g = (unsigned char)((rel_y / max_y) * 255);
        if(rel_z > 0) b = (unsigned char)((rel_z / max_z) * 255);

        return (b << 16) | (g << 8) | r;
    }

    /*---------------------------------------------------------*\
    | Test Mode 7: Sequential Flash (Y Position Order)       |
    | Flashes LEDs based on Y position to show ordering        |
    \*---------------------------------------------------------*/
    if(test_mode == 7)
    {
        // Normalize Y position to 0-1 range
        float range_y = max_y - min_y;
        if(range_y < 0.01f) range_y = 1.0f;
        float normalized_y = (rel_y - min_y) / range_y;

        // Create a wave that sweeps from bottom (0) to top (1)
        float wave_position = fmod(progress * 0.02f, 1.0f);
        float distance_from_wave = fabs(normalized_y - wave_position);

        // Flash when wave passes this Y position
        float flash_width = 0.1f;
        if(distance_from_wave < flash_width)
        {
            float intensity = 1.0f - (distance_from_wave / flash_width);
            unsigned char white = (unsigned char)(intensity * 255);

            return (white << 16) | (white << 8) | white;
        }
        else
        {
            // Dim blue background
            unsigned char b = (unsigned char)(30);
            return (b << 16);
        }
    }

    return 0x00000000;
}

RGBColor DiagnosticTest3D::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    /*---------------------------------------------------------*\
    | Diagnostic Test is a spatial diagnostic effect - simply  |
    | delegate to the standard CalculateColor implementation   |
    \*---------------------------------------------------------*/
    (void)grid;  // Unused parameter
    return CalculateColor(x, y, z, time);
}

nlohmann::json DiagnosticTest3D::SaveSettings() const
{
    nlohmann::json j = SpatialEffect3D::SaveSettings();
    j["test_mode"] = test_mode;
    return j;
}

void DiagnosticTest3D::LoadSettings(const nlohmann::json& settings)
{
    SpatialEffect3D::LoadSettings(settings);
    if(settings.contains("test_mode")) test_mode = settings["test_mode"];

    if(test_mode_combo) test_mode_combo->setCurrentIndex(test_mode);
}
