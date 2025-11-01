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
#include <QSignalBlocker>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <array>
#include <limits>

ScreenMirror3D::ScreenMirror3D(QWidget* parent)
    : SpatialEffect3D(parent)
    , global_scale_slider(nullptr)
    , smoothing_time_slider(nullptr)
    , brightness_slider(nullptr)
    , propagation_speed_slider(nullptr)
    , wave_decay_slider(nullptr)
    , test_pattern_check(nullptr)
    , screen_preview_check(nullptr)
    , global_scale_invert_check(nullptr)
    
    , global_scale(1.0f) // Default 100% global scale
    , smoothing_time_ms(50.0f)
    , brightness_multiplier(1.0f)
    , propagation_speed_mm_per_ms(20.0f)
    , wave_decay_ms(250.0f)
    , show_test_pattern(false)
    , reference_points(nullptr)
    , global_reference_point_index(-1)
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
    info.effect_type            = SPATIAL_EFFECT_WAVE;
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
    info.show_scale_control     = false;

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

    QLabel* info_label = new QLabel("Uses every active display plane automatically.");
    info_label->setWordWrap(true);
    info_label->setStyleSheet("QLabel { color: #888; font-style: italic; }");
    status_layout->addWidget(info_label);

    // Show active planes count
    std::vector<DisplayPlane3D*> planes = DisplayPlaneManager::instance()->GetDisplayPlanes();
    int active_count = 0;
    for (unsigned int plane_index = 0; plane_index < planes.size(); plane_index++)
    {
        DisplayPlane3D* plane = planes[plane_index];
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
    QGroupBox* monitors_container = new QGroupBox("Per-Monitor Balance");
    QVBoxLayout* monitors_layout = new QVBoxLayout();
    monitors_layout->setSpacing(6);

    // Create expandable settings group for each monitor
    for (unsigned int plane_index = 0; plane_index < planes.size(); plane_index++)
    {
        DisplayPlane3D* plane = planes[plane_index];
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
            settings.group_box->setToolTip("Enable or disable this monitor's influence.");
            connect(settings.group_box, &QGroupBox::toggled, this, &ScreenMirror3D::OnParameterChanged);

            QFormLayout* monitor_form = new QFormLayout();
            monitor_form->setContentsMargins(8, 4, 8, 4);

            settings.scale_slider = new QSlider(Qt::Horizontal);
            settings.scale_slider->setRange(0, 200);
            settings.scale_slider->setValue((int)(settings.scale * 100));
            settings.scale_slider->setTickPosition(QSlider::TicksBelow);
            settings.scale_slider->setTickInterval(25);
            settings.scale_slider->setToolTip("Per-monitor brightness reach (0% to 200%).");
            connect(settings.scale_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
            monitor_form->addRow("Scale:", settings.scale_slider);

            settings.ref_point_combo = new QComboBox();
            settings.ref_point_combo->addItem("Room Center", -1);
            settings.ref_point_combo->setToolTip("Anchor for falloff distance.");
            connect(settings.ref_point_combo, SIGNAL(currentIndexChanged(int)), this, SLOT(OnParameterChanged()));
            monitor_form->addRow("Reference:", settings.ref_point_combo);

            settings.softness_slider = new QSlider(Qt::Horizontal);
            settings.softness_slider->setRange(0, 100);
            settings.softness_slider->setValue((int)settings.edge_softness);
            settings.softness_slider->setTickPosition(QSlider::TicksBelow);
            settings.softness_slider->setTickInterval(10);
            settings.softness_slider->setToolTip("Edge feathering (0 = hard, 100 = very soft).");
            connect(settings.softness_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
            monitor_form->addRow("Softness:", settings.softness_slider);

            settings.blend_slider = new QSlider(Qt::Horizontal);
            settings.blend_slider->setRange(0, 100);
            settings.blend_slider->setValue((int)settings.blend);
            settings.blend_slider->setTickPosition(QSlider::TicksBelow);
            settings.blend_slider->setTickInterval(10);
            settings.blend_slider->setToolTip("Blend with other monitors (0 = isolated, 100 = fully shared).");
            connect(settings.blend_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
            monitor_form->addRow("Blend:", settings.blend_slider);

            settings.edge_zone_slider = new QSlider(Qt::Horizontal);
            settings.edge_zone_slider->setRange(0, 50);
            settings.edge_zone_slider->setValue((int)std::round(settings.edge_zone_depth * 100.0f));
            settings.edge_zone_slider->setTickPosition(QSlider::TicksBelow);
            settings.edge_zone_slider->setTickInterval(10);
            settings.edge_zone_slider->setToolTip("Sample inside the screen edge (0 = boundary, 50 = half-way).");
            connect(settings.edge_zone_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
            monitor_form->addRow("Edge Zone:", settings.edge_zone_slider);

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

    // Global reach controls
    QGroupBox* global_group = new QGroupBox("Global Reach");
    QFormLayout* global_form = new QFormLayout();

    // Global Scale (0-100 slider, maps to 0-200% reach internally)
    float clamped_scale = std::clamp(global_scale, 0.0f, 2.0f);
    float slider_percent = std::clamp(clamped_scale / 2.0f, 0.0f, 1.0f);
    global_scale_slider = new QSlider(Qt::Horizontal);
    global_scale_slider->setRange(0, 100);
    global_scale_slider->setValue((int)std::lround(slider_percent * 100.0f));
    global_scale_slider->setTickPosition(QSlider::TicksBelow);
    global_scale_slider->setTickInterval(10);
    global_scale_slider->setToolTip("Overall coverage (0 = none, 100 = full room).");
    connect(global_scale_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    global_form->addRow("Scale:", global_scale_slider);
    global_scale_invert_check = new QCheckBox("Collapse toward reference");
    global_scale_invert_check->setToolTip("Unchecked = light grows outward. Checked = light collapses toward the reference point.");
    global_scale_invert_check->setChecked(IsScaleInverted());
    connect(global_scale_invert_check, &QCheckBox::toggled, this, [this](bool checked){
        SetScaleInverted(checked);
        OnParameterChanged();
    });
    global_form->addRow("Mode:", global_scale_invert_check);

    // Propagation speed
    propagation_speed_slider = new QSlider(Qt::Horizontal);
    propagation_speed_slider->setRange(0, 400); // 0.0 - 40.0 mm/ms
    propagation_speed_slider->setValue((int)std::lround(propagation_speed_mm_per_ms * 10.0f));
    propagation_speed_slider->setTickPosition(QSlider::TicksBelow);
    propagation_speed_slider->setTickInterval(40);
    propagation_speed_slider->setToolTip("Delay the wave (0 = instant, higher = slower sweep).");
    connect(propagation_speed_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    global_form->addRow("Propagation:", propagation_speed_slider);

    global_group->setLayout(global_form);
    main_layout->addWidget(global_group);

    // Capture Settings
    QGroupBox* capture_group_box = new QGroupBox("Debug Tools");
    QFormLayout* capture_form = new QFormLayout();

    test_pattern_check = new QCheckBox();
    test_pattern_check->setChecked(false);
    test_pattern_check->setToolTip("Display a fixed color quadrant pattern on LEDs for calibration.");
    connect(test_pattern_check,
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            &QCheckBox::checkStateChanged,
            this, [this](Qt::CheckState) { OnParameterChanged(); }
#else
            &QCheckBox::stateChanged,
            this, [this](int) { OnParameterChanged(); }
#endif
    );
    capture_form->addRow("Test Pattern:", test_pattern_check);

    screen_preview_check = new QCheckBox();
    screen_preview_check->setChecked(false);
    screen_preview_check->setToolTip("Project the captured image onto the 3D display planes.");
    connect(screen_preview_check,
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            &QCheckBox::checkStateChanged,
            this, [this](Qt::CheckState) { OnScreenPreviewChanged(); }
#else
            &QCheckBox::stateChanged,
            this, [this](int) { OnScreenPreviewChanged(); }
#endif
    );
    capture_form->addRow("Screen Preview:", screen_preview_check);

    capture_group_box->setLayout(capture_form);
    main_layout->addWidget(capture_group_box);

    // Appearance
    QGroupBox* appearance_group = new QGroupBox("Light & Motion");
    QFormLayout* appearance_form = new QFormLayout();

    brightness_slider = new QSlider(Qt::Horizontal);
    brightness_slider->setRange(0, 500);
    brightness_slider->setValue(100); // Default 100 = 1.0 brightness
    brightness_slider->setTickPosition(QSlider::TicksBelow);
    brightness_slider->setTickInterval(50);
    brightness_slider->setToolTip("Overall brightness multiplier.");
    connect(brightness_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    appearance_form->addRow("Intensity:", brightness_slider);

    smoothing_time_slider = new QSlider(Qt::Horizontal);
    smoothing_time_slider->setRange(0, 500);
    smoothing_time_slider->setValue(50); // Default 50ms
    smoothing_time_slider->setTickPosition(QSlider::TicksBelow);
    smoothing_time_slider->setTickInterval(50);
    smoothing_time_slider->setToolTip("Temporal smoothing (0 = crisp, higher = smoother).");
    connect(smoothing_time_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    appearance_form->addRow("Smoothing:", smoothing_time_slider);

    wave_decay_slider = new QSlider(Qt::Horizontal);
    wave_decay_slider->setRange(50, 1000);
    wave_decay_slider->setValue((int)wave_decay_ms);
    wave_decay_slider->setTickPosition(QSlider::TicksBelow);
    wave_decay_slider->setTickInterval(100);
    wave_decay_slider->setToolTip("How long the wave stays bright as it travels.");
    connect(wave_decay_slider, &QSlider::valueChanged, this, &ScreenMirror3D::OnParameterChanged);
    appearance_form->addRow("Wave Decay:", wave_decay_slider);

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
namespace
{
    inline float Smoothstep(float edge0, float edge1, float x)
    {
        if(edge0 == edge1)
        {
            return (x >= edge1) ? 1.0f : 0.0f;
        }
        float t = (x - edge0) / (edge1 - edge0);
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    float ComputeMaxReferenceDistanceMm(const GridContext3D& grid, const Vector3D& reference, float grid_scale_mm)
    {
        std::array<float, 2> xs = {grid.min_x, grid.max_x};
        std::array<float, 2> ys = {grid.min_y, grid.max_y};
        std::array<float, 2> zs = {grid.min_z, grid.max_z};

        float max_distance_sq = 0.0f;
        for(size_t x_index = 0; x_index < xs.size(); x_index++)
        {
            float cx = xs[x_index];
            for(size_t y_index = 0; y_index < ys.size(); y_index++)
            {
                float cy = ys[y_index];
                for(size_t z_index = 0; z_index < zs.size(); z_index++)
                {
                    float cz = zs[z_index];
                    float dx = (cx - reference.x) * grid_scale_mm;
                    float dy = (cy - reference.y) * grid_scale_mm;
                    float dz = (cz - reference.z) * grid_scale_mm;
                    float dist_sq = dx * dx + dy * dy + dz * dz;
                    if(dist_sq > max_distance_sq)
                    {
                        max_distance_sq = dist_sq;
                    }
                }
            }
        }
        if(max_distance_sq <= 0.0f)
        {
            return 0.0f;
        }
        return sqrtf(max_distance_sq);
    }

    float ComputeInvertedShellFalloff(float distance_mm,
                                      float max_distance_mm,
                                      float coverage,
                                      float softness_percent)
    {
        coverage = std::max(0.0f, coverage);
        if(coverage <= 0.0001f || max_distance_mm <= 0.0f)
        {
            return 0.0f;
        }

        // Allow slight over-coverage to flood entire room when sliders exceed 100%
        if(coverage >= 0.999f)
        {
            return 1.0f;
        }

        float normalized_distance = std::clamp(distance_mm / std::max(max_distance_mm, 1.0f), 0.0f, 1.0f);
        float boundary = std::max(0.0f, 1.0f - std::min(coverage, 1.0f));
        if(boundary <= 0.0005f)
        {
            return 1.0f;
        }

        float softness_ratio = std::clamp(softness_percent / 100.0f, 0.0f, 0.95f);
        float feather_band = softness_ratio * 0.5f;
        float fade_start = std::max(0.0f, boundary - feather_band);
        float fade_end = boundary;

        if(normalized_distance <= fade_start)
        {
            return 0.0f;
        }
        if(normalized_distance >= fade_end)
        {
            return 1.0f;
        }
        return Smoothstep(fade_start, fade_end, normalized_distance);
    }
}

RGBColor ScreenMirror3D::CalculateColorGrid(float x, float y, float z, float /*time*/, const GridContext3D& grid)
{
    std::vector<DisplayPlane3D*> all_planes = DisplayPlaneManager::instance()->GetDisplayPlanes();

    if (all_planes.empty())
    {
        return ToRGBColor(0, 0, 0);
    }

    Vector3D led_pos = {x, y, z};
    ScreenCaptureManager& capture_mgr = ScreenCaptureManager::Instance();

    struct MonitorContribution
    {
        DisplayPlane3D* plane;
        Geometry3D::PlaneProjection proj;
        std::shared_ptr<CapturedFrame> frame;
        float weight; // How much this monitor contributes (0-1)
        float blend;  // Blend percentage for this monitor (0-100)
        float delay_ms;
        uint64_t sample_timestamp;
    };

    std::vector<MonitorContribution> contributions;
    Vector3D effect_reference_point;
    bool has_effect_reference = GetEffectReferencePoint(effect_reference_point);
    const Vector3D* base_falloff_ref = has_effect_reference ? &effect_reference_point : &global_reference_point;

    const float grid_scale_mm = 10.0f;
    float base_max_distance_mm = ComputeMaxReferenceDistanceMm(grid, *base_falloff_ref, grid_scale_mm);
    if(base_max_distance_mm <= 0.0f)
    {
        // Fallback to 3m radius if room bounds are unavailable
        base_max_distance_mm = 3000.0f;
    }

    float normalized_scale = std::clamp(global_scale / 2.0f, 0.0f, 1.0f);

    for (size_t plane_index = 0; plane_index < all_planes.size(); plane_index++)
    {
        DisplayPlane3D* plane = all_planes[plane_index];
        if (!plane) continue;

        // Check if this monitor is enabled in the effect settings
        std::string plane_name = plane->GetName();
        std::map<std::string, MonitorSettings>::iterator settings_it = monitor_settings.find(plane_name);
        if (settings_it == monitor_settings.end())
        {
            settings_it = monitor_settings.emplace(plane_name, MonitorSettings()).first;
        }

        MonitorSettings& mon_settings = settings_it->second;
        bool monitor_enabled = mon_settings.enabled;
        if(mon_settings.group_box)
        {
            monitor_enabled = mon_settings.group_box->isChecked();
        }
        if(!monitor_enabled)
        {
            continue;
        }

        // In test pattern mode, we don't need capture sources - skip those checks
        std::string capture_id = plane->GetCaptureSourceId();
        std::shared_ptr<CapturedFrame> frame = nullptr;

        if (!show_test_pattern)
        {
            // Normal mode: need valid capture source and frames
            if (capture_id.empty())
            {
                continue;
            }

            // Check if capture is actually running and has frames
            if (!capture_mgr.IsCapturing(capture_id))
            {
                capture_mgr.StartCapture(capture_id);
                if (!capture_mgr.IsCapturing(capture_id))
                {
                    continue;
                }
            }

            frame = capture_mgr.GetLatestFrame(capture_id);
            if (!frame || !frame->valid || frame->data.empty())
            {
                continue;
            }

            AddFrameToHistory(capture_id, frame);
        }

        const Vector3D* falloff_ref = base_falloff_ref;
        if (mon_settings.reference_point_index >= 0 && reference_points &&
            mon_settings.reference_point_index < (int)reference_points->size())
        {
            static Vector3D custom_ref;
            if (ResolveReferencePoint(mon_settings.reference_point_index, custom_ref))
            {
                falloff_ref = &custom_ref;
            }
        }

        float reference_max_distance_mm = base_max_distance_mm;
        if(falloff_ref != base_falloff_ref)
        {
            reference_max_distance_mm = ComputeMaxReferenceDistanceMm(grid, *falloff_ref, grid_scale_mm);
            if(reference_max_distance_mm <= 0.0f)
            {
                reference_max_distance_mm = base_max_distance_mm;
            }
        }

        Geometry3D::PlaneProjection proj = Geometry3D::SpatialMapToScreen(led_pos, *plane, mon_settings.edge_zone_depth, falloff_ref, grid_scale_mm);

        if (!proj.is_valid) continue;

        float monitor_scale = std::clamp(mon_settings.scale, 0.0f, 2.0f);
        float coverage = normalized_scale * monitor_scale;
        float distance_falloff = 0.0f;

        if(IsScaleInverted())
        {
            if(coverage > 0.0001f)
            {
                float effective_range = reference_max_distance_mm * coverage;
                effective_range = std::max(effective_range, 10.0f);
                distance_falloff = Geometry3D::ComputeFalloff(proj.distance, effective_range, mon_settings.edge_softness);
            }
        }
        else
        {
            distance_falloff = ComputeInvertedShellFalloff(proj.distance, reference_max_distance_mm, coverage, mon_settings.edge_softness);

            // Allow over-scaling ( >1 ) to fully illuminate room
            if(coverage >= 1.0f && distance_falloff < 1.0f)
            {
                distance_falloff = std::max(distance_falloff, std::min(coverage - 0.99f, 1.0f));
            }
        }

        float delay_ms = 0.0f;
        if (propagation_speed_mm_per_ms > 0.001f)
        {
            delay_ms = proj.distance / propagation_speed_mm_per_ms;
        }

        std::shared_ptr<CapturedFrame> sampling_frame = frame;
        if (!show_test_pattern && !capture_id.empty())
        {
            std::shared_ptr<CapturedFrame> delayed = GetFrameForDelay(capture_id, delay_ms);
            if (delayed)
            {
                sampling_frame = delayed;
            }
        }

        float wave_envelope = 1.0f;
        if (propagation_speed_mm_per_ms > 0.001f && wave_decay_ms > 0.1f)
        {
            wave_envelope = std::exp(-delay_ms / wave_decay_ms);
        }

        float weight = distance_falloff * wave_envelope;

        if (weight > 0.01f)
        {
            MonitorContribution contrib;
            contrib.plane = plane;
            contrib.proj = proj;
            contrib.frame = sampling_frame;
            contrib.weight = weight;
            contrib.blend = mon_settings.blend;
            contrib.delay_ms = delay_ms;
            contrib.sample_timestamp = sampling_frame ? sampling_frame->timestamp_ms :
                                       (frame ? frame->timestamp_ms : 0);
            contributions.push_back(contrib);
        }
    }

    if (contributions.empty())
    {
        int capturing_count = 0;
        for (size_t plane_index = 0; plane_index < all_planes.size(); plane_index++)
        {
            DisplayPlane3D* plane = all_planes[plane_index];
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

    // Blend monitor contributions
    float avg_blend = 0.0f;
    for (size_t contrib_index = 0; contrib_index < contributions.size(); contrib_index++)
    {
        avg_blend += contributions[contrib_index].blend;
    }
    avg_blend /= fmaxf(1.0f, (float)contributions.size());
    float blend_factor = avg_blend / 100.0f; // Convert to 0-1

    if (blend_factor < 0.01f && contributions.size() > 1)
    {
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

        MonitorContribution strongest = contributions[strongest_idx];
        contributions.clear();
        contributions.push_back(strongest);
    }

    float total_r = 0.0f, total_g = 0.0f, total_b = 0.0f;
    float total_weight = 0.0f;
    uint64_t latest_timestamp = 0;

    for (size_t contrib_index = 0; contrib_index < contributions.size(); contrib_index++)
    {
        const MonitorContribution& contrib = contributions[contrib_index];
        float sample_u = contrib.proj.u;
        float sample_v = contrib.proj.v;

        float r, g, b;

        if (show_test_pattern)
        {
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
                r = 255.0f;
                g = 0.0f;
                b = 0.0f;
            }
            else if (bottom_half && !left_half)
            {
                r = 0.0f;
                g = 255.0f;
                b = 0.0f;
            }
            else if (!bottom_half && !left_half)
            {
                r = 0.0f;
                g = 0.0f;
                b = 255.0f;
            }
            else
            {
                r = 255.0f;
                g = 255.0f;
                b = 0.0f;
            }
        }
        else
        {
            if (!contrib.frame || contrib.frame->data.empty())
            {
                continue;
            }

            float flipped_v = 1.0f - sample_v;

            RGBColor sampled_color = Geometry3D::SampleFrame(
                contrib.frame->data.data(),
                contrib.frame->width,
                contrib.frame->height,
                sample_u,
                flipped_v,
                true
            );

            r = (float)RGBGetRValue(sampled_color);
            g = (float)RGBGetGValue(sampled_color);
            b = (float)RGBGetBValue(sampled_color);
        }

        float adjusted_weight = contrib.weight * (0.5f + 0.5f * blend_factor);

        total_r += r * adjusted_weight;
        total_g += g * adjusted_weight;
        total_b += b * adjusted_weight;
        total_weight += adjusted_weight;

        if (contrib.sample_timestamp > latest_timestamp)
        {
            latest_timestamp = contrib.sample_timestamp;
        }
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

    // Temporal smoothing (EMA per LED) for trailing effect
    if (smoothing_time_ms > 0.1f)
    {
        LEDKey key = MakeLEDKey(x, y, z);
        LEDState& state = led_states[key];

        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        uint64_t now_ms = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        uint64_t sample_time_ms = latest_timestamp ? latest_timestamp : now_ms;

        if (state.last_update_ms == 0)
        {
            state.r = total_r;
            state.g = total_g;
            state.b = total_b;
            state.last_update_ms = sample_time_ms;
        }
        else
        {
            uint64_t dt_ms_u64 = (sample_time_ms > state.last_update_ms) ? (sample_time_ms - state.last_update_ms) : 0;
            if (dt_ms_u64 == 0)
            {
                dt_ms_u64 = 16; // assume ~60 FPS
            }
            float dt = (float)dt_ms_u64;
            float tau = smoothing_time_ms;
            float alpha = dt / (tau + dt);

            state.r += alpha * (total_r - state.r);
            state.g += alpha * (total_g - state.g);
            state.b += alpha * (total_b - state.b);
            state.last_update_ms = sample_time_ms;

            total_r = state.r;
            total_g = state.g;
            total_b = state.b;
        }
    }
    else if (!led_states.empty())
    {
        led_states.clear();
    }

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
    settings["global_reference_point_index"] = global_reference_point_index;
    settings["propagation_speed_mm_per_ms"] = propagation_speed_mm_per_ms;
    settings["wave_decay_ms"] = wave_decay_ms;
    settings["scale_inverted"] = IsScaleInverted();

    // Save per-monitor settings
    nlohmann::json monitors = nlohmann::json::object();
    for (std::map<std::string, MonitorSettings>::const_iterator it = monitor_settings.begin();
         it != monitor_settings.end();
         ++it)
    {
        const MonitorSettings& settings = it->second;
        nlohmann::json mon;
        mon["enabled"] = settings.enabled;
        mon["scale"] = settings.scale;
        mon["edge_softness"] = settings.edge_softness;
        mon["blend"] = settings.blend;
        mon["edge_zone_depth"] = settings.edge_zone_depth;
        mon["reference_point_index"] = settings.reference_point_index;
        monitors[it->first] = mon;
    }
    settings["monitor_settings"] = monitors;

    return settings;
}

void ScreenMirror3D::LoadSettings(const nlohmann::json& settings)
{
    // Load global settings
    if (settings.contains("global_scale"))
    {
        global_scale = settings["global_scale"].get<float>();
    }
    // Legacy safety: if value stored as 0-200 integer, normalise back to 0-2 range
    if (global_scale > 2.0f && global_scale <= 400.0f)
    {
        global_scale = global_scale / 100.0f;
    }
    global_scale = std::clamp(global_scale, 0.0f, 2.0f);

    if (settings.contains("smoothing_time_ms"))
        smoothing_time_ms = settings["smoothing_time_ms"].get<float>();

    if (settings.contains("brightness_multiplier"))
        brightness_multiplier = settings["brightness_multiplier"].get<float>();

    if (settings.contains("show_test_pattern"))
        show_test_pattern = settings["show_test_pattern"].get<bool>();

    if (settings.contains("global_reference_point_index"))
        global_reference_point_index = settings["global_reference_point_index"].get<int>();

    if (settings.contains("propagation_speed_mm_per_ms"))
        propagation_speed_mm_per_ms = settings["propagation_speed_mm_per_ms"].get<float>();

    if (settings.contains("wave_decay_ms"))
        wave_decay_ms = settings["wave_decay_ms"].get<float>();

    bool invert_flag = IsScaleInverted();
    if (settings.contains("scale_inverted"))
    {
        invert_flag = settings["scale_inverted"].get<bool>();
    }

    // Load per-monitor settings
    if (settings.contains("monitor_settings"))
    {
        const nlohmann::json& monitors = settings["monitor_settings"];
        for (nlohmann::json::const_iterator it = monitors.begin(); it != monitors.end(); ++it)
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

            msettings.scale = std::clamp(msettings.scale, 0.0f, 2.0f);
            msettings.edge_softness = std::clamp(msettings.edge_softness, 0.0f, 100.0f);
            msettings.blend = std::clamp(msettings.blend, 0.0f, 100.0f);
            msettings.edge_zone_depth = std::clamp(msettings.edge_zone_depth, 0.0f, 0.5f);
        }
    }

    // Update UI - Global (convert float to slider values)
    if (global_scale_slider)
    {
        QSignalBlocker blocker(global_scale_slider);
        float clamped = std::clamp(global_scale, 0.0f, 2.0f);
        float slider_percent = (clamped / 2.0f) * 100.0f;
        global_scale_slider->setValue((int)std::lround(slider_percent));
    }
    if (global_scale_invert_check)
    {
        QSignalBlocker blocker(global_scale_invert_check);
        global_scale_invert_check->setChecked(invert_flag);
    }
    if (smoothing_time_slider)
    {
        QSignalBlocker blocker(smoothing_time_slider);
        smoothing_time_slider->setValue((int)std::lround(smoothing_time_ms));
    }
    if (brightness_slider)
    {
        QSignalBlocker blocker(brightness_slider);
        brightness_slider->setValue((int)std::lround(brightness_multiplier * 100.0f));
    }
    if (propagation_speed_slider)
    {
        QSignalBlocker blocker(propagation_speed_slider);
        propagation_speed_slider->setValue((int)std::lround(propagation_speed_mm_per_ms * 10.0f));
    }
    if (wave_decay_slider)
    {
        QSignalBlocker blocker(wave_decay_slider);
        wave_decay_slider->setValue((int)std::lround(wave_decay_ms));
    }
    if (test_pattern_check)
    {
        QSignalBlocker blocker(test_pattern_check);
        test_pattern_check->setChecked(show_test_pattern);
    }

    // Update monitor UI widgets to match loaded state
    for (std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
         it != monitor_settings.end();
         ++it)
    {
        MonitorSettings& msettings = it->second;
        if (msettings.group_box)
        {
            QSignalBlocker blocker(msettings.group_box);
            msettings.group_box->setChecked(msettings.enabled);
        }
        if (msettings.scale_slider)
        {
            QSignalBlocker blocker(msettings.scale_slider);
            msettings.scale_slider->setValue((int)std::lround(msettings.scale * 100.0f));
        }
        if (msettings.softness_slider)
        {
            QSignalBlocker blocker(msettings.softness_slider);
            msettings.softness_slider->setValue((int)std::lround(msettings.edge_softness));
        }
        if (msettings.blend_slider)
        {
            QSignalBlocker blocker(msettings.blend_slider);
            msettings.blend_slider->setValue((int)std::lround(msettings.blend));
        }
        if (msettings.edge_zone_slider)
        {
            QSignalBlocker blocker(msettings.edge_zone_slider);
            msettings.edge_zone_slider->setValue((int)std::lround(msettings.edge_zone_depth * 100.0f));
        }
        if (msettings.ref_point_combo)
        {
            QSignalBlocker blocker(msettings.ref_point_combo);
            int desired = msettings.reference_point_index;
            int idx = msettings.ref_point_combo->findData(desired);
            if (idx < 0)
            {
                idx = msettings.ref_point_combo->findData(-1);
            }
            if (idx >= 0)
            {
                msettings.ref_point_combo->setCurrentIndex(idx);
            }
        }
    }

    // Ensure reference point menus reflect updated selections
    RefreshReferencePointDropdowns();

    // Apply inversion flag after UI sync and recalc internal state
    SetScaleInverted(invert_flag);
    OnParameterChanged();
}

void ScreenMirror3D::OnParameterChanged()
{
    // Update global settings (convert slider values to float)
    if (global_scale_slider)
    {
        float slider_norm = std::clamp(global_scale_slider->value() / 100.0f, 0.0f, 1.0f);
        global_scale = std::clamp(slider_norm * 2.0f, 0.0f, 2.0f);
    }
    if (global_scale_invert_check && global_scale_invert_check->isChecked() != IsScaleInverted())
    {
        QSignalBlocker blocker(global_scale_invert_check);
        global_scale_invert_check->setChecked(IsScaleInverted());
    }
    if (smoothing_time_slider) smoothing_time_ms = (float)smoothing_time_slider->value();
    if (brightness_slider) brightness_multiplier = brightness_slider->value() / 100.0f;
    if (propagation_speed_slider) propagation_speed_mm_per_ms = propagation_speed_slider->value() / 10.0f;
    if (wave_decay_slider) wave_decay_ms = (float)wave_decay_slider->value();
    if (test_pattern_check) show_test_pattern = test_pattern_check->isChecked();

    // Update per-monitor settings (convert slider values to float)
    for (std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
         it != monitor_settings.end();
         ++it)
    {
        MonitorSettings& settings = it->second;
        if (settings.group_box) settings.enabled = settings.group_box->isChecked();
        if (settings.scale_slider) settings.scale = settings.scale_slider->value() / 100.0f;
        if (settings.softness_slider) settings.edge_softness = (float)settings.softness_slider->value();
        if (settings.blend_slider) settings.blend = (float)settings.blend_slider->value();
        if (settings.edge_zone_slider) settings.edge_zone_depth = settings.edge_zone_slider->value() / 100.0f;
        if (settings.ref_point_combo) settings.reference_point_index = settings.ref_point_combo->currentData().toInt();
    }

    emit ParametersChanged();
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
    for (std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
         it != monitor_settings.end();
         ++it)
    {
        MonitorSettings& settings = it->second;
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

        // Add "Room Center" option (uses calculated room center, not a reference point)
        settings.ref_point_combo->addItem("Room Center", -1);

        // Add all reference points
        for (size_t i = 0; i < reference_points->size(); i++)
        {
            VirtualReferencePoint3D* ref_point = (*reference_points)[i].get();
            if (ref_point)
            {
                QString name = QString::fromStdString(ref_point->GetName());
                QString type = QString(VirtualReferencePoint3D::GetTypeName(ref_point->GetType()));
                QString display = QString("%1 (%2)").arg(name).arg(type);
                settings.ref_point_combo->addItem(display, (int)i);
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

    if(reference_points)
    {
        if(global_reference_point_index >= (int)reference_points->size())
        {
            global_reference_point_index = -1;
        }
    }
    else
    {
        global_reference_point_index = -1;
    }
}

bool ScreenMirror3D::ResolveReferencePoint(int index, Vector3D& out) const
{
    if(!reference_points || index < 0 || index >= (int)reference_points->size())
    {
        return false;
    }

    VirtualReferencePoint3D* ref_point = (*reference_points)[index].get();
    if(!ref_point)
    {
        return false;
    }

    out = ref_point->GetPosition();
    return true;
}

bool ScreenMirror3D::GetEffectReferencePoint(Vector3D& out) const
{
    return ResolveReferencePoint(global_reference_point_index, out);
}

void ScreenMirror3D::AddFrameToHistory(const std::string& capture_id, const std::shared_ptr<CapturedFrame>& frame)
{
    if(capture_id.empty() || !frame || !frame->valid)
    {
        return;
    }

    FrameHistory& history = capture_history[capture_id];
    if(!history.frames.empty() && history.frames.back()->frame_id == frame->frame_id)
    {
        return;
    }

    history.frames.push_back(frame);

    uint64_t retention_ms = (uint64_t)GetHistoryRetentionMs();
    uint64_t cutoff = (frame->timestamp_ms > retention_ms) ? frame->timestamp_ms - retention_ms : 0;

    while(history.frames.size() > 1 && history.frames.front()->timestamp_ms < cutoff)
    {
        history.frames.pop_front();
    }

    const size_t max_frames = 180; // ~3 seconds at 60fps
    if(history.frames.size() > max_frames)
    {
        history.frames.pop_front();
    }
}

std::shared_ptr<CapturedFrame> ScreenMirror3D::GetFrameForDelay(const std::string& capture_id, float delay_ms) const
{
    std::unordered_map<std::string, FrameHistory>::const_iterator history_it = capture_history.find(capture_id);
    if(history_it == capture_history.end() || history_it->second.frames.empty())
    {
        return nullptr;
    }

    const std::deque<std::shared_ptr<CapturedFrame>>& frames = history_it->second.frames;
    if(delay_ms <= 0.0f)
    {
        return frames.back();
    }

    uint64_t latest_timestamp = frames.back()->timestamp_ms;
    uint64_t delay_u64 = delay_ms >= (float)std::numeric_limits<uint64_t>::max() ? latest_timestamp : (uint64_t)delay_ms;
    uint64_t target_timestamp = (latest_timestamp > delay_u64) ? latest_timestamp - delay_u64 : 0;

    for(std::deque<std::shared_ptr<CapturedFrame>>::const_reverse_iterator frame_it = frames.rbegin();
        frame_it != frames.rend();
        ++frame_it)
    {
        if((*frame_it)->timestamp_ms <= target_timestamp)
        {
            return *frame_it;
        }
    }

    return frames.front();
}

float ScreenMirror3D::GetHistoryRetentionMs() const
{
    float retention = std::max(wave_decay_ms * 3.0f, smoothing_time_ms * 3.0f);
    if(propagation_speed_mm_per_ms > 0.001f)
    {
        // ensure we can cover longer distances (up to ~3m default)
        float max_distance_mm = 4000.0f;
        retention = std::max(retention, max_distance_mm / propagation_speed_mm_per_ms);
    }
    return std::max(retention, 600.0f);
}

ScreenMirror3D::LEDKey ScreenMirror3D::MakeLEDKey(float x, float y, float z) const
{
    const float quantize_scale = 1000.0f; // Preserve millimeter precision
    LEDKey key;
    key.x = (int)std::lround(x * quantize_scale);
    key.y = (int)std::lround(y * quantize_scale);
    key.z = (int)std::lround(z * quantize_scale);
    return key;
}

void ScreenMirror3D::StartCaptureIfNeeded()
{
    // Get ALL planes and start capture for each one with a capture source
    std::vector<DisplayPlane3D*> planes = DisplayPlaneManager::instance()->GetDisplayPlanes();

    ScreenCaptureManager& capture_mgr = ScreenCaptureManager::Instance();

    if (!capture_mgr.IsInitialized())
    {
        capture_mgr.Initialize();
    }

    for (size_t plane_index = 0; plane_index < planes.size(); plane_index++)
    {
        DisplayPlane3D* plane = planes[plane_index];
        if (!plane) continue;

        std::string capture_id = plane->GetCaptureSourceId();
        if (capture_id.empty()) continue;

        if (!capture_mgr.IsCapturing(capture_id))
        {
            capture_mgr.StartCapture(capture_id);
            LOG_INFO("[ScreenMirror3D] Started capture for '%s' (plane: %s)",
                       capture_id.c_str(), plane->GetName().c_str());
        }
    }
}

void ScreenMirror3D::StopCaptureIfNeeded()
{
    // We could stop capture here, but better to leave it running
    // in case other effects or instances are using it
}
