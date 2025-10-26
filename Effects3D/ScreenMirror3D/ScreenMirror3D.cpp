/*---------------------------------------------------------*\
| ScreenMirror3D.cpp                                        |
|                                                           |
|   3D Spatial screen mirroring effect with ambilight      |
|                                                           |
|   Date: 2025-10-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "ScreenMirror3D.h"
#include "ScreenCaptureManager.h"
#include "DisplayPlane3D.h"
#include "DisplayPlaneManager.h"
#include "Geometry3DUtils.h"
#include "LogManager.h"
#include "VirtualReferencePoint3D.h"

/*---------------------------------------------------------*\
| Register this effect with the effect manager             |
\*---------------------------------------------------------*/
REGISTER_EFFECT_3D(ScreenMirror3D);

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QSlider>
#include <QTimer>
#include <chrono>

ScreenMirror3D::ScreenMirror3D(QWidget* parent)
    : SpatialEffect3D(parent)
    , global_scale_slider(nullptr)
    , smoothing_time_slider(nullptr)
    , brightness_slider(nullptr)
    , test_pattern_check(nullptr)
    , screen_preview_check(nullptr)
    , global_scale(1.0f) // Default 100% global scale
    , smoothing_time_ms(50.0f)
    , brightness_multiplier(1.0f)
    , show_test_pattern(false)
    , reference_points(nullptr)
{
}

ScreenMirror3D::~ScreenMirror3D()
{
    StopCaptureIfNeeded();
}

/*---------------------------------------------------------*\
| Effect Info                                              |
\*---------------------------------------------------------*/
EffectInfo3D ScreenMirror3D::GetEffectInfo()
{
    EffectInfo3D info           = {};
    info.info_version           = 2;
    info.effect_name            = "Screen Mirror 3D";
    info.effect_description     = "Projects screen content onto LEDs using 3D spatial mapping";
    info.category               = "Ambilight";
    info.effect_type            = SPATIAL_EFFECT_WAVE;  // TODO: Add ambilight type
    info.is_reversible          = false;
    info.supports_random        = false;
    info.max_speed              = 100;
    info.min_speed              = 1;
    info.user_colors            = 0;
    info.has_custom_settings    = true;
    info.needs_3d_origin        = false;
    info.needs_direction        = false;
    info.needs_thickness        = false;
    info.needs_arms             = false;
    info.needs_frequency        = false;
    info.use_size_parameter     = false;

    return info;
}

/*---------------------------------------------------------*\
| Setup Custom UI                                          |
\*---------------------------------------------------------*/
void ScreenMirror3D::SetupCustomUI(QWidget* parent)
{
    QWidget* container = new QWidget();
    QVBoxLayout* main_layout = new QVBoxLayout(container);

    // Multi-Monitor Status
    QGroupBox* status_group = new QGroupBox("Multi-Monitor Status");
    QVBoxLayout* status_layout = new QVBoxLayout();

    QLabel* info_label = new QLabel(
        "This effect automatically detects all display planes with capture sources.\n"
        "Each LED will sample from its nearest monitor in 3D space.\n"
        "Configure monitors in the Display Planes tab."
    );
    info_label->setWordWrap(true);
    info_label->setStyleSheet("QLabel { color: #888; font-style: italic; }");
    status_layout->addWidget(info_label);

    // Show active planes count
    std::vector<DisplayPlane3D*> planes = DisplayPlaneManager::instance()->GetDisplayPlanes();
    int active_count = 0;
    for (auto plane : planes)
    {
        if (plane && !plane->GetCaptureSourceId().empty())
        {
            active_count++;
        }
    }

    QLabel* count_label = new QLabel(QString("Active Monitors: %1").arg(active_count));
    count_label->setStyleSheet("QLabel { font-weight: bold; font-size: 14pt; }");
    status_layout->addWidget(count_label);

    status_group->setLayout(status_layout);
    main_layout->addWidget(status_group);

    // Per-Monitor Settings
    QGroupBox* monitors_container = new QGroupBox("Per-Monitor Settings");
    QVBoxLayout* monitors_layout = new QVBoxLayout();

    QLabel* monitor_info = new QLabel(
        "Configure individual settings for each monitor.\n"
        "Each monitor can have its own scale, softness, blend, and edge zone settings."
    );
    monitor_info->setWordWrap(true);
    monitor_info->setStyleSheet("QLabel { color: #888; font-style: italic; font-size: 9pt; }");
    monitors_layout->addWidget(monitor_info);

    // Create expandable settings group for each monitor
    for (auto plane : planes)
    {
        if (plane && !plane->GetCaptureSourceId().empty())
        {
            std::string plane_name = plane->GetName();

            // Get or create settings for this monitor
            if (monitor_settings.find(plane_name) == monitor_settings.end())
            {
                monitor_settings[plane_name] = MonitorSettings(); // Use defaults
            }
            MonitorSettings& settings = monitor_settings[plane_name];

            // Create collapsible group box
            settings.group_box = new QGroupBox(QString::fromStdString(plane_name));
            settings.group_box->setCheckable(true);
            settings.group_box->setChecked(settings.enabled);
            settings.group_box->setToolTip(QString("Enable/disable and configure '%1'").arg(QString::fromStdString(plane_name)));
            connect(settings.group_box, &QGroupBox::toggled, this, &ScreenMirror3D::OnParameterChanged);

            QFormLayout* monitor_form = new QFormLayout();

            // Scale (0-200 slider = 0.0-2.0 actual value)
            settings.scale_slider = new QSlider(Qt::Horizontal);
            settings.scale_slider->setRange(0, 200);
            settings.scale_slider->setValue((int)(settings.scale * 100));
            settings.scale_slider->setTickPosition(QSlider::TicksBelow);
            settings.scale_slider->setTickInterval(25);
            settings.scale_slider->setToolTip("How much this monitor affects LEDs (0 = off, 100 = normal, 200 = double reach)");
            connect(settings.scale_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
            monitor_form->addRow("Scale:", settings.scale_slider);

            // Edge Softness (0-100 slider = 0-100%)
            settings.softness_slider = new QSlider(Qt::Horizontal);
            settings.softness_slider->setRange(0, 100);
            settings.softness_slider->setValue((int)settings.edge_softness);
            settings.softness_slider->setTickPosition(QSlider::TicksBelow);
            settings.softness_slider->setTickInterval(10);
            settings.softness_slider->setToolTip("Feathering for smooth fade (0% = hard cutoff, 30% = natural, 50% = very soft)");
            connect(settings.softness_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
            monitor_form->addRow("Edge Softness:", settings.softness_slider);

            // Blend (0-100 slider = 0-100%)
            settings.blend_slider = new QSlider(Qt::Horizontal);
            settings.blend_slider->setRange(0, 100);
            settings.blend_slider->setValue((int)settings.blend);
            settings.blend_slider->setTickPosition(QSlider::TicksBelow);
            settings.blend_slider->setTickInterval(10);
            settings.blend_slider->setToolTip("Blending with other monitors (0% = no blending, 50% = natural, 100% = full blend)");
            connect(settings.blend_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
            monitor_form->addRow("Blend:", settings.blend_slider);

            // Edge Zone Depth (1-50 slider = 0.01-0.50 actual value)
            settings.edge_zone_slider = new QSlider(Qt::Horizontal);
            settings.edge_zone_slider->setRange(1, 50);
            settings.edge_zone_slider->setValue((int)(settings.edge_zone_depth * 100));
            settings.edge_zone_slider->setTickPosition(QSlider::TicksBelow);
            settings.edge_zone_slider->setTickInterval(10);
            settings.edge_zone_slider->setToolTip("Edge sampling depth (1 = 1% of screen for very edge pixels, 50 = 50%)");
            connect(settings.edge_zone_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
            monitor_form->addRow("Edge Zone:", settings.edge_zone_slider);

            // Reference Point Selection
            settings.ref_point_combo = new QComboBox();
            settings.ref_point_combo->addItem("Use Global Reference", -1);
            settings.ref_point_combo->setToolTip("Reference point for calculating falloff distance\nUse Reference Points tab to create custom viewing positions");
            connect(settings.ref_point_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(OnParameterChanged()));
            monitor_form->addRow("Reference Point:", settings.ref_point_combo);

            settings.group_box->setLayout(monitor_form);
            monitors_layout->addWidget(settings.group_box);
        }
    }

    if (monitor_settings.empty())
    {
        QLabel* no_monitors_label = new QLabel("No monitors configured. Set up Display Planes first.");
        no_monitors_label->setStyleSheet("QLabel { color: #cc6600; font-style: italic; }");
        monitors_layout->addWidget(no_monitors_label);
    }

    monitors_container->setLayout(monitors_layout);
    main_layout->addWidget(monitors_container);

    // Global Settings
    QGroupBox* global_group = new QGroupBox("Global Settings");
    QFormLayout* global_form = new QFormLayout();

    // Global Scale (0-200 slider = 0.0-2.0 actual value)
    global_scale_slider = new QSlider(Qt::Horizontal);
    global_scale_slider->setRange(0, 200);
    global_scale_slider->setValue(100); // Default 100 = 1.0 scale
    global_scale_slider->setTickPosition(QSlider::TicksBelow);
    global_scale_slider->setTickInterval(25);
    global_scale_slider->setToolTip("Master scale multiplier for all monitors (50 = half reach, 100 = normal, 200 = double reach)");
    connect(global_scale_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    global_form->addRow("Global Scale:", global_scale_slider);

    global_group->setLayout(global_form);
    main_layout->addWidget(global_group);

    // Capture Settings
    QGroupBox* capture_group = new QGroupBox("Ray-Tracing Settings");
    QFormLayout* capture_form = new QFormLayout();

    QLabel* raytracing_info = new QLabel(
        "Each LED casts a ray toward the screen and samples the exact pixel it \"sees\".\n"
        "Like real light physics: screen is the light source, LEDs catch the light!"
    );
    raytracing_info->setWordWrap(true);
    raytracing_info->setStyleSheet("QLabel { color: #888; font-style: italic; font-size: 9pt; }");
    capture_form->addRow(raytracing_info);

    test_pattern_check = new QCheckBox();
    test_pattern_check->setChecked(false);
    test_pattern_check->setToolTip("Show 4-color test pattern on LEDs instead of screen capture\nRED=bottom-left, GREEN=bottom-right, BLUE=top-right, YELLOW=top-left\nUse this to verify ray-tracing is working correctly");
    connect(test_pattern_check, &QCheckBox::stateChanged, this, &ScreenMirror3D::OnParameterChanged);
    capture_form->addRow("Test Pattern Mode:", test_pattern_check);

    screen_preview_check = new QCheckBox();
    screen_preview_check->setChecked(false);
    screen_preview_check->setToolTip("Display live screen capture on display planes in 3D viewport\nShows what the effect sees - useful for debugging UV mapping and screen alignment");
    connect(screen_preview_check, &QCheckBox::stateChanged, this, &ScreenMirror3D::OnScreenPreviewChanged);
    capture_form->addRow("Show Screen Preview:", screen_preview_check);

    capture_group->setLayout(capture_form);
    main_layout->addWidget(capture_group);

    // Appearance
    QGroupBox* appearance_group = new QGroupBox("Appearance");
    QFormLayout* appearance_form = new QFormLayout();

    // Brightness (0-500 slider = 0.0-5.0 actual value)
    brightness_slider = new QSlider(Qt::Horizontal);
    brightness_slider->setRange(0, 500);
    brightness_slider->setValue(100); // Default 100 = 1.0 brightness
    brightness_slider->setTickPosition(QSlider::TicksBelow);
    brightness_slider->setTickInterval(50);
    brightness_slider->setToolTip("Light intensity - like dimming a bulb (0 = off, 100 = normal, 500 = very bright)");
    connect(brightness_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    appearance_form->addRow("Light Intensity:", brightness_slider);

    // Smoothing Time (0-500 slider = 0-500ms)
    smoothing_time_slider = new QSlider(Qt::Horizontal);
    smoothing_time_slider->setRange(0, 500);
    smoothing_time_slider->setValue(50); // Default 50ms
    smoothing_time_slider->setTickPosition(QSlider::TicksBelow);
    smoothing_time_slider->setTickInterval(50);
    smoothing_time_slider->setToolTip("Temporal smoothing time constant (0 = no smoothing, 50 = normal, 200 = very smooth)");
    connect(smoothing_time_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    appearance_form->addRow("Smoothing Time:", smoothing_time_slider);

    appearance_group->setLayout(appearance_form);
    main_layout->addWidget(appearance_group);

    main_layout->addStretch();

    // Add container to parent's layout
    if(parent && parent->layout())
    {
        parent->layout()->addWidget(container);
    }

    // Start capturing from all configured monitors
    StartCaptureIfNeeded();

    // Emit initial screen preview state (delayed so viewport connection is ready)
    QTimer::singleShot(100, this, &ScreenMirror3D::OnScreenPreviewChanged);
}

/*---------------------------------------------------------*\
| Update Parameters                                        |
\*---------------------------------------------------------*/
void ScreenMirror3D::UpdateParams(SpatialEffectParams& /*params*/)
{
    // Screen mirror doesn't use standard parameters
}

/*---------------------------------------------------------*\
| Calculate Color (not used - we override CalculateColorGrid) |
\*---------------------------------------------------------*/
RGBColor ScreenMirror3D::CalculateColor(float /*x*/, float /*y*/, float /*z*/, float /*time*/)
{
    return ToRGBColor(0, 0, 0);
}

/*---------------------------------------------------------*\
| Calculate Color Grid - The Main Logic                    |
\*---------------------------------------------------------*/
RGBColor ScreenMirror3D::CalculateColorGrid(float x, float y, float z, float /*time*/, const GridContext3D& /*grid*/)
{
    static bool logged_once = false;

    // Get ALL display planes from the manager
    std::vector<DisplayPlane3D*> all_planes = DisplayPlaneManager::instance()->GetDisplayPlanes();

    if (all_planes.empty())
    {
        if (!logged_once)
        {
            LOG_WARNING("[ScreenMirror3D] No display planes configured");
            logged_once = true;
        }
        return ToRGBColor(0, 0, 0);
    }

    // MULTI-MONITOR BLENDING: Collect contributions from ALL monitors (like colored spotlights)
    Vector3D led_pos = {x, y, z};
    ScreenCaptureManager& capture_mgr = ScreenCaptureManager::Instance();

    // Store each monitor's contribution
    struct MonitorContribution
    {
        DisplayPlane3D* plane;
        Geometry3D::PlaneProjection proj;
        std::shared_ptr<CapturedFrame> frame;
        float weight; // How much this monitor contributes (0-1)
        float blend;  // Blend percentage for this monitor (0-100)
    };

    std::vector<MonitorContribution> contributions;

    // Calculate contribution from each monitor
    for (DisplayPlane3D* plane : all_planes)
    {
        if (!plane) continue;

        // Check if this monitor is enabled in the effect settings
        std::string plane_name = plane->GetName();
        auto settings_it = monitor_settings.find(plane_name);
        if (settings_it == monitor_settings.end() || !settings_it->second.enabled)
        {
            // Monitor is not configured or explicitly disabled, skip it
            continue;
        }
        const MonitorSettings& mon_settings = settings_it->second;

        // In test pattern mode, we don't need capture sources - skip those checks
        std::string capture_id = plane->GetCaptureSourceId();
        std::shared_ptr<CapturedFrame> frame = nullptr;

        if (!show_test_pattern)
        {
            // Normal mode: need valid capture source and frames
            if (capture_id.empty())
            {
                if (!logged_once)
                {
                    LOG_WARNING("[ScreenMirror3D] Skipping plane '%s' - no capture source", plane->GetName().c_str());
                }
                continue;
            }

            // Check if capture is actually running and has frames
            if (!capture_mgr.IsCapturing(capture_id))
            {
                if (!logged_once)
                {
                    LOG_WARNING("[ScreenMirror3D] Skipping plane '%s' - capture not running for '%s'",
                               plane->GetName().c_str(), capture_id.c_str());
                }
                continue;
            }

            frame = capture_mgr.GetLatestFrame(capture_id);
            if (!frame || !frame->valid || frame->data.empty())
            {
                if (!logged_once)
                {
                    LOG_WARNING("[ScreenMirror3D] Skipping plane '%s' - no valid frames for '%s'",
                               plane->GetName().c_str(), capture_id.c_str());
                }
                continue;
            }
        }

        // Use spatial mapping for perceptually correct 3D ambilight
        // This maps LED position to screen UV based on spatial relationship
        // Use per-monitor reference point if set, otherwise use global
        const Vector3D* falloff_ref = &global_reference_point;
        if (mon_settings.reference_point_index >= 0 && reference_points &&
            mon_settings.reference_point_index < (int)reference_points->size())
        {
            VirtualReferencePoint3D* ref_point = (*reference_points)[mon_settings.reference_point_index].get();
            if (ref_point)
            {
                static Vector3D custom_ref;
                custom_ref = ref_point->GetPosition();
                falloff_ref = &custom_ref;
            }
        }

        // Grid scale: default 10mm per grid unit
        const float grid_scale_mm = 10.0f;
        Geometry3D::PlaneProjection proj = Geometry3D::SpatialMapToScreen(led_pos, *plane, mon_settings.edge_zone_depth, falloff_ref, grid_scale_mm);

        if (!proj.is_valid) continue;

        // Calculate weight using per-monitor settings and global scale
        // Effective range = base_range (3000mm) * global_scale * monitor_scale
        float effective_range = 3000.0f * global_scale * mon_settings.scale;
        float distance_falloff = Geometry3D::ComputeFalloff(proj.distance, effective_range, mon_settings.edge_softness);

        // TEMPORARILY DISABLED: Angular falloff was rejecting too many LEDs
        // float angular_falloff = Geometry3D::ComputeAngularFalloff(
        //     led_pos, *plane,
        //     horizontal_wrap_angle, vertical_wrap_angle, wrap_strength
        // );

        float weight = distance_falloff; // * angular_falloff;

        // DEBUG: Log what's happening with first LED and right-side LEDs
        static bool logged_falloff = false;
        if (!logged_falloff)
        {
            LOG_WARNING("[ScreenMirror3D DEBUG] First LED: pos(%.1f,%.1f,%.1f) plane:'%s' distance=%.1fmm dist_falloff=%.3f weight=%.3f UV=(%.3f,%.3f)",
                     led_pos.x, led_pos.y, led_pos.z, plane->GetName().c_str(),
                     proj.distance, distance_falloff, weight, proj.u, proj.v);
            logged_falloff = true;
        }

        // DEBUG: Log ALL keyboard and speaker LEDs for debugging
        static int debug_led_count = 0;
        if (debug_led_count < 20)
        {
            LOG_WARNING("[ScreenMirror3D DEBUG] LED: pos(%.1f,%.1f,%.1f) screen:'%s'@(%.1f,%.1f,%.1f) UV=(%.3f,%.3f) weight=%.3f",
                     led_pos.x, led_pos.y, led_pos.z, plane->GetName().c_str(),
                     plane->GetTransform().position.x, plane->GetTransform().position.y, plane->GetTransform().position.z,
                     proj.u, proj.v, weight);
            debug_led_count++;
        }

        // Only include if weight is significant (avoid unnecessary sampling)
        if (weight > 0.01f) // 1% threshold
        {
            MonitorContribution contrib;
            contrib.plane = plane;
            contrib.proj = proj;
            contrib.frame = frame;
            contrib.weight = weight;
            contrib.blend = mon_settings.blend;
            contributions.push_back(contrib);
        }
        else if (debug_led_count < 15 && (led_pos.z < 25.0f || led_pos.x < 210.0f || led_pos.x > 330.0f))
        {
            LOG_WARNING("[ScreenMirror3D DEBUG] Edge LED REJECTED: pos(%.1f,%.1f,%.1f) plane:'%s' distance=%.1fmm weight=%.3f (threshold=0.01)",
                     led_pos.x, led_pos.y, led_pos.z, plane->GetName().c_str(), proj.distance, weight);
        }
    }

    if (contributions.empty())
    {
        if (!logged_once)
        {
            // Count how many planes have capture sources vs total
            int with_capture = 0;
            int capturing = 0;
            for (DisplayPlane3D* plane : all_planes)
            {
                if (plane && !plane->GetCaptureSourceId().empty())
                {
                    with_capture++;
                    if (capture_mgr.IsCapturing(plane->GetCaptureSourceId()))
                    {
                        capturing++;
                    }
                }
            }
            LOG_WARNING("[ScreenMirror3D] No active planes ready. Total: %zu, With capture source: %d, Actively capturing: %d",
                       all_planes.size(), with_capture, capturing);
            logged_once = true;
        }

        // If captures are running but no frames yet, return black (falloff working, just no frames yet)
        // If nothing is capturing, return purple (error - setup issue)
        int capturing_count = 0;
        for (DisplayPlane3D* plane : all_planes)
        {
            if (plane && !plane->GetCaptureSourceId().empty())
            {
                if (capture_mgr.IsCapturing(plane->GetCaptureSourceId()))
                {
                    capturing_count++;
                }
            }
        }

        if (capturing_count > 0)
        {
            return ToRGBColor(0, 0, 0); // Black - falloff working, waiting for first frames
        }
        else
        {
            return ToRGBColor(128, 0, 128); // Purple - no captures running (setup issue)
        }
    }

    // Log success once
    if (!logged_once)
    {
        LOG_WARNING("[ScreenMirror3D] Successfully blending from %zu monitor(s)", contributions.size());
        logged_once = true;
    }

    // BLEND ALL MONITOR CONTRIBUTIONS (like colored spotlights mixing)
    // Each monitor's blend setting controls how much it mixes with others
    // Calculate average blend factor from contributing monitors
    float avg_blend = 0.0f;
    for (const MonitorContribution& contrib : contributions)
    {
        avg_blend += contrib.blend;
    }
    avg_blend /= fmaxf(1.0f, (float)contributions.size());
    float blend_factor = avg_blend / 100.0f; // Convert to 0-1

    // At low blend values, keep only the strongest (nearest) monitor
    if (blend_factor < 0.01f && contributions.size() > 1)
    {
        // Find strongest contribution
        float max_weight = 0.0f;
        size_t strongest_idx = 0;
        for (size_t i = 0; i < contributions.size(); i++)
        {
            if (contributions[i].weight > max_weight)
            {
                max_weight = contributions[i].weight;
                strongest_idx = i;
            }
        }

        // Keep only the strongest
        MonitorContribution strongest = contributions[strongest_idx];
        contributions.clear();
        contributions.push_back(strongest);
    }

    float total_r = 0.0f, total_g = 0.0f, total_b = 0.0f;
    float total_weight = 0.0f;

    for (const MonitorContribution& contrib : contributions)
    {
        // Ray-trace: Sample the exact pixel this LED sees on this monitor
        // Allow UV coordinates outside [0,1] for ambilight edge extension
        float sample_u = contrib.proj.u;
        float sample_v = contrib.proj.v;

        // DEBUG: Log first few LEDs with WHICH SCREEN they're sampling
        static int debug_count = 0;
        if (debug_count < 20)
        {
            const char* quadrant = "";
            if (sample_u < 0.5f && sample_v < 0.5f) quadrant = "BOTTOM-LEFT";
            else if (sample_u >= 0.5f && sample_v < 0.5f) quadrant = "BOTTOM-RIGHT";
            else if (sample_u >= 0.5f && sample_v >= 0.5f) quadrant = "TOP-RIGHT";
            else quadrant = "TOP-LEFT";

            LOG_WARNING("[ScreenMirror3D] LED#%d pos(%.0f,%.0f,%.0f) -> SCREEN:'%s' UV=(%.3f,%.3f) %s weight=%.2f",
                     debug_count, led_pos.x, led_pos.y, led_pos.z,
                     contrib.plane->GetName().c_str(), sample_u, sample_v, quadrant, contrib.weight);
            debug_count++;
        }

        float r, g, b;

        if (show_test_pattern)
        {
            // Test pattern: 4 quadrants with solid colors
            // RED=bottom-left, GREEN=bottom-right, BLUE=top-right, YELLOW=top-left
            // Clamp for test pattern only to show quadrants
            float clamped_u = sample_u;
            float clamped_v = sample_v;
            if (clamped_u < 0.0f) clamped_u = 0.0f;
            if (clamped_u > 1.0f) clamped_u = 1.0f;
            if (clamped_v < 0.0f) clamped_v = 0.0f;
            if (clamped_v > 1.0f) clamped_v = 1.0f;

            bool left_half = (clamped_u < 0.5f);
            bool bottom_half = (clamped_v < 0.5f);

            if (bottom_half && left_half)
            {
                // Bottom-left: RED
                r = 255.0f;
                g = 0.0f;
                b = 0.0f;
            }
            else if (bottom_half && !left_half)
            {
                // Bottom-right: GREEN
                r = 0.0f;
                g = 255.0f;
                b = 0.0f;
            }
            else if (!bottom_half && !left_half)
            {
                // Top-right: BLUE
                r = 0.0f;
                g = 0.0f;
                b = 255.0f;
            }
            else
            {
                // Top-left: YELLOW
                r = 255.0f;
                g = 255.0f;
                b = 0.0f;
            }
        }
        else
        {
            // Normal mode: Sample from captured screen
            // Screen capture is upside-down, so flip V coordinate
            float flipped_v = 1.0f - sample_v;

            // SampleFrame will automatically clamp to edges for ambilight extension
            RGBColor sampled_color = Geometry3D::SampleFrame(
                contrib.frame->data.data(),
                contrib.frame->width,
                contrib.frame->height,
                sample_u,        // U coordinate (may be outside [0,1])
                flipped_v,       // Flipped V for upside-down capture
                true             // Always use bilinear for smooth ambilight
            );

            // Extract RGB
            r = (float)RGBGetRValue(sampled_color);
            g = (float)RGBGetGValue(sampled_color);
            b = (float)RGBGetBValue(sampled_color);
        }

        // Apply blend factor to contribution strength
        float adjusted_weight = contrib.weight * (0.5f + 0.5f * blend_factor);

        total_r += r * adjusted_weight;
        total_g += g * adjusted_weight;
        total_b += b * adjusted_weight;
        total_weight += adjusted_weight;
    }

    // Normalize by total weight (prevents over-brightening when multiple monitors overlap)
    if (total_weight > 0.0f)
    {
        total_r /= total_weight;
        total_g /= total_weight;
        total_b /= total_weight;
    }

    // Apply brightness multiplier
    total_r *= brightness_multiplier;
    total_g *= brightness_multiplier;
    total_b *= brightness_multiplier;

    // Clamp to valid range
    if (total_r > 255.0f) total_r = 255.0f;
    if (total_g > 255.0f) total_g = 255.0f;
    if (total_b > 255.0f) total_b = 255.0f;

    return ToRGBColor((uint8_t)total_r, (uint8_t)total_g, (uint8_t)total_b);
}

/*---------------------------------------------------------*\
| Settings Persistence                                     |
\*---------------------------------------------------------*/
nlohmann::json ScreenMirror3D::SaveSettings() const
{
    nlohmann::json settings;

    // Save global settings
    settings["global_scale"] = global_scale;
    settings["smoothing_time_ms"] = smoothing_time_ms;
    settings["brightness_multiplier"] = brightness_multiplier;
    settings["show_test_pattern"] = show_test_pattern;

    // Save per-monitor settings
    nlohmann::json monitors = nlohmann::json::object();
    for (const auto& pair : monitor_settings)
    {
        nlohmann::json mon;
        mon["enabled"] = pair.second.enabled;
        mon["scale"] = pair.second.scale;
        mon["edge_softness"] = pair.second.edge_softness;
        mon["blend"] = pair.second.blend;
        mon["edge_zone_depth"] = pair.second.edge_zone_depth;
        mon["reference_point_index"] = pair.second.reference_point_index;
        monitors[pair.first] = mon;
    }
    settings["monitor_settings"] = monitors;

    return settings;
}

void ScreenMirror3D::LoadSettings(const nlohmann::json& settings)
{
    // Load global settings
    if (settings.contains("global_scale"))
        global_scale = settings["global_scale"].get<float>();

    if (settings.contains("smoothing_time_ms"))
        smoothing_time_ms = settings["smoothing_time_ms"].get<float>();

    if (settings.contains("brightness_multiplier"))
        brightness_multiplier = settings["brightness_multiplier"].get<float>();

    if (settings.contains("show_test_pattern"))
        show_test_pattern = settings["show_test_pattern"].get<bool>();

    // Load per-monitor settings
    if (settings.contains("monitor_settings"))
    {
        const nlohmann::json& monitors = settings["monitor_settings"];
        for (auto it = monitors.begin(); it != monitors.end(); ++it)
        {
            const std::string& monitor_name = it.key();
            const nlohmann::json& mon = it.value();

            // Get or create monitor settings
            if (monitor_settings.find(monitor_name) == monitor_settings.end())
            {
                monitor_settings[monitor_name] = MonitorSettings();
            }
            MonitorSettings& msettings = monitor_settings[monitor_name];

            if (mon.contains("enabled")) msettings.enabled = mon["enabled"].get<bool>();
            if (mon.contains("scale")) msettings.scale = mon["scale"].get<float>();
            if (mon.contains("edge_softness")) msettings.edge_softness = mon["edge_softness"].get<float>();
            if (mon.contains("blend")) msettings.blend = mon["blend"].get<float>();
            if (mon.contains("edge_zone_depth")) msettings.edge_zone_depth = mon["edge_zone_depth"].get<float>();
            if (mon.contains("reference_point_index")) msettings.reference_point_index = mon["reference_point_index"].get<int>();
        }
    }

    // Update UI - Global (convert float to slider values)
    if (global_scale_slider) global_scale_slider->setValue((int)(global_scale * 100));
    if (smoothing_time_slider) smoothing_time_slider->setValue((int)smoothing_time_ms);
    if (brightness_slider) brightness_slider->setValue((int)(brightness_multiplier * 100));
    if (test_pattern_check) test_pattern_check->setChecked(show_test_pattern);

    // Update UI - Per-Monitor (convert float to slider values)
    for (auto& pair : monitor_settings)
    {
        MonitorSettings& msettings = pair.second;
        if (msettings.group_box) msettings.group_box->setChecked(msettings.enabled);
        if (msettings.scale_slider) msettings.scale_slider->setValue((int)(msettings.scale * 100));
        if (msettings.softness_slider) msettings.softness_slider->setValue((int)msettings.edge_softness);
        if (msettings.blend_slider) msettings.blend_slider->setValue((int)msettings.blend);
        if (msettings.edge_zone_slider) msettings.edge_zone_slider->setValue((int)(msettings.edge_zone_depth * 100));

        // Update reference point dropdown
        if (msettings.ref_point_combo)
        {
            int index = msettings.ref_point_combo->findData(msettings.reference_point_index);
            if (index >= 0)
            {
                msettings.ref_point_combo->setCurrentIndex(index);
            }
        }
    }
}

/*---------------------------------------------------------*\
| Slots                                                    |
\*---------------------------------------------------------*/
void ScreenMirror3D::OnParameterChanged()
{
    // Update global settings (convert slider values to float)
    if (global_scale_slider) global_scale = global_scale_slider->value() / 100.0f;
    if (smoothing_time_slider) smoothing_time_ms = (float)smoothing_time_slider->value();
    if (brightness_slider) brightness_multiplier = brightness_slider->value() / 100.0f;
    if (test_pattern_check) show_test_pattern = test_pattern_check->isChecked();

    // Update per-monitor settings (convert slider values to float)
    for (auto& pair : monitor_settings)
    {
        MonitorSettings& settings = pair.second;
        if (settings.group_box) settings.enabled = settings.group_box->isChecked();
        if (settings.scale_slider) settings.scale = settings.scale_slider->value() / 100.0f;
        if (settings.softness_slider) settings.edge_softness = (float)settings.softness_slider->value();
        if (settings.blend_slider) settings.blend = (float)settings.blend_slider->value();
        if (settings.edge_zone_slider) settings.edge_zone_depth = settings.edge_zone_slider->value() / 100.0f;
        if (settings.ref_point_combo) settings.reference_point_index = settings.ref_point_combo->currentData().toInt();
    }
}

void ScreenMirror3D::OnScreenPreviewChanged()
{
    if (screen_preview_check)
    {
        bool enabled = screen_preview_check->isChecked();
        emit ScreenPreviewChanged(enabled);
    }
}

/*---------------------------------------------------------*\
| Reference Points Management                              |
\*---------------------------------------------------------*/
void ScreenMirror3D::SetReferencePoints(std::vector<std::unique_ptr<VirtualReferencePoint3D>>* ref_points)
{
    reference_points = ref_points;
    RefreshReferencePointDropdowns();
}

void ScreenMirror3D::RefreshReferencePointDropdowns()
{
    if (!reference_points) return;

    // Update all monitor reference point dropdowns
    for (auto& pair : monitor_settings)
    {
        MonitorSettings& settings = pair.second;
        if (!settings.ref_point_combo) continue;

        // Save current selection
        int current_index = settings.ref_point_combo->currentIndex();
        int current_data = -1;
        if (current_index >= 0)
        {
            current_data = settings.ref_point_combo->currentData().toInt();
        }

        // Rebuild dropdown
        settings.ref_point_combo->blockSignals(true);
        settings.ref_point_combo->clear();

        // Add "Use Global" option
        settings.ref_point_combo->addItem("Use Global Reference", -1);

        // Add all reference points
        for (size_t i = 0; i < reference_points->size(); i++)
        {
            VirtualReferencePoint3D* ref_point = (*reference_points)[i].get();
            if (ref_point)
            {
                QString name = QString::fromStdString(ref_point->GetName());
                settings.ref_point_combo->addItem(name, (int)i);
            }
        }

        // Restore selection
        if (current_data >= -1)
        {
            int restore_index = settings.ref_point_combo->findData(current_data);
            if (restore_index >= 0)
            {
                settings.ref_point_combo->setCurrentIndex(restore_index);
            }
        }

        settings.ref_point_combo->blockSignals(false);
    }
}

void ScreenMirror3D::StartCaptureIfNeeded()
{
    // Get ALL planes and start capture for each one with a capture source
    std::vector<DisplayPlane3D*> planes = DisplayPlaneManager::instance()->GetDisplayPlanes();

    auto& capture_mgr = ScreenCaptureManager::Instance();

    if (!capture_mgr.IsInitialized())
    {
        capture_mgr.Initialize();
    }

    for (auto plane : planes)
    {
        if (!plane) continue;

        std::string capture_id = plane->GetCaptureSourceId();
        if (capture_id.empty()) continue;

        if (!capture_mgr.IsCapturing(capture_id))
        {
            capture_mgr.StartCapture(capture_id);
            LOG_WARNING("[ScreenMirror3D] Started capture for '%s' (plane: %s)",
                       capture_id.c_str(), plane->GetName().c_str());
        }
    }
}

void ScreenMirror3D::StopCaptureIfNeeded()
{
    // We could stop capture here, but better to leave it running
    // in case other effects or instances are using it
}
