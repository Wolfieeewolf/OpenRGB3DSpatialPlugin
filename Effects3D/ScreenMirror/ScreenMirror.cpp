// SPDX-License-Identifier: GPL-2.0-only

#include "ScreenMirror.h"
#include "ScreenCaptureManager.h"
#include "DisplayPlane3D.h"
#include "DisplayPlaneManager.h"
#include "Geometry3DUtils.h"
#include "GridSpaceUtils.h"
#include "VirtualReferencePoint3D.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginUiUtils.h"
#include "ScreenMirror/ScreenMirrorCalibrationPattern.h"

REGISTER_EFFECT_3D(ScreenMirror);

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QChar>
#include <QFormLayout>
#include <QSlider>
#include <QTimer>
#include <QDateTime>
#include <QSignalBlocker>
#include <QFont>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPushButton>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <array>
#include <limits>
#include <functional>
#include <mutex>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace
{
const uint8_t* GetCalibrationPatternBuffer(int& out_w, int& out_h)
{
    static std::vector<uint8_t> buffer;
    static std::once_flag init_once;
    std::call_once(init_once, []() { ScreenMirrorFillCalibrationPatternBuffer(buffer); });
    out_w = kScreenMirrorCalibrationPatternW;
    out_h = kScreenMirrorCalibrationPatternH;
    return buffer.data();
}

/** Slider integer = degrees × this (one slider step = 0.5°). */
constexpr int kScreenMapRollTicksPerDegree = 2;
constexpr int kScreenMapRollSliderMax = 180 * kScreenMapRollTicksPerDegree;
inline float RadialMapUiToInternal(int ui_0_100)
{
    return (float)std::clamp(ui_0_100, 0, 100) - 50.0f;
}

bool CaptureSourceIdIsPrimary(const std::string& source_id)
{
    if(source_id.empty())
    {
        return false;
    }
    ScreenCaptureManager& mgr = ScreenCaptureManager::Instance();
    if(!mgr.IsInitialized())
    {
        return false;
    }
    for(const CaptureSourceInfo& info : mgr.GetAvailableSources())
    {
        if(info.id == source_id)
        {
            return info.is_primary;
        }
    }
    return false;
}

/** Primary capture wins; if none marked, first plane in manager order. */
bool DefaultMonitorEnabledForPlane(DisplayPlane3D* plane)
{
    std::vector<DisplayPlane3D*> planes = DisplayPlaneManager::instance()->GetDisplayPlanes();
    if(!plane || planes.empty())
    {
        return true;
    }
    DisplayPlane3D* primary_plane = nullptr;
    for(DisplayPlane3D* p : planes)
    {
        if(!p) continue;
        if(CaptureSourceIdIsPrimary(p->GetCaptureSourceId()))
        {
            primary_plane = p;
            break;
        }
    }
    if(primary_plane)
    {
        return plane == primary_plane;
    }
    return plane == planes[0];
}

DisplayPlane3D* FindDisplayPlaneByName(const std::string& name)
{
    for(DisplayPlane3D* p : DisplayPlaneManager::instance()->GetDisplayPlanes())
    {
        if(p && p->GetName() == name)
        {
            return p;
        }
    }
    return nullptr;
}
}

ScreenMirror::ScreenMirror(QWidget* parent)
    : SpatialEffect3D(parent)
    , global_scale_slider(nullptr)
    , global_scale_label(nullptr)
    , smoothing_time_slider(nullptr)
    , smoothing_time_label(nullptr)
    , brightness_slider(nullptr)
    , brightness_label(nullptr)
    , propagation_speed_slider(nullptr)
    , propagation_speed_label(nullptr)
    , wave_decay_slider(nullptr)
    , wave_decay_label(nullptr)
    , brightness_threshold_slider(nullptr)
    , brightness_threshold_label(nullptr)
    , global_scale_invert_check(nullptr)
    , monitor_status_label(nullptr)
    , monitor_help_label(nullptr)
    , monitors_container(nullptr)
    , monitors_layout(nullptr)
    , grid_scale_mm_(10.0f)
    , capture_quality(1)
    , capture_quality_combo(nullptr)
    , capture_backend_mode(1)
    , capture_backend_combo(nullptr)
    , show_calibration_pattern(false)
    , in_parameter_change_(false)
    , reference_points(nullptr)
    , frame_cache_refresh_ms_(0)
    , frame_cache_last_render_seq_(0)
{
}

ScreenMirror::~ScreenMirror() = default;

EffectInfo3D ScreenMirror::GetEffectInfo()
{
    EffectInfo3D info           = {};
    info.info_version           = 2;
    info.effect_name            = "Screen Mirror";
    info.effect_description =
        "Maps screen content onto LEDs in 3D space. Output shaping → Sampling coarsens LED color sampling (retro pixel look).";
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
    
    info.show_color_controls    = false;
    info.show_speed_control     = false;
    info.show_brightness_control = false;
    info.show_frequency_control = false;
    info.show_size_control      = false;
    info.show_scale_control     = false;
    info.show_axis_control      = false;

    return info;
}

void ScreenMirror::SetupCustomUI(QWidget* parent)
{
    capture_quality_combo = nullptr;
    capture_backend_combo = nullptr;
    monitor_status_label = nullptr;
    monitor_help_label = nullptr;
    monitors_container = nullptr;
    monitors_layout = nullptr;
    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        MonitorSettings& s = it->second;
        s.group_box = nullptr;
        s.scale_slider = nullptr;
        s.scale_label = nullptr;
        s.scale_invert_check = nullptr;
        s.smoothing_time_slider = nullptr;
        s.smoothing_time_label = nullptr;
        s.brightness_slider = nullptr;
        s.brightness_label = nullptr;
        s.brightness_threshold_slider = nullptr;
        s.brightness_threshold_label = nullptr;
        s.white_rolloff_slider = nullptr;
        s.white_rolloff_label = nullptr;
        s.vibrance_slider = nullptr;
        s.vibrance_label = nullptr;
        s.black_bar_letterbox_slider = nullptr;
        s.black_bar_letterbox_label = nullptr;
        s.black_bar_pillarbox_slider = nullptr;
        s.black_bar_pillarbox_label = nullptr;
        s.softness_slider = nullptr;
        s.softness_label = nullptr;
        s.blend_slider = nullptr;
        s.blend_label = nullptr;
        s.propagation_speed_slider = nullptr;
        s.propagation_speed_label = nullptr;
        s.wave_decay_slider = nullptr;
        s.wave_decay_label = nullptr;
        s.wave_time_to_edge_slider = nullptr;
        s.wave_time_to_edge_label = nullptr;
        s.falloff_curve_slider = nullptr;
        s.falloff_curve_label = nullptr;
        s.front_back_balance_slider = nullptr;
        s.front_back_balance_label = nullptr;
        s.left_right_balance_slider = nullptr;
        s.left_right_balance_label = nullptr;
        s.top_bottom_balance_slider = nullptr;
        s.top_bottom_balance_label = nullptr;
        s.ref_point_combo = nullptr;
        s.calibration_pattern_check = nullptr;
        s.screen_preview_check = nullptr;
        s.capture_area_preview = nullptr;
        s.add_zone_button = nullptr;
        s.capture_zones_widget = nullptr;
        s.screen_map_roll_slider = nullptr;
        s.screen_map_roll_label = nullptr;
        s.radial_corner_expansion_slider = nullptr;
        s.radial_corner_expansion_label = nullptr;
        s.radial_corner_bias_tl_slider = nullptr;
        s.radial_corner_bias_tl_label = nullptr;
        s.radial_corner_bias_tr_slider = nullptr;
        s.radial_corner_bias_tr_label = nullptr;
        s.radial_corner_bias_bl_slider = nullptr;
        s.radial_corner_bias_bl_label = nullptr;
        s.radial_corner_bias_br_slider = nullptr;
        s.radial_corner_bias_br_label = nullptr;
        s.corner_blend_strength_slider = nullptr;
        s.corner_blend_strength_label = nullptr;
        s.corner_blend_zone_slider = nullptr;
        s.corner_blend_zone_label = nullptr;
    }

    if(rotation_yaw_slider)
    {
        QWidget* rotation_group = rotation_yaw_slider->parentWidget();
        while(rotation_group && !qobject_cast<QGroupBox*>(rotation_group))
        {
            rotation_group = rotation_group->parentWidget();
        }
        if(rotation_group && rotation_group != effect_controls_group)
        {
            rotation_group->setVisible(false);
        }
    }
    if(intensity_slider)
    {
        QWidget* intensity_widget = intensity_slider->parentWidget();
        if(intensity_widget && intensity_widget != effect_controls_group)
        {
            intensity_widget->setVisible(false);
        }
    }
    if(sharpness_slider)
    {
        QWidget* sharpness_widget = sharpness_slider->parentWidget();
        if(sharpness_widget && sharpness_widget != effect_controls_group)
        {
            sharpness_widget->setVisible(false);
        }
    }
    
    QWidget* container = new QWidget();
    QVBoxLayout* main_layout = new QVBoxLayout(container);
    QGroupBox* status_group = new QGroupBox("Multi-Monitor Status");
    QVBoxLayout* status_layout = new QVBoxLayout();

    QLabel* info_label = new QLabel("Uses every active display plane automatically.");
    info_label->setWordWrap(true);
    PluginUiApplyItalicSecondaryLabel(info_label);
    status_layout->addWidget(info_label);
    monitor_status_label = new QLabel("Calculating...");
    {
        QFont sf = monitor_status_label->font();
        sf.setBold(true);
        sf.setPointSizeF(sf.pointSizeF() * 1.15);
        monitor_status_label->setFont(sf);
    }
    status_layout->addWidget(monitor_status_label);
    monitor_help_label = nullptr;

    status_group->setLayout(status_layout);
    main_layout->addWidget(status_group);
    QGroupBox* capture_group = new QGroupBox("Capture Quality");
    QVBoxLayout* capture_outer = new QVBoxLayout();
    QHBoxLayout* capture_layout = new QHBoxLayout();
    QLabel* capture_quality_label = new QLabel("Resolution:");
    capture_quality_combo = new QComboBox();
    capture_quality_combo->addItem("Low (320×180)", QVariant(0));
    capture_quality_combo->addItem("Medium (480×270)", QVariant(1));
    capture_quality_combo->addItem("High (640×360)", QVariant(2));
    capture_quality_combo->addItem("Ultra (960×540)", QVariant(3));
    capture_quality_combo->addItem("Maximum (1280×720)", QVariant(4));
    capture_quality_combo->addItem("1080p (1920×1080)", QVariant(5));
    capture_quality_combo->addItem("1440p (2560×1440)", QVariant(6));
    capture_quality_combo->addItem("4K (3840×2160)", QVariant(7));
    capture_quality_combo->setCurrentIndex(std::clamp(capture_quality, 0, 7));
    capture_quality_combo->setToolTip("Downscaled capture size before color extraction. Higher = sharper, heavier CPU/GPU/RAM.");
    capture_quality_combo->setItemData(0, "Lightest load; fine for small planes or testing.", Qt::ToolTipRole);
    capture_quality_combo->setItemData(1, "Low bandwidth; acceptable on integrated GPUs.", Qt::ToolTipRole);
    capture_quality_combo->setItemData(2, "Balanced default for many setups.", Qt::ToolTipRole);
    capture_quality_combo->setItemData(3, "Sharper color detail on wide monitors.", Qt::ToolTipRole);
    capture_quality_combo->setItemData(4, "720p capture; use when GPU headroom is comfortable.", Qt::ToolTipRole);
    capture_quality_combo->setItemData(5, "1080p; noticeable cost—prefer on discrete GPUs.", Qt::ToolTipRole);
    capture_quality_combo->setItemData(6, "1440p; high cost.", Qt::ToolTipRole);
    capture_quality_combo->setItemData(7, "4K; only if you need maximum edge fidelity.", Qt::ToolTipRole);
    capture_layout->addWidget(capture_quality_label);
    capture_layout->addWidget(capture_quality_combo, 1);
    capture_outer->addLayout(capture_layout);

    QHBoxLayout* backend_layout = new QHBoxLayout();
    QLabel* capture_backend_label = new QLabel("Capture API:");
    capture_backend_combo = new QComboBox();
    capture_backend_combo->addItem("Auto (GDI if DXGI stalls)", QVariant(0));
    capture_backend_combo->addItem("DXGI only", QVariant(1));
    capture_backend_combo->addItem("GDI only", QVariant(2));
    capture_backend_combo->setCurrentIndex(std::clamp(capture_backend_mode, 0, 2));
    capture_backend_combo->setToolTip(
        "Windows: Auto uses GDI when DXGI stalls (static pages). Lock DXGI or GDI to reduce API-mix flicker. "
        "DXGI-only skips BitBlt (idle desktop may barely update). No effect on Linux/macOS.");
    backend_layout->addWidget(capture_backend_label);
    backend_layout->addWidget(capture_backend_combo, 1);
    capture_outer->addLayout(backend_layout);

    capture_group->setLayout(capture_outer);
    main_layout->addWidget(capture_group);

    ScreenCaptureManager::Instance().SetWindowsCaptureBackendMode(capture_backend_mode);
    connect(capture_backend_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        capture_backend_mode = std::clamp(index, 0, 2);
        ScreenCaptureManager::Instance().SetWindowsCaptureBackendMode(capture_backend_mode);
        OnParameterChanged();
    });
    connect(capture_quality_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        capture_quality = std::clamp(index, 0, 7);
        int w = 320, h = 180;
        if(capture_quality == 1) { w = 480; h = 270; }
        else if(capture_quality == 2) { w = 640; h = 360; }
        else if(capture_quality == 3) { w = 960; h = 540; }
        else if(capture_quality == 4) { w = 1280; h = 720; }
        else if(capture_quality == 5) { w = 1920; h = 1080; }
        else if(capture_quality == 6) { w = 2560; h = 1440; }
        else if(capture_quality == 7) { w = 3840; h = 2160; }
        ScreenCaptureManager::Instance().SetDownscaleResolution(w, h);
        OnParameterChanged();
    });

    monitors_container = new QGroupBox("Per-Monitor Balance");
    monitors_layout = new QVBoxLayout();
    monitors_layout->setSpacing(6);

    std::vector<DisplayPlane3D*> planes = DisplayPlaneManager::instance()->GetDisplayPlanes();
    for(unsigned int plane_index = 0; plane_index < planes.size(); plane_index++)
    {
        DisplayPlane3D* plane = planes[plane_index];
        if(!plane) continue;

        std::string plane_name = plane->GetName();

        std::map<std::string, MonitorSettings>::iterator settings_it = monitor_settings.find(plane_name);
        if(settings_it == monitor_settings.end())
        {
            MonitorSettings new_settings;
            int plane_ref_id = LookupReferencePointIdByIndex(plane->GetReferencePointIndex());
            if(plane_ref_id > 0)
            {
                new_settings.reference_point_id = plane_ref_id;
            }
            new_settings.enabled = DefaultMonitorEnabledForPlane(plane);
            settings_it = monitor_settings.emplace(plane_name, new_settings).first;
        }
        MonitorSettings& settings = settings_it->second;
        if(settings.reference_point_id <= 0)
        {
            int plane_ref_id = LookupReferencePointIdByIndex(plane->GetReferencePointIndex());
            if(plane_ref_id > 0)
            {
                settings.reference_point_id = plane_ref_id;
            }
        }

        if(!settings.group_box)
        {
            CreateMonitorSettingsUI(plane, settings);
        }
    }

    if(monitor_settings.empty())
    {
        QLabel* no_monitors_label = new QLabel("No monitors configured. Set up Display Planes first.");
        PluginUiApplyItalicSecondaryLabel(no_monitors_label);
        monitors_layout->addWidget(no_monitors_label);
    }

    monitors_container->setLayout(monitors_layout);
    main_layout->addWidget(monitors_container);

    RefreshMonitorStatus();

    main_layout->addStretch();

    AddWidgetToParent(container, parent);
}

void ScreenMirror::UpdateParams(SpatialEffectParams& /*params*/)
{
}


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

    /** Near image corners, blend center UV with vertical-edge and horizontal-edge samples (HyperHDR-style 2D idea in UV space). */
    inline RGBColor SampleFrameWithCornerBlend(const uint8_t* frame_data,
                                               int frame_w,
                                               int frame_h,
                                               float u_s,
                                               float v_s,
                                               float u_min,
                                               float u_max,
                                               float v_min,
                                               float v_max,
                                               unsigned int samp,
                                               float corner_strength_01,
                                               float corner_zone_01)
    {
        if(!frame_data || frame_w <= 0 || frame_h <= 0)
        {
            return ToRGBColor(0, 0, 0);
        }

        auto quant = [&](float& u, float& v) {
            if(samp < 100u)
            {
                Geometry3D::QuantizeMediaUV01(u, v, frame_w, frame_h, samp);
            }
        };

        const float strength = std::clamp(corner_strength_01, 0.0f, 1.0f);
        if(strength <= 0.004f)
        {
            float um = u_s, vm = v_s;
            quant(um, vm);
            return Geometry3D::SampleFrame(frame_data, frame_w, frame_h, um, vm, true);
        }

        const float zone = std::clamp(corner_zone_01, 0.0f, 0.32f);
        const float span_u = std::max(1e-6f, u_max - u_min);
        const float span_v = std::max(1e-6f, v_max - v_min);
        const float u_n = std::clamp((u_s - u_min) / span_u, 0.0f, 1.0f);
        const float v_n = std::clamp((v_s - v_min) / span_v, 0.0f, 1.0f);
        const float edge_u = std::min(u_n, 1.0f - u_n);
        const float edge_v = std::min(v_n, 1.0f - v_n);
        const float near_u = 1.0f - Smoothstep(0.0f, zone, edge_u);
        const float near_v = 1.0f - Smoothstep(0.0f, zone, edge_v);
        const float corner_w = std::min(near_u, near_v);

        float um = u_s, vm = v_s;
        quant(um, vm);
        RGBColor c_mid = Geometry3D::SampleFrame(frame_data, frame_w, frame_h, um, vm, true);
        if(corner_w <= 0.004f)
        {
            return c_mid;
        }

        const float w = corner_w * strength;
        if(w <= 0.004f)
        {
            return c_mid;
        }

        const float snap_u = (u_n < 0.5f) ? u_min : u_max;
        const float snap_v = (v_n < 0.5f) ? v_min : v_max;

        float uv_u = snap_u, uv_v = v_s;
        quant(uv_u, uv_v);
        RGBColor c_vert = Geometry3D::SampleFrame(frame_data, frame_w, frame_h, uv_u, uv_v, true);

        float uh_u = u_s, uh_v = snap_v;
        quant(uh_u, uh_v);
        RGBColor c_horiz = Geometry3D::SampleFrame(frame_data, frame_w, frame_h, uh_u, uh_v, true);

        const float r =
            (1.0f - w) * (float)RGBGetRValue(c_mid) + w * 0.5f * ((float)RGBGetRValue(c_vert) + (float)RGBGetRValue(c_horiz));
        const float g =
            (1.0f - w) * (float)RGBGetGValue(c_mid) + w * 0.5f * ((float)RGBGetGValue(c_vert) + (float)RGBGetGValue(c_horiz));
        const float b =
            (1.0f - w) * (float)RGBGetBValue(c_mid) + w * 0.5f * ((float)RGBGetBValue(c_vert) + (float)RGBGetBValue(c_horiz));
        return ToRGBColor((uint8_t)std::clamp((int)std::lround(r), 0, 255),
                          (uint8_t)std::clamp((int)std::lround(g), 0, 255),
                          (uint8_t)std::clamp((int)std::lround(b), 0, 255));
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
                    float dx = GridUnitsToMM(cx - reference.x, grid_scale_mm);
                    float dy = GridUnitsToMM(cy - reference.y, grid_scale_mm);
                    float dz = GridUnitsToMM(cz - reference.z, grid_scale_mm);
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

    float WaveIntensityToSpeedMmPerMs(float intensity_0_to_100)
    {
        if(intensity_0_to_100 < 0.5f) return 0.0f;
        float p = std::clamp(intensity_0_to_100, 1.0f, 100.0f) / 100.0f;
        float speed = 200.0f * powf(0.01f, p);
        return std::clamp(speed, 0.5f, 500.0f);
    }

    float ComputeInvertedShellFalloff(float distance_mm,
                                      float max_distance_mm,
                                      float coverage,
                                      float softness_percent,
                                      float curve_exponent = 1.0f)
    {
        coverage = std::max(0.0f, coverage);
        if(coverage <= 0.0001f || max_distance_mm <= 0.0f)
        {
            return 0.0f;
        }

        if(coverage >= 0.999f)
        {
            return 1.0f;
        }

        float normalized_distance = std::clamp(distance_mm / std::max(max_distance_mm, 1.0f), 0.0f, 1.0f);
        float exp_val = std::clamp(curve_exponent, 0.25f, 4.0f);
        normalized_distance = powf(normalized_distance, exp_val);
        float boundary = std::max(0.0f, 1.0f - coverage);
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

void ScreenMirror::RefreshFrameCacheForRenderSequence(const GridContext3D& grid)
{
    const uint64_t now_ms = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    bool need_frame_cache_refresh = false;
    if(grid.render_sequence != 0)
    {
        if(grid.render_sequence != frame_cache_last_render_seq_)
        {
            need_frame_cache_refresh = true;
            frame_cache_last_render_seq_ = grid.render_sequence;
        }
    }
    else
    {
        const uint64_t cache_max_age_ms = 8;
        if(frame_cache_refresh_ms_ == 0 || (now_ms - frame_cache_refresh_ms_) >= cache_max_age_ms)
        {
            need_frame_cache_refresh = true;
        }
    }

    if(!need_frame_cache_refresh)
    {
        return;
    }

    frame_cache_planes_ = DisplayPlaneManager::instance()->GetDisplayPlanes();
    std::unordered_set<std::string> seen_capture_ids;
    ScreenCaptureManager& capture_mgr = ScreenCaptureManager::Instance();
    if(!capture_mgr.IsInitialized())
    {
        capture_mgr.Initialize();
    }
    capture_mgr.SetTargetFPS(120);
    int cap_w = 320, cap_h = 180;
    int q = std::clamp(capture_quality, 0, 7);
    if(q == 1) { cap_w = 480; cap_h = 270; }
    else if(q == 2) { cap_w = 640; cap_h = 360; }
    else if(q == 3) { cap_w = 960; cap_h = 540; }
    else if(q == 4) { cap_w = 1280; cap_h = 720; }
    else if(q == 5) { cap_w = 1920; cap_h = 1080; }
    else if(q == 6) { cap_w = 2560; cap_h = 1440; }
    else if(q == 7) { cap_w = 3840; cap_h = 2160; }
    capture_mgr.SetDownscaleResolution(cap_w, cap_h);
    for(size_t i = 0; i < frame_cache_planes_.size(); i++)
    {
        DisplayPlane3D* plane = frame_cache_planes_[i];
        if(!plane) continue;
        std::string capture_id = plane->GetCaptureSourceId();
        if(capture_id.empty()) continue;
        seen_capture_ids.insert(capture_id);
        if(!capture_mgr.IsCapturing(capture_id))
        {
            capture_mgr.StartCapture(capture_id);
        }
        std::shared_ptr<CapturedFrame> frame = capture_mgr.GetLatestFrame(capture_id);
        if(frame && frame->valid && !frame->data.empty())
        {
            frame_cache_[capture_id] = frame;
            AddFrameToHistory(capture_id, frame);
        }
    }
    for(std::unordered_map<std::string, std::shared_ptr<CapturedFrame>>::iterator it = frame_cache_.begin();
        it != frame_cache_.end(); )
    {
        if(seen_capture_ids.find(it->first) == seen_capture_ids.end())
        {
            it = frame_cache_.erase(it);
        }
        else
        {
            ++it;
        }
    }
    frame_cache_refresh_ms_ = now_ms;
}

RGBColor ScreenMirror::CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid)
{
    RefreshFrameCacheForRenderSequence(grid);
    return CalculateColorGridInternal(x, y, z, time, grid, &frame_cache_, &frame_cache_planes_);
}

RGBColor ScreenMirror::CalculateColorGridInternal(float x, float y, float z, float time, const GridContext3D& grid,
                                                     const std::unordered_map<std::string, std::shared_ptr<CapturedFrame>>* frame_cache,
                                                     const std::vector<DisplayPlane3D*>* pre_fetched_planes,
                                                     bool apply_led_smoothing)
{
    (void)time;
    std::vector<DisplayPlane3D*> all_planes;
    if(pre_fetched_planes)
        all_planes = *pre_fetched_planes;
    else
        all_planes = DisplayPlaneManager::instance()->GetDisplayPlanes();

    if(all_planes.empty())
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
        std::shared_ptr<CapturedFrame> frame_blend;
        float blend_t;
        float weight;
        float blend;
        float brightness_multiplier;
        float brightness_threshold;
        float white_rolloff;
        float vibrance;
        float black_bar_letterbox_percent;
        float black_bar_pillarbox_percent;
        float smoothing_time_ms;
        bool use_calibration_pattern;
        float corner_blend_strength_01;
        float corner_blend_zone_01;
    };

    std::vector<MonitorContribution> contributions;
    contributions.reserve(all_planes.size());
    Vector3D grid_anchor_ref = GetReferencePointGrid(grid);

    const float scale_mm = (grid.grid_scale_mm > 0.001f) ? grid.grid_scale_mm : 10.0f;
    float base_max_distance_mm = ComputeMaxReferenceDistanceMm(grid, grid_anchor_ref, scale_mm);
    if(base_max_distance_mm <= 0.0f)
    {
        base_max_distance_mm = 3000.0f;
    }

    std::map<std::string, std::unordered_map<std::string, FrameHistory>::iterator> history_cache;

    for(size_t plane_index = 0; plane_index < all_planes.size(); plane_index++)
    {
        DisplayPlane3D* plane = all_planes[plane_index];
        if(!plane) continue;

        std::string plane_name = plane->GetName();
        std::map<std::string, MonitorSettings>::iterator settings_it = monitor_settings.find(plane_name);
        if(settings_it == monitor_settings.end())
        {
            settings_it = monitor_settings.emplace(plane_name, MonitorSettings()).first;
            settings_it->second.enabled = DefaultMonitorEnabledForPlane(plane);
            if(settings_it->second.capture_zones.empty())
            {
                settings_it->second.capture_zones.push_back(CaptureZone(0.0f, 1.0f, 0.0f, 1.0f));
            }
        }

        MonitorSettings& mon_settings = settings_it->second;
        
        if(mon_settings.capture_zones.empty())
        {
            mon_settings.capture_zones.push_back(CaptureZone(0.0f, 1.0f, 0.0f, 1.0f));
        }
        bool has_enabled_zone = false;
        for(size_t zone_idx = 0; zone_idx < mon_settings.capture_zones.size(); zone_idx++)
        {
            if(mon_settings.capture_zones[zone_idx].enabled)
            {
                has_enabled_zone = true;
                break;
            }
        }
        if(!has_enabled_zone && !mon_settings.capture_zones.empty())
        {
            mon_settings.capture_zones[0].enabled = true;
        }
        
        bool monitor_enabled = mon_settings.group_box ? mon_settings.group_box->isChecked() : mon_settings.enabled;
        if(!monitor_enabled)
        {
            continue;
        }

        bool monitor_calibration_pattern = mon_settings.show_calibration_pattern;
        
        std::string capture_id = plane->GetCaptureSourceId();
        std::shared_ptr<CapturedFrame> frame = nullptr;

        if(!monitor_calibration_pattern)
        {
            if(capture_id.empty())
            {
                continue;
            }
            if(frame_cache)
            {
                std::unordered_map<std::string, std::shared_ptr<CapturedFrame>>::const_iterator it =
                    frame_cache->find(capture_id);
                frame = (it != frame_cache->end()) ? it->second : nullptr;
            }
            else
            {
                if(!capture_mgr.IsCapturing(capture_id))
                {
                    capture_mgr.StartCapture(capture_id);
                    if(!capture_mgr.IsCapturing(capture_id))
                    {
                        continue;
                    }
                }
                frame = capture_mgr.GetLatestFrame(capture_id);
                if(!frame || !frame->valid || frame->data.empty())
                {
                    continue;
                }
                AddFrameToHistory(capture_id, frame);
            }
            if(!frame || !frame->valid || frame->data.empty())
            {
                continue;
            }
        }

        const Vector3D* falloff_ref = &grid_anchor_ref;
        Vector3D custom_ref;
        if(mon_settings.reference_point_id > 0 &&
           ResolveReferencePointById(mon_settings.reference_point_id, custom_ref))
        {
            falloff_ref = &custom_ref;
        }

        float reference_max_distance_mm = base_max_distance_mm;
        if(falloff_ref != &grid_anchor_ref)
        {
            reference_max_distance_mm = ComputeMaxReferenceDistanceMm(grid, *falloff_ref, scale_mm);
            if(reference_max_distance_mm <= 0.0f)
            {
                reference_max_distance_mm = base_max_distance_mm;
            }
        }

        Geometry3D::PlaneProjection proj =
            Geometry3D::SpatialMapToScreen(led_pos, *plane, 0.0f, falloff_ref, scale_mm);

        if(!proj.is_valid) continue;

        float u = proj.u;
        float v = proj.v;
        Geometry3D::ApplyRadialCornerMapping01(u, v,
                                               RadialMapUiToInternal(mon_settings.radial_corner_expansion_ui),
                                               RadialMapUiToInternal(mon_settings.radial_corner_bias_tl_ui),
                                               RadialMapUiToInternal(mon_settings.radial_corner_bias_tr_ui),
                                               RadialMapUiToInternal(mon_settings.radial_corner_bias_bl_ui),
                                               RadialMapUiToInternal(mon_settings.radial_corner_bias_br_ui));
        
        if(mon_settings.capture_zones.empty())
        {
            mon_settings.capture_zones.push_back(CaptureZone(0.0f, 1.0f, 0.0f, 1.0f));
        }
        
        bool in_zone = false;
        for(size_t zone_idx = 0; zone_idx < mon_settings.capture_zones.size(); zone_idx++)
        {
            const CaptureZone& zone = mon_settings.capture_zones[zone_idx];
            if(zone.Contains(u, v))
            {
                in_zone = true;
                break;
            }
        }
        
        if(!in_zone)
        {
            continue;
        }
        
        u = std::clamp(u, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);

        Geometry3D::ApplyUVRotationDegrees01(u, v, mon_settings.screen_map_roll_deg);

        proj.u = u;
        proj.v = v;

        float monitor_scale = std::clamp(mon_settings.scale, 0.0f, 3.0f);
        float coverage = monitor_scale;
        float curve_exp = std::clamp(mon_settings.falloff_curve_exponent, 0.5f, 2.0f);
        float distance_falloff = 0.0f;
        const float edge_softness = mon_settings.edge_softness;

        if(mon_settings.scale_inverted)
        {
            if(coverage > 0.0001f)
            {
                float effective_range = reference_max_distance_mm * coverage;
                effective_range = std::max(effective_range, 10.0f);
                distance_falloff = Geometry3D::ComputeFalloff(proj.distance, effective_range, edge_softness);
            }
        }
        else
        {
            distance_falloff = ComputeInvertedShellFalloff(proj.distance, reference_max_distance_mm, coverage, edge_softness, curve_exp);

            if(coverage >= 1.0f && distance_falloff < 1.0f)
            {
                distance_falloff = std::max(distance_falloff, std::min(coverage - 0.99f, 1.0f));
            }
        }

        std::shared_ptr<CapturedFrame> sampling_frame = frame;
        std::shared_ptr<CapturedFrame> sampling_frame_blend;
        float sampling_blend_t = 0.0f;
        float delay_ms = 0.0f;
        float speed_mm_per_ms = 0.0f;
        bool use_wave = !monitor_calibration_pattern && !capture_id.empty();
        if(use_wave && mon_settings.wave_time_to_edge_sec > 0.4f)
        {
            float t_sec = std::clamp(mon_settings.wave_time_to_edge_sec, 0.5f, 10.0f);
            speed_mm_per_ms = reference_max_distance_mm / (t_sec * 1000.0f);
            speed_mm_per_ms = std::max(speed_mm_per_ms, 0.1f);
            delay_ms = proj.distance / speed_mm_per_ms;
            delay_ms = std::clamp(delay_ms, 0.0f, 60000.0f);
        }
        else if(use_wave && mon_settings.propagation_speed_mm_per_ms >= 5.0f)
        {
            speed_mm_per_ms = WaveIntensityToSpeedMmPerMs(mon_settings.propagation_speed_mm_per_ms);
            delay_ms = proj.distance / std::max(speed_mm_per_ms, 0.5f);
            delay_ms = std::clamp(delay_ms, 0.0f, 15000.0f);
        }

        if(use_wave && (mon_settings.wave_time_to_edge_sec > 0.4f || mon_settings.propagation_speed_mm_per_ms >= 5.0f))
        {
            std::unordered_map<std::string, FrameHistory>::iterator history_it = capture_history.end();
            std::map<std::string, std::unordered_map<std::string, FrameHistory>::iterator>::iterator cache_it = history_cache.find(capture_id);
            if(cache_it != history_cache.end())
            {
                history_it = cache_it->second;
            }
            else
            {
                history_it = capture_history.find(capture_id);
                if(history_it != capture_history.end())
                {
                    history_cache[capture_id] = history_it;
                }
            }
            
            if(history_it != capture_history.end() && history_it->second.frames.size() >= 2)
            {
                FrameHistory& history = history_it->second;
                const std::deque<std::shared_ptr<CapturedFrame>>& frames = history.frames;
                
                float avg_frame_time_ms = history.cached_avg_frame_time_ms > 0.0f
                    ? history.cached_avg_frame_time_ms : 16.67f;
                uint64_t latest_timestamp = frames.back()->timestamp_ms;
                const uint64_t frame_rate_stale_ms = 200;

                if(history.last_frame_rate_update == 0 ||
                   (latest_timestamp - history.last_frame_rate_update) > frame_rate_stale_ms)
                {
                    if(frames.size() >= 2)
                    {
                        size_t check_frames = std::min(frames.size() - 1, (size_t)10);
                        uint64_t total_time = 0;
                        size_t valid_pairs = 0;
                        const uint64_t min_delta_ms = 8;
                        const uint64_t max_delta_ms = 80;

                        for(size_t i = frames.size() - check_frames; i < frames.size(); i++)
                        {
                            if(i > 0 && i < frames.size())
                            {
                                uint64_t frame_time = frames[i]->timestamp_ms;
                                uint64_t prev_time = frames[i-1]->timestamp_ms;
                                uint64_t delta = (frame_time > prev_time) ? (frame_time - prev_time) : 0;
                                if(delta >= min_delta_ms && delta <= max_delta_ms)
                                {
                                    total_time += delta;
                                    valid_pairs++;
                                }
                            }
                        }

                        if(valid_pairs > 0 && total_time > 0)
                        {
                            float measured_ms = (float)total_time / (float)valid_pairs;
                            measured_ms = std::clamp(measured_ms, 12.0f, 50.0f);
                            if(history.cached_avg_frame_time_ms > 0.0f)
                                avg_frame_time_ms = 0.75f * history.cached_avg_frame_time_ms + 0.25f * measured_ms;
                            else
                                avg_frame_time_ms = measured_ms;
                        }
                    }
                    history.cached_avg_frame_time_ms = avg_frame_time_ms;
                    history.last_frame_rate_update = latest_timestamp;
                }
                else
                {
                    avg_frame_time_ms = history.cached_avg_frame_time_ms;
                }

                float frame_offset_f = delay_ms / std::max(avg_frame_time_ms, 1.0f);
                frame_offset_f = std::max(0.0f, frame_offset_f);
                int frame_offset_int = (int)(frame_offset_f + 0.5f);
                frame_offset_int = std::max(0, frame_offset_int);

                if(frame_offset_int < (int)frames.size())
                {
                    size_t frame_index_lo = frames.size() - 1 - (size_t)frame_offset_int;
                    float frac = frame_offset_f - std::floor(frame_offset_f);
                    if(frame_index_lo < frames.size())
                    {
                        sampling_frame = frames[frame_index_lo];
                        if(frac > 0.01f && frame_index_lo + 1 < frames.size())
                        {
                            sampling_frame_blend = frames[frame_index_lo + 1];
                            sampling_blend_t = frac;
                        }
                    }
                }
            }
            else
            {
            }
        }

        float wave_envelope = 1.0f;
        if((mon_settings.wave_time_to_edge_sec > 0.4f || mon_settings.propagation_speed_mm_per_ms >= 5.0f) && mon_settings.wave_decay_ms > 0.1f)
        {
            if(delay_ms <= 0.0f && use_wave)
            {
                if(mon_settings.wave_time_to_edge_sec > 0.4f)
                {
                    float t_sec = std::clamp(mon_settings.wave_time_to_edge_sec, 0.5f, 10.0f);
                    speed_mm_per_ms = reference_max_distance_mm / (t_sec * 1000.0f);
                    delay_ms = proj.distance / std::max(speed_mm_per_ms, 0.1f);
                }
                else
                {
                    speed_mm_per_ms = WaveIntensityToSpeedMmPerMs(mon_settings.propagation_speed_mm_per_ms);
                    delay_ms = proj.distance / std::max(speed_mm_per_ms, 0.5f);
                }
                delay_ms = std::clamp(delay_ms, 0.0f, 60000.0f);
            }
            wave_envelope = std::exp(-delay_ms / std::max(mon_settings.wave_decay_ms, 0.1f));
        }

        float weight = distance_falloff * wave_envelope;

        float ref_max_units = reference_max_distance_mm / std::max(scale_mm, 0.001f);
        if(ref_max_units > 0.001f && (std::fabs(mon_settings.front_back_balance) > 0.5f || std::fabs(mon_settings.left_right_balance) > 0.5f || std::fabs(mon_settings.top_bottom_balance) > 0.5f))
        {
            Vector3D ref_to_led = { led_pos.x - falloff_ref->x, led_pos.y - falloff_ref->y, led_pos.z - falloff_ref->z };
            const Transform3D& transform = plane->GetTransform();
            float rot[9];
            Geometry3D::ComputeRotationMatrix(transform.rotation, rot);
            Vector3D plane_right  = { rot[0], rot[3], rot[6] };
            Vector3D plane_up     = { rot[1], rot[4], rot[7] };
            Vector3D plane_normal = { rot[2], rot[5], rot[8] };
            float lateral = ref_to_led.x * plane_right.x + ref_to_led.y * plane_right.y + ref_to_led.z * plane_right.z;
            float vertical = ref_to_led.x * plane_up.x + ref_to_led.y * plane_up.y + ref_to_led.z * plane_up.z;
            float depth = ref_to_led.x * plane_normal.x + ref_to_led.y * plane_normal.y + ref_to_led.z * plane_normal.z;
            float lat_norm = std::clamp(lateral / ref_max_units, -1.0f, 1.0f);
            float vert_norm = std::clamp(vertical / ref_max_units, -1.0f, 1.0f);
            float depth_norm = std::clamp(depth / ref_max_units, -1.0f, 1.0f);
            float dir_fb = std::clamp(1.0f + (mon_settings.front_back_balance / 100.0f) * depth_norm, 0.0f, 2.0f);
            float dir_lr = std::clamp(1.0f + (mon_settings.left_right_balance / 100.0f) * lat_norm, 0.0f, 2.0f);
            float dir_tb = std::clamp(1.0f + (mon_settings.top_bottom_balance / 100.0f) * vert_norm, 0.0f, 2.0f);
            weight *= dir_fb * dir_lr * dir_tb;
        }

        if(weight > 0.01f)
        {
            MonitorContribution contrib;
            contrib.plane = plane;
            contrib.proj = proj;
            contrib.frame = sampling_frame;
            contrib.frame_blend = sampling_frame_blend;
            contrib.blend_t = sampling_blend_t;
            contrib.weight = weight;
            contrib.blend = mon_settings.blend;
            contrib.brightness_multiplier = mon_settings.brightness_multiplier;
            contrib.brightness_threshold = mon_settings.brightness_threshold;
            contrib.white_rolloff = mon_settings.white_rolloff;
            contrib.vibrance = mon_settings.vibrance;
            contrib.black_bar_letterbox_percent = mon_settings.black_bar_letterbox_percent;
            contrib.black_bar_pillarbox_percent = mon_settings.black_bar_pillarbox_percent;
            contrib.smoothing_time_ms = mon_settings.smoothing_time_ms;
            contrib.use_calibration_pattern = mon_settings.show_calibration_pattern;
            contrib.corner_blend_strength_01 =
                std::clamp(mon_settings.corner_blend_strength_pct / 100.0f, 0.0f, 1.0f);
            contrib.corner_blend_zone_01 =
                std::clamp(mon_settings.corner_blend_zone_pct / 100.0f, 0.0f, 0.32f);
            contributions.push_back(contrib);
        }
    }

    if(contributions.empty())
    {
        if(show_calibration_pattern)
        {
            return ToRGBColor(0, 0, 0);
        }
        
        int capturing_count = 0;
        for(size_t plane_index = 0; plane_index < all_planes.size(); plane_index++)
        {
            DisplayPlane3D* plane = all_planes[plane_index];
            if(plane && !plane->GetCaptureSourceId().empty())
            {
                if(capture_mgr.IsCapturing(plane->GetCaptureSourceId()))
                {
                    capturing_count++;
                }
            }
        }

        if(capturing_count > 0)
        {
            return ToRGBColor(0, 0, 0);
        }
        else
        {
            return ToRGBColor(128, 0, 128);
        }
    }

    float avg_blend = 0.0f;
    for(size_t contrib_index = 0; contrib_index < contributions.size(); contrib_index++)
    {
        avg_blend += contributions[contrib_index].blend;
    }
    avg_blend /= (float)contributions.size();
    float blend_factor = avg_blend / 100.0f;

    if(blend_factor < 0.01f && contributions.size() > 1)
    {
        size_t strongest_idx = 0;
        float max_weight = contributions[0].weight;
        for(size_t i = 1; i < contributions.size(); i++)
        {
            if(contributions[i].weight > max_weight)
            {
                max_weight = contributions[i].weight;
                strongest_idx = i;
            }
        }
        if(strongest_idx != 0)
        {
            contributions[0] = contributions[strongest_idx];
        }
        contributions.resize(1);
    }

    float total_r = 0.0f, total_g = 0.0f, total_b = 0.0f;
    float total_weight = 0.0f;

    for(size_t contrib_index = 0; contrib_index < contributions.size(); contrib_index++)
    {
        MonitorContribution& contrib = contributions[contrib_index];
        float sample_u = contrib.proj.u;
        float sample_v = contrib.proj.v;
        const float corner_strength_01 = contrib.corner_blend_strength_01;
        const float corner_zone_01 = contrib.corner_blend_zone_01;

        float r, g, b;

        if(contrib.use_calibration_pattern)
        {
            int cal_w = 0;
            int cal_h = 0;
            const uint8_t* cal_data = GetCalibrationPatternBuffer(cal_w, cal_h);
            float lp = std::clamp(contrib.black_bar_letterbox_percent, 0.0f, 49.0f) / 100.0f;
            float pp = std::clamp(contrib.black_bar_pillarbox_percent, 0.0f, 49.0f) / 100.0f;
            float u_min = pp;
            float u_max = 1.0f - pp;
            float v_min = lp;
            float v_max = 1.0f - lp;
            float sample_u_clamped = std::clamp(sample_u, u_min, u_max);
            /* Procedural calibration texture uses the same v axis as SpatialMapToScreen (not DXGI row order). */
            float tex_v = std::clamp(sample_v, v_min, v_max);
            const unsigned int samp = GetSamplingResolution();
            RGBColor sampled_cal = SampleFrameWithCornerBlend(cal_data,
                                                              cal_w,
                                                              cal_h,
                                                              sample_u_clamped,
                                                              tex_v,
                                                              u_min,
                                                              u_max,
                                                              v_min,
                                                              v_max,
                                                              samp,
                                                              corner_strength_01,
                                                              corner_zone_01);
            r = (float)RGBGetRValue(sampled_cal);
            g = (float)RGBGetGValue(sampled_cal);
            b = (float)RGBGetBValue(sampled_cal);
        }
        else
        {
            if(!contrib.frame || contrib.frame->data.empty())
            {
                continue;
            }

            float lp = std::clamp(contrib.black_bar_letterbox_percent, 0.0f, 49.0f) / 100.0f;
            float pp = std::clamp(contrib.black_bar_pillarbox_percent, 0.0f, 49.0f) / 100.0f;
            float u_min = pp, u_max = 1.0f - pp;
            float v_min = lp, v_max = 1.0f - lp;
            float sample_u_clamped = std::clamp(sample_u, u_min, u_max);
            float flipped_v = 1.0f - sample_v;
            flipped_v = std::clamp(flipped_v, v_min, v_max);
            const unsigned int samp = GetSamplingResolution();
            const float u_s = sample_u_clamped;
            const float v_s = flipped_v;

            RGBColor sampled_color = SampleFrameWithCornerBlend(contrib.frame->data.data(),
                                                                contrib.frame->width,
                                                                contrib.frame->height,
                                                                u_s,
                                                                v_s,
                                                                u_min,
                                                                u_max,
                                                                v_min,
                                                                v_max,
                                                                samp,
                                                                corner_strength_01,
                                                                corner_zone_01);

            r = (float)RGBGetRValue(sampled_color);
            g = (float)RGBGetGValue(sampled_color);
            b = (float)RGBGetBValue(sampled_color);

            if(contrib.frame_blend && !contrib.frame_blend->data.empty() && contrib.blend_t > 0.01f)
            {
                const float u_b = sample_u_clamped;
                const float v_b = flipped_v;
                RGBColor sampled_blend = SampleFrameWithCornerBlend(contrib.frame_blend->data.data(),
                                                                      contrib.frame_blend->width,
                                                                      contrib.frame_blend->height,
                                                                      u_b,
                                                                      v_b,
                                                                      u_min,
                                                                      u_max,
                                                                      v_min,
                                                                      v_max,
                                                                      samp,
                                                                      corner_strength_01,
                                                                      corner_zone_01);
                float r2 = (float)RGBGetRValue(sampled_blend);
                float g2 = (float)RGBGetGValue(sampled_blend);
                float b2 = (float)RGBGetBValue(sampled_blend);
                float t = std::clamp(contrib.blend_t, 0.0f, 1.0f);
                r = (1.0f - t) * r + t * r2;
                g = (1.0f - t) * g + t * g2;
                b = (1.0f - t) * b + t * b2;
            }

            if(contrib.brightness_threshold > 0.0f)
            {
                float luminance = 0.299f * r + 0.587f * g + 0.114f * b;
                float thr = std::min(255.0f, contrib.brightness_threshold);
                if(luminance <= thr)
                {
                    float t = (thr <= 0.0f) ? 1.0f : std::max(0.0f, luminance / thr);
                    t = t * t;
                    contrib.weight *= t;
                    r *= t;
                    g *= t;
                    b *= t;
                }
            }
        }

        float lum = 0.299f * r + 0.587f * g + 0.114f * b;
        float max_rgb = std::max(r, std::max(g, b));
        float min_rgb = std::min(r, std::min(g, b));
        float sat = (max_rgb > 0.001f) ? ((max_rgb - min_rgb) / max_rgb) : 0.0f;

        if(lum > 235.0f && sat > 0.10f)
        {
            float t = std::clamp((lum - 235.0f) / 20.0f, 0.0f, 1.0f);
            float gray = lum;
            r = (1.0f - t) * r + t * gray;
            g = (1.0f - t) * g + t * gray;
            b = (1.0f - t) * b + t * gray;
        }
        float white_factor = (1.0f - sat) * (lum / 255.0f);
        float white_reduce = std::min(1.0f, contrib.white_rolloff);
        float dampen = 1.0f - white_reduce * white_factor;

        r *= contrib.brightness_multiplier * dampen;
        g *= contrib.brightness_multiplier * dampen;
        b *= contrib.brightness_multiplier * dampen;

        float vib = std::max(0.0f, std::min(2.0f, contrib.vibrance));
        if(std::fabs(vib - 1.0f) > 0.001f)
        {
            float gray = (r + g + b) / 3.0f;
            r = gray + (r - gray) * vib;
            g = gray + (g - gray) * vib;
            b = gray + (b - gray) * vib;
            r = std::max(0.0f, std::min(255.0f, r));
            g = std::max(0.0f, std::min(255.0f, g));
            b = std::max(0.0f, std::min(255.0f, b));
        }

        float adjusted_weight = contrib.weight * (0.5f + 0.5f * blend_factor);

        total_r += r * adjusted_weight;
        total_g += g * adjusted_weight;
        total_b += b * adjusted_weight;
        total_weight += adjusted_weight;
    }

    if(total_weight > 0.0f)
    {
        total_r /= total_weight;
        total_g /= total_weight;
        total_b /= total_weight;
    }


    if(total_r > 255.0f) total_r = 255.0f;
    if(total_g > 255.0f) total_g = 255.0f;
    if(total_b > 255.0f) total_b = 255.0f;

    if(apply_led_smoothing)
    {
        float max_smoothing_time = 0.0f;
        if(contributions.size() == 1)
        {
            max_smoothing_time = contributions[0].smoothing_time_ms;
        }
        else
        {
            for(size_t i = 0; i < contributions.size(); i++)
            {
                if(contributions[i].smoothing_time_ms > max_smoothing_time)
                {
                    max_smoothing_time = contributions[i].smoothing_time_ms;
                }
            }
        }
        
        if(max_smoothing_time > 0.1f)
        {
            LEDKey key = MakeLEDKey(x, y, z);
            LEDState& state = led_states[key];

            static const std::chrono::steady_clock::time_point smooth_clock_start = std::chrono::steady_clock::now();
            std::chrono::steady_clock::time_point now_tp = std::chrono::steady_clock::now();
            uint64_t tick_ms = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
                                     now_tp - smooth_clock_start)
                                     .count();

            if(state.smooth_last_tick_ms == 0)
            {
                state.r = total_r;
                state.g = total_g;
                state.b = total_b;
                state.smooth_last_tick_ms = tick_ms;
            }
            else
            {
                uint64_t dt_ms_u64 = (tick_ms > state.smooth_last_tick_ms) ? (tick_ms - state.smooth_last_tick_ms) : 0;
                if(dt_ms_u64 == 0)
                {
                    dt_ms_u64 = 1;
                }
                float dt = (float)dt_ms_u64;
                float tau = max_smoothing_time;
                float alpha = dt / (tau + dt);

                state.r += alpha * (total_r - state.r);
                state.g += alpha * (total_g - state.g);
                state.b += alpha * (total_b - state.b);
                state.smooth_last_tick_ms = tick_ms;

                total_r = state.r;
                total_g = state.g;
                total_b = state.b;
            }
        }
        else
        {
            LEDKey key = MakeLEDKey(x, y, z);
            led_states.erase(key);
        }
    }

    return ToRGBColor((uint8_t)total_r, (uint8_t)total_g, (uint8_t)total_b);
}

nlohmann::json ScreenMirror::SaveSettings() const
{
    nlohmann::json settings;
    settings["capture_quality"] = std::clamp(capture_quality, 0, 7);
    settings["capture_backend_mode"] = std::clamp(capture_backend_mode, 0, 2);
    nlohmann::json monitors = nlohmann::json::object();
    for(std::map<std::string, MonitorSettings>::const_iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        const MonitorSettings& mon_settings = it->second;
        nlohmann::json mon;
        mon["enabled"] = mon_settings.enabled;
        
        mon["scale"] = mon_settings.scale;
        mon["scale_inverted"] = mon_settings.scale_inverted;
        
        mon["smoothing_time_ms"] = mon_settings.smoothing_time_ms;
        mon["brightness_multiplier"] = mon_settings.brightness_multiplier;
        mon["brightness_threshold"] = mon_settings.brightness_threshold;
        mon["white_rolloff"] = mon_settings.white_rolloff;
        mon["vibrance"] = mon_settings.vibrance;
        mon["black_bar_letterbox_percent"] = mon_settings.black_bar_letterbox_percent;
        mon["black_bar_pillarbox_percent"] = mon_settings.black_bar_pillarbox_percent;
        
        mon["edge_softness"] = mon_settings.edge_softness;
        mon["blend"] = mon_settings.blend;
        mon["propagation_speed_mm_per_ms"] = mon_settings.propagation_speed_mm_per_ms;
        mon["wave_decay_ms"] = mon_settings.wave_decay_ms;
        mon["wave_time_to_edge_sec"] = mon_settings.wave_time_to_edge_sec;
        mon["falloff_curve_exponent"] = mon_settings.falloff_curve_exponent;
        mon["front_back_balance"] = mon_settings.front_back_balance;
        mon["left_right_balance"] = mon_settings.left_right_balance;
        mon["top_bottom_balance"] = mon_settings.top_bottom_balance;
        
        mon["reference_point_id"] = mon_settings.reference_point_id;
        mon["show_calibration_pattern"] = mon_settings.show_calibration_pattern;
        mon["show_screen_preview"] = mon_settings.show_screen_preview;

        mon["radial_map_roll_deg"] = std::clamp(mon_settings.screen_map_roll_deg, -180.0f, 180.0f);
        mon["radial_map_expansion"] = std::clamp(mon_settings.radial_corner_expansion_ui, 0, 100);
        mon["radial_map_bias_tl"] = std::clamp(mon_settings.radial_corner_bias_tl_ui, 0, 100);
        mon["radial_map_bias_tr"] = std::clamp(mon_settings.radial_corner_bias_tr_ui, 0, 100);
        mon["radial_map_bias_bl"] = std::clamp(mon_settings.radial_corner_bias_bl_ui, 0, 100);
        mon["radial_map_bias_br"] = std::clamp(mon_settings.radial_corner_bias_br_ui, 0, 100);
        mon["sample_corner_blend_strength_pct"] = std::clamp(mon_settings.corner_blend_strength_pct, 0.0f, 100.0f);
        mon["sample_corner_blend_zone_pct"] = std::clamp(mon_settings.corner_blend_zone_pct, 0.0f, 32.0f);
        
        nlohmann::json zones_array = nlohmann::json::array();
        for(const CaptureZone& zone : mon_settings.capture_zones)
        {
            nlohmann::json zone_json;
            zone_json["u_min"] = zone.u_min;
            zone_json["u_max"] = zone.u_max;
            zone_json["v_min"] = zone.v_min;
            zone_json["v_max"] = zone.v_max;
            zone_json["enabled"] = zone.enabled;
            zone_json["name"] = zone.name;
            zones_array.push_back(zone_json);
        }
        mon["capture_zones"] = zones_array;
        monitors[it->first] = mon;
    }
    settings["monitor_settings"] = monitors;

    return settings;
}

void ScreenMirror::SyncMonitorSettingsToUI(MonitorSettings& msettings)
{
    if(msettings.group_box)
    {
        QSignalBlocker blocker(msettings.group_box);
        msettings.group_box->setChecked(msettings.enabled);
    }
    if(msettings.scale_slider)
    {
        QSignalBlocker blocker(msettings.scale_slider);
        msettings.scale_slider->setValue((int)std::lround(msettings.scale * 100.0f));
    }
    if(msettings.scale_label)
    {
        msettings.scale_label->setText(QString::number((int)std::lround(msettings.scale * 100.0f)) + "%");
    }
    if(msettings.scale_invert_check)
    {
        QSignalBlocker blocker(msettings.scale_invert_check);
        msettings.scale_invert_check->setChecked(msettings.scale_inverted);
    }
    if(msettings.smoothing_time_slider)
    {
        QSignalBlocker blocker(msettings.smoothing_time_slider);
        msettings.smoothing_time_slider->setValue((int)std::lround(msettings.smoothing_time_ms));
    }
    if(msettings.smoothing_time_label)
    {
        msettings.smoothing_time_label->setText(QString::number((int)msettings.smoothing_time_ms) + "ms");
    }
    if(msettings.brightness_slider)
    {
        QSignalBlocker blocker(msettings.brightness_slider);
        msettings.brightness_slider->setValue((int)std::lround(msettings.brightness_multiplier * 100.0f));
    }
    if(msettings.brightness_label)
    {
        msettings.brightness_label->setText(QString::number((int)std::lround(msettings.brightness_multiplier * 100.0f)) + "%");
    }
    if(msettings.brightness_threshold_slider)
    {
        QSignalBlocker blocker(msettings.brightness_threshold_slider);
        msettings.brightness_threshold_slider->setValue((int)msettings.brightness_threshold);
    }
    if(msettings.brightness_threshold_label)
    {
        msettings.brightness_threshold_label->setText(QString::number((int)msettings.brightness_threshold));
    }
    if(msettings.white_rolloff_slider)
    {
        QSignalBlocker blocker(msettings.white_rolloff_slider);
        msettings.white_rolloff_slider->setValue((int)std::lround(msettings.white_rolloff * 100.0f));
    }
    if(msettings.white_rolloff_label)
    {
        msettings.white_rolloff_label->setText(QString::number((int)std::lround(msettings.white_rolloff * 100.0f)) + "%");
    }
    if(msettings.vibrance_slider)
    {
        QSignalBlocker blocker(msettings.vibrance_slider);
        msettings.vibrance_slider->setValue((int)std::lround(msettings.vibrance * 100.0f));
    }
    if(msettings.vibrance_label)
    {
        msettings.vibrance_label->setText(QString::number((int)std::lround(msettings.vibrance * 100.0f)) + "%");
    }
    if(msettings.black_bar_letterbox_slider)
    {
        QSignalBlocker blocker(msettings.black_bar_letterbox_slider);
        msettings.black_bar_letterbox_slider->setValue((int)std::lround(msettings.black_bar_letterbox_percent));
    }
    if(msettings.black_bar_letterbox_label)
    {
        msettings.black_bar_letterbox_label->setText(QString::number((int)std::lround(msettings.black_bar_letterbox_percent)));
    }
    if(msettings.black_bar_pillarbox_slider)
    {
        QSignalBlocker blocker(msettings.black_bar_pillarbox_slider);
        msettings.black_bar_pillarbox_slider->setValue((int)std::lround(msettings.black_bar_pillarbox_percent));
    }
    if(msettings.black_bar_pillarbox_label)
    {
        msettings.black_bar_pillarbox_label->setText(QString::number((int)std::lround(msettings.black_bar_pillarbox_percent)));
    }
    if(msettings.softness_slider)
    {
        QSignalBlocker blocker(msettings.softness_slider);
        msettings.softness_slider->setValue((int)std::lround(msettings.edge_softness));
    }
    if(msettings.softness_label)
    {
        msettings.softness_label->setText(QString::number((int)msettings.edge_softness));
    }
    if(msettings.blend_slider)
    {
        QSignalBlocker blocker(msettings.blend_slider);
        msettings.blend_slider->setValue((int)std::lround(msettings.blend));
    }
    if(msettings.blend_label)
    {
        msettings.blend_label->setText(QString::number((int)msettings.blend));
    }
    if(msettings.propagation_speed_slider)
    {
        QSignalBlocker blocker(msettings.propagation_speed_slider);
        msettings.propagation_speed_slider->setValue((int)std::lround(msettings.propagation_speed_mm_per_ms));
    }
    if(msettings.propagation_speed_label)
    {
        msettings.propagation_speed_label->setText(QString::number((int)std::lround(msettings.propagation_speed_mm_per_ms)) + "%");
    }
    if(msettings.wave_decay_slider)
    {
        QSignalBlocker blocker(msettings.wave_decay_slider);
        msettings.wave_decay_slider->setValue((int)std::lround(msettings.wave_decay_ms));
    }
    if(msettings.wave_decay_label)
    {
        msettings.wave_decay_label->setText(QString::number((int)msettings.wave_decay_ms) + "ms");
    }
    if(msettings.wave_time_to_edge_slider)
    {
        QSignalBlocker blocker(msettings.wave_time_to_edge_slider);
        msettings.wave_time_to_edge_slider->setValue((int)(msettings.wave_time_to_edge_sec * 10.0f));
    }
    if(msettings.wave_time_to_edge_label)
    {
        msettings.wave_time_to_edge_label->setText(msettings.wave_time_to_edge_sec <= 0.0f ? "Off" : QString::number(msettings.wave_time_to_edge_sec, 'f', 1) + "s");
    }
    if(msettings.falloff_curve_slider)
    {
        QSignalBlocker blocker(msettings.falloff_curve_slider);
        msettings.falloff_curve_slider->setValue((int)(msettings.falloff_curve_exponent * 100.0f));
    }
    if(msettings.falloff_curve_label)
    {
        msettings.falloff_curve_label->setText(QString::number((int)(msettings.falloff_curve_exponent * 100.0f)) + "%");
    }
    if(msettings.front_back_balance_slider)
    {
        QSignalBlocker blocker(msettings.front_back_balance_slider);
        msettings.front_back_balance_slider->setValue((int)std::lround(msettings.front_back_balance));
    }
    if(msettings.front_back_balance_label)
    {
        msettings.front_back_balance_label->setText(QString::number((int)std::lround(msettings.front_back_balance)));
    }
    if(msettings.left_right_balance_slider)
    {
        QSignalBlocker blocker(msettings.left_right_balance_slider);
        msettings.left_right_balance_slider->setValue((int)std::lround(msettings.left_right_balance));
    }
    if(msettings.left_right_balance_label)
    {
        msettings.left_right_balance_label->setText(QString::number((int)std::lround(msettings.left_right_balance)));
    }
    if(msettings.top_bottom_balance_slider)
    {
        QSignalBlocker blocker(msettings.top_bottom_balance_slider);
        msettings.top_bottom_balance_slider->setValue((int)std::lround(msettings.top_bottom_balance));
    }
    if(msettings.top_bottom_balance_label)
    {
        msettings.top_bottom_balance_label->setText(QString::number((int)std::lround(msettings.top_bottom_balance)));
    }
    if(msettings.calibration_pattern_check)
    {
        QSignalBlocker blocker(msettings.calibration_pattern_check);
        msettings.calibration_pattern_check->setChecked(msettings.show_calibration_pattern);
    }
    if(msettings.screen_preview_check)
    {
        QSignalBlocker blocker(msettings.screen_preview_check);
        msettings.screen_preview_check->setChecked(msettings.show_screen_preview);
    }
    if(msettings.capture_area_preview)
    {
        msettings.capture_area_preview->update();
    }
    if(msettings.screen_map_roll_slider)
    {
        QSignalBlocker blocker(msettings.screen_map_roll_slider);
        msettings.screen_map_roll_slider->setValue(
            (int)std::lround(std::clamp(msettings.screen_map_roll_deg, -180.0f, 180.0f) *
                             (float)kScreenMapRollTicksPerDegree));
    }
    if(msettings.screen_map_roll_label)
    {
        msettings.screen_map_roll_label->setText(
            QString::number((double)std::clamp(msettings.screen_map_roll_deg, -180.0f, 180.0f), 'f', 1) +
            QChar(0x00B0));
    }
    if(msettings.radial_corner_expansion_slider)
    {
        QSignalBlocker blocker(msettings.radial_corner_expansion_slider);
        msettings.radial_corner_expansion_slider->setValue(msettings.radial_corner_expansion_ui);
    }
    if(msettings.radial_corner_expansion_label)
    {
        msettings.radial_corner_expansion_label->setText(QString::number(msettings.radial_corner_expansion_ui) + "%");
    }
    if(msettings.radial_corner_bias_tl_slider)
    {
        QSignalBlocker blocker(msettings.radial_corner_bias_tl_slider);
        msettings.radial_corner_bias_tl_slider->setValue(msettings.radial_corner_bias_tl_ui);
    }
    if(msettings.radial_corner_bias_tl_label)
    {
        msettings.radial_corner_bias_tl_label->setText(QString::number(msettings.radial_corner_bias_tl_ui) + "%");
    }
    if(msettings.radial_corner_bias_tr_slider)
    {
        QSignalBlocker blocker(msettings.radial_corner_bias_tr_slider);
        msettings.radial_corner_bias_tr_slider->setValue(msettings.radial_corner_bias_tr_ui);
    }
    if(msettings.radial_corner_bias_tr_label)
    {
        msettings.radial_corner_bias_tr_label->setText(QString::number(msettings.radial_corner_bias_tr_ui) + "%");
    }
    if(msettings.radial_corner_bias_bl_slider)
    {
        QSignalBlocker blocker(msettings.radial_corner_bias_bl_slider);
        msettings.radial_corner_bias_bl_slider->setValue(msettings.radial_corner_bias_bl_ui);
    }
    if(msettings.radial_corner_bias_bl_label)
    {
        msettings.radial_corner_bias_bl_label->setText(QString::number(msettings.radial_corner_bias_bl_ui) + "%");
    }
    if(msettings.radial_corner_bias_br_slider)
    {
        QSignalBlocker blocker(msettings.radial_corner_bias_br_slider);
        msettings.radial_corner_bias_br_slider->setValue(msettings.radial_corner_bias_br_ui);
    }
    if(msettings.radial_corner_bias_br_label)
    {
        msettings.radial_corner_bias_br_label->setText(QString::number(msettings.radial_corner_bias_br_ui) + "%");
    }
    if(msettings.corner_blend_strength_slider)
    {
        QSignalBlocker blocker(msettings.corner_blend_strength_slider);
        msettings.corner_blend_strength_slider->setValue((int)std::lround(msettings.corner_blend_strength_pct));
    }
    if(msettings.corner_blend_strength_label)
    {
        msettings.corner_blend_strength_label->setText(
            QString::number((int)std::lround(msettings.corner_blend_strength_pct)) + "%");
    }
    if(msettings.corner_blend_zone_slider)
    {
        QSignalBlocker blocker(msettings.corner_blend_zone_slider);
        msettings.corner_blend_zone_slider->setValue((int)std::lround(msettings.corner_blend_zone_pct));
    }
    if(msettings.corner_blend_zone_label)
    {
        msettings.corner_blend_zone_label->setText(
            QString::number((int)std::lround(msettings.corner_blend_zone_pct)) + "%");
    }
    if(msettings.ref_point_combo)
    {
        QSignalBlocker blocker(msettings.ref_point_combo);
        int desired = msettings.reference_point_id;
        int idx = msettings.ref_point_combo->findData(desired);
        if(idx < 0)
        {
            idx = msettings.ref_point_combo->findData(-1);
            msettings.reference_point_id = -1;
        }
        if(idx >= 0) msettings.ref_point_combo->setCurrentIndex(idx);
    }
}

void ScreenMirror::LoadSettings(const nlohmann::json& settings)
{
    static const float default_scale = 1.0f;
    static const bool default_scale_inverted = true;
    static const float default_smoothing_time_ms = 0.0f;
    static const float default_brightness_multiplier = 1.0f;
    static const float default_brightness_threshold = 0.0f;
    static const float default_propagation_speed_mm_per_ms = 0.0f;
    static const float default_wave_decay_ms = 0.0f;
    static const float default_wave_time_to_edge_sec = 0.0f;
    static const float default_falloff_curve_exponent = 1.0f;
    static const float default_front_back_balance = 0.0f;
    static const float default_left_right_balance = 0.0f;
    static const float default_top_bottom_balance = 0.0f;

    if(settings.contains("capture_quality"))
    {
        capture_quality = std::clamp(settings["capture_quality"].get<int>(), 0, 7);
        if(capture_quality_combo)
        {
            capture_quality_combo->setCurrentIndex(capture_quality);
        }
    }
    if(settings.contains("capture_backend_mode"))
    {
        capture_backend_mode = std::clamp(settings["capture_backend_mode"].get<int>(), 0, 2);
        if(capture_backend_combo)
        {
            capture_backend_combo->setCurrentIndex(capture_backend_mode);
        }
        ScreenCaptureManager::Instance().SetWindowsCaptureBackendMode(capture_backend_mode);
    }

    if(!settings.contains("monitor_settings"))
    {
        return;
    }

    const nlohmann::json& monitors = settings["monitor_settings"];
    for(nlohmann::json::const_iterator it = monitors.begin(); it != monitors.end(); ++it)
    {
            const std::string& monitor_name = it.key();
            const nlohmann::json& mon = it.value();

            std::map<std::string, MonitorSettings>::iterator existing_it = monitor_settings.find(monitor_name);
            bool had_existing = (existing_it != monitor_settings.end());
            
            QGroupBox* existing_group_box = nullptr;
            QComboBox* existing_ref_point_combo = nullptr;
            QSlider* existing_scale_slider = nullptr;
            QLabel* existing_scale_label = nullptr;
            QCheckBox* existing_scale_invert_check = nullptr;
            QSlider* existing_smoothing_time_slider = nullptr;
            QLabel* existing_smoothing_time_label = nullptr;
            QSlider* existing_brightness_slider = nullptr;
            QLabel* existing_brightness_label = nullptr;
            QSlider* existing_brightness_threshold_slider = nullptr;
            QLabel* existing_brightness_threshold_label = nullptr;
            QSlider* existing_white_rolloff_slider = nullptr;
            QLabel* existing_white_rolloff_label = nullptr;
            QSlider* existing_vibrance_slider = nullptr;
            QLabel* existing_vibrance_label = nullptr;
            QSlider* existing_black_bar_letterbox_slider = nullptr;
            QLabel* existing_black_bar_letterbox_label = nullptr;
            QSlider* existing_black_bar_pillarbox_slider = nullptr;
            QLabel* existing_black_bar_pillarbox_label = nullptr;
            QSlider* existing_softness_slider = nullptr;
            QLabel* existing_softness_label = nullptr;
            QSlider* existing_blend_slider = nullptr;
            QLabel* existing_blend_label = nullptr;
            QSlider* existing_propagation_speed_slider = nullptr;
            QLabel* existing_propagation_speed_label = nullptr;
            QSlider* existing_wave_decay_slider = nullptr;
            QLabel* existing_wave_decay_label = nullptr;
            QSlider* existing_wave_time_to_edge_slider = nullptr;
            QLabel* existing_wave_time_to_edge_label = nullptr;
            QSlider* existing_falloff_curve_slider = nullptr;
            QLabel* existing_falloff_curve_label = nullptr;
            QSlider* existing_front_back_balance_slider = nullptr;
            QLabel* existing_front_back_balance_label = nullptr;
            QSlider* existing_left_right_balance_slider = nullptr;
            QLabel* existing_left_right_balance_label = nullptr;
            QSlider* existing_top_bottom_balance_slider = nullptr;
            QLabel* existing_top_bottom_balance_label = nullptr;
            QCheckBox* existing_calibration_pattern_check = nullptr;
            QCheckBox* existing_screen_preview_check = nullptr;
            QWidget* existing_capture_area_preview = nullptr;
            QPushButton* existing_add_zone_button = nullptr;
            CaptureZonesWidget* existing_capture_zones_widget = nullptr;
            QSlider* existing_screen_map_roll_slider = nullptr;
            QLabel* existing_screen_map_roll_label = nullptr;
            QSlider* existing_radial_corner_expansion_slider = nullptr;
            QLabel* existing_radial_corner_expansion_label = nullptr;
            QSlider* existing_radial_corner_bias_tl_slider = nullptr;
            QLabel* existing_radial_corner_bias_tl_label = nullptr;
            QSlider* existing_radial_corner_bias_tr_slider = nullptr;
            QLabel* existing_radial_corner_bias_tr_label = nullptr;
            QSlider* existing_radial_corner_bias_bl_slider = nullptr;
            QLabel* existing_radial_corner_bias_bl_label = nullptr;
            QSlider* existing_radial_corner_bias_br_slider = nullptr;
            QLabel* existing_radial_corner_bias_br_label = nullptr;
            QSlider* existing_corner_blend_strength_slider = nullptr;
            QLabel* existing_corner_blend_strength_label = nullptr;
            QSlider* existing_corner_blend_zone_slider = nullptr;
            QLabel* existing_corner_blend_zone_label = nullptr;

            if(had_existing)
            {
                existing_group_box = existing_it->second.group_box;
                existing_ref_point_combo = existing_it->second.ref_point_combo;
                existing_scale_slider = existing_it->second.scale_slider;
                existing_scale_label = existing_it->second.scale_label;
                existing_scale_invert_check = existing_it->second.scale_invert_check;
                existing_smoothing_time_slider = existing_it->second.smoothing_time_slider;
                existing_smoothing_time_label = existing_it->second.smoothing_time_label;
                existing_brightness_slider = existing_it->second.brightness_slider;
                existing_brightness_label = existing_it->second.brightness_label;
                existing_brightness_threshold_slider = existing_it->second.brightness_threshold_slider;
                existing_brightness_threshold_label = existing_it->second.brightness_threshold_label;
                existing_white_rolloff_slider = existing_it->second.white_rolloff_slider;
                existing_white_rolloff_label = existing_it->second.white_rolloff_label;
                existing_vibrance_slider = existing_it->second.vibrance_slider;
                existing_vibrance_label = existing_it->second.vibrance_label;
                existing_black_bar_letterbox_slider = existing_it->second.black_bar_letterbox_slider;
                existing_black_bar_letterbox_label = existing_it->second.black_bar_letterbox_label;
                existing_black_bar_pillarbox_slider = existing_it->second.black_bar_pillarbox_slider;
                existing_black_bar_pillarbox_label = existing_it->second.black_bar_pillarbox_label;
                existing_softness_slider = existing_it->second.softness_slider;
                existing_softness_label = existing_it->second.softness_label;
                existing_blend_slider = existing_it->second.blend_slider;
                existing_blend_label = existing_it->second.blend_label;
                existing_propagation_speed_slider = existing_it->second.propagation_speed_slider;
                existing_propagation_speed_label = existing_it->second.propagation_speed_label;
                existing_wave_decay_slider = existing_it->second.wave_decay_slider;
                existing_wave_decay_label = existing_it->second.wave_decay_label;
                existing_wave_time_to_edge_slider = existing_it->second.wave_time_to_edge_slider;
                existing_wave_time_to_edge_label = existing_it->second.wave_time_to_edge_label;
                existing_falloff_curve_slider = existing_it->second.falloff_curve_slider;
                existing_falloff_curve_label = existing_it->second.falloff_curve_label;
                existing_front_back_balance_slider = existing_it->second.front_back_balance_slider;
                existing_front_back_balance_label = existing_it->second.front_back_balance_label;
                existing_left_right_balance_slider = existing_it->second.left_right_balance_slider;
                existing_left_right_balance_label = existing_it->second.left_right_balance_label;
                existing_top_bottom_balance_slider = existing_it->second.top_bottom_balance_slider;
                existing_top_bottom_balance_label = existing_it->second.top_bottom_balance_label;
                existing_capture_area_preview = existing_it->second.capture_area_preview;
                existing_add_zone_button = existing_it->second.add_zone_button;
                existing_capture_zones_widget = existing_it->second.capture_zones_widget;
                existing_screen_map_roll_slider = existing_it->second.screen_map_roll_slider;
                existing_screen_map_roll_label = existing_it->second.screen_map_roll_label;
                existing_radial_corner_expansion_slider = existing_it->second.radial_corner_expansion_slider;
                existing_radial_corner_expansion_label = existing_it->second.radial_corner_expansion_label;
                existing_radial_corner_bias_tl_slider = existing_it->second.radial_corner_bias_tl_slider;
                existing_radial_corner_bias_tl_label = existing_it->second.radial_corner_bias_tl_label;
                existing_radial_corner_bias_tr_slider = existing_it->second.radial_corner_bias_tr_slider;
                existing_radial_corner_bias_tr_label = existing_it->second.radial_corner_bias_tr_label;
                existing_radial_corner_bias_bl_slider = existing_it->second.radial_corner_bias_bl_slider;
                existing_radial_corner_bias_bl_label = existing_it->second.radial_corner_bias_bl_label;
                existing_radial_corner_bias_br_slider = existing_it->second.radial_corner_bias_br_slider;
                existing_radial_corner_bias_br_label = existing_it->second.radial_corner_bias_br_label;
                existing_corner_blend_strength_slider = existing_it->second.corner_blend_strength_slider;
                existing_corner_blend_strength_label = existing_it->second.corner_blend_strength_label;
                existing_corner_blend_zone_slider = existing_it->second.corner_blend_zone_slider;
                existing_corner_blend_zone_label = existing_it->second.corner_blend_zone_label;
            }
            else
            {
                monitor_settings[monitor_name] = MonitorSettings();
                existing_it = monitor_settings.find(monitor_name);
            }
            MonitorSettings& msettings = existing_it->second;

            if(mon.contains("enabled"))
            {
                msettings.enabled = mon["enabled"].get<bool>();
            }
            else if(!had_existing)
            {
                DisplayPlane3D* loaded_plane = FindDisplayPlaneByName(monitor_name);
                msettings.enabled = loaded_plane ? DefaultMonitorEnabledForPlane(loaded_plane) : false;
            }

            if(mon.contains("scale")) msettings.scale = mon["scale"].get<float>();
            else msettings.scale = default_scale;
            if(mon.contains("scale_inverted")) msettings.scale_inverted = mon["scale_inverted"].get<bool>();
            else msettings.scale_inverted = default_scale_inverted;

            if(mon.contains("smoothing_time_ms")) msettings.smoothing_time_ms = mon["smoothing_time_ms"].get<float>();
            else msettings.smoothing_time_ms = default_smoothing_time_ms;
            if(mon.contains("brightness_multiplier")) msettings.brightness_multiplier = mon["brightness_multiplier"].get<float>();
            else msettings.brightness_multiplier = default_brightness_multiplier;
            if(mon.contains("brightness_threshold"))
            {
                msettings.brightness_threshold = mon["brightness_threshold"].get<float>();
            }
            else msettings.brightness_threshold = default_brightness_threshold;
            if(mon.contains("white_rolloff")) msettings.white_rolloff = mon["white_rolloff"].get<float>();
            else msettings.white_rolloff = 0.0f;
            if(mon.contains("vibrance")) msettings.vibrance = mon["vibrance"].get<float>();
            else msettings.vibrance = 1.0f;
            if(mon.contains("black_bar_letterbox_percent")) msettings.black_bar_letterbox_percent = mon["black_bar_letterbox_percent"].get<float>();
            else msettings.black_bar_letterbox_percent = 0.0f;
            if(mon.contains("black_bar_pillarbox_percent")) msettings.black_bar_pillarbox_percent = mon["black_bar_pillarbox_percent"].get<float>();
            else msettings.black_bar_pillarbox_percent = 0.0f;

            if(mon.contains("edge_softness")) msettings.edge_softness = mon["edge_softness"].get<float>();
            else msettings.edge_softness = 0.0f;
            if(mon.contains("blend")) msettings.blend = mon["blend"].get<float>();
            else msettings.blend = 0.0f;
            if(mon.contains("propagation_speed_mm_per_ms"))
            {
                msettings.propagation_speed_mm_per_ms = mon["propagation_speed_mm_per_ms"].get<float>();
            }
            else msettings.propagation_speed_mm_per_ms = default_propagation_speed_mm_per_ms;
            if(mon.contains("wave_decay_ms")) msettings.wave_decay_ms = mon["wave_decay_ms"].get<float>();
            else msettings.wave_decay_ms = default_wave_decay_ms;
            if(mon.contains("wave_time_to_edge_sec")) msettings.wave_time_to_edge_sec = mon["wave_time_to_edge_sec"].get<float>();
            else msettings.wave_time_to_edge_sec = default_wave_time_to_edge_sec;
            if(mon.contains("falloff_curve_exponent")) msettings.falloff_curve_exponent = mon["falloff_curve_exponent"].get<float>();
            else msettings.falloff_curve_exponent = default_falloff_curve_exponent;
            if(mon.contains("front_back_balance")) msettings.front_back_balance = mon["front_back_balance"].get<float>();
            else msettings.front_back_balance = default_front_back_balance;
            if(mon.contains("left_right_balance")) msettings.left_right_balance = mon["left_right_balance"].get<float>();
            else msettings.left_right_balance = default_left_right_balance;
            if(mon.contains("top_bottom_balance")) msettings.top_bottom_balance = mon["top_bottom_balance"].get<float>();
            else msettings.top_bottom_balance = default_top_bottom_balance;
            
            if(mon.contains("show_calibration_pattern"))
            {
                msettings.show_calibration_pattern = mon["show_calibration_pattern"].get<bool>();
            }
            if(mon.contains("show_screen_preview")) msettings.show_screen_preview = mon["show_screen_preview"].get<bool>();
            
            if(mon.contains("capture_zones") && mon["capture_zones"].is_array())
            {
                msettings.capture_zones.clear();
                const nlohmann::json& zones_arr = mon["capture_zones"];
                for(size_t zi = 0; zi < zones_arr.size(); zi++)
                {
                    const nlohmann::json& zone_json = zones_arr[zi];
                    CaptureZone zone;
                    zone.u_min = zone_json.value("u_min", 0.0f);
                    zone.u_max = zone_json.value("u_max", 1.0f);
                    zone.v_min = zone_json.value("v_min", 0.0f);
                    zone.v_max = zone_json.value("v_max", 1.0f);
                    zone.enabled = zone_json.value("enabled", true);
                    zone.name = zone_json.value("name", "Zone");
                    
                    zone.u_min = std::clamp(zone.u_min, 0.0f, 1.0f);
                    zone.u_max = std::clamp(zone.u_max, 0.0f, 1.0f);
                    zone.v_min = std::clamp(zone.v_min, 0.0f, 1.0f);
                    zone.v_max = std::clamp(zone.v_max, 0.0f, 1.0f);
                    if(zone.u_min > zone.u_max) { float temp = zone.u_min; zone.u_min = zone.u_max; zone.u_max = temp; }
                    if(zone.v_min > zone.v_max) { float temp = zone.v_min; zone.v_min = zone.v_max; zone.v_max = temp; }
                    
                    msettings.capture_zones.push_back(zone);
                }
                
                if(msettings.capture_zones.empty())
                {
                    msettings.capture_zones.push_back(CaptureZone(0.0f, 1.0f, 0.0f, 1.0f));
                }
            }
            else
            {
                msettings.capture_zones.clear();
                msettings.capture_zones.push_back(CaptureZone(0.0f, 1.0f, 0.0f, 1.0f));
            }
            
            if(mon.contains("reference_point_id"))
            {
                msettings.reference_point_id = mon["reference_point_id"].get<int>();
            }

            if(mon.contains("radial_map_roll_deg"))
            {
                msettings.screen_map_roll_deg =
                    std::clamp(static_cast<float>(mon["radial_map_roll_deg"].get<double>()), -180.0f, 180.0f);
            }
            if(mon.contains("radial_map_expansion"))
            {
                msettings.radial_corner_expansion_ui = std::clamp(mon["radial_map_expansion"].get<int>(), 0, 100);
            }
            if(mon.contains("radial_map_bias_tl"))
            {
                msettings.radial_corner_bias_tl_ui = std::clamp(mon["radial_map_bias_tl"].get<int>(), 0, 100);
            }
            if(mon.contains("radial_map_bias_tr"))
            {
                msettings.radial_corner_bias_tr_ui = std::clamp(mon["radial_map_bias_tr"].get<int>(), 0, 100);
            }
            if(mon.contains("radial_map_bias_bl"))
            {
                msettings.radial_corner_bias_bl_ui = std::clamp(mon["radial_map_bias_bl"].get<int>(), 0, 100);
            }
            if(mon.contains("radial_map_bias_br"))
            {
                msettings.radial_corner_bias_br_ui = std::clamp(mon["radial_map_bias_br"].get<int>(), 0, 100);
            }
            if(mon.contains("sample_corner_blend_strength_pct"))
            {
                msettings.corner_blend_strength_pct = std::clamp(
                    static_cast<float>(mon["sample_corner_blend_strength_pct"].get<double>()), 0.0f, 100.0f);
            }
            if(mon.contains("sample_corner_blend_zone_pct"))
            {
                msettings.corner_blend_zone_pct = std::clamp(
                    static_cast<float>(mon["sample_corner_blend_zone_pct"].get<double>()), 0.0f, 32.0f);
            }

            msettings.scale = std::clamp(msettings.scale, 0.0f, 3.0f);
            msettings.smoothing_time_ms = std::clamp(msettings.smoothing_time_ms, 0.0f, 500.0f);
            msettings.brightness_multiplier = std::clamp(msettings.brightness_multiplier, 0.0f, 2.0f);
            msettings.brightness_threshold = std::clamp(msettings.brightness_threshold, 0.0f, 255.0f);
            msettings.white_rolloff = std::clamp(msettings.white_rolloff, 0.0f, 1.0f);
            msettings.vibrance = std::clamp(msettings.vibrance, 0.0f, 2.0f);
            msettings.black_bar_letterbox_percent = std::clamp(msettings.black_bar_letterbox_percent, 0.0f, 50.0f);
            msettings.black_bar_pillarbox_percent = std::clamp(msettings.black_bar_pillarbox_percent, 0.0f, 50.0f);
            msettings.edge_softness = std::clamp(msettings.edge_softness, 0.0f, 100.0f);
            msettings.blend = std::clamp(msettings.blend, 0.0f, 100.0f);
            msettings.propagation_speed_mm_per_ms = std::clamp(msettings.propagation_speed_mm_per_ms, 0.0f, 100.0f);
            msettings.wave_decay_ms = std::clamp(msettings.wave_decay_ms, 0.0f, 3000.0f);
            msettings.wave_time_to_edge_sec = std::clamp(msettings.wave_time_to_edge_sec, 0.0f, 10.0f);
            msettings.falloff_curve_exponent = std::clamp(msettings.falloff_curve_exponent, 0.5f, 2.0f);
            msettings.front_back_balance = std::clamp(msettings.front_back_balance, -100.0f, 100.0f);
            msettings.left_right_balance = std::clamp(msettings.left_right_balance, -100.0f, 100.0f);
            msettings.top_bottom_balance = std::clamp(msettings.top_bottom_balance, -100.0f, 100.0f);
            msettings.screen_map_roll_deg = std::clamp(msettings.screen_map_roll_deg, -180.0f, 180.0f);
            msettings.radial_corner_expansion_ui = std::clamp(msettings.radial_corner_expansion_ui, 0, 100);
            msettings.radial_corner_bias_tl_ui = std::clamp(msettings.radial_corner_bias_tl_ui, 0, 100);
            msettings.radial_corner_bias_tr_ui = std::clamp(msettings.radial_corner_bias_tr_ui, 0, 100);
            msettings.radial_corner_bias_bl_ui = std::clamp(msettings.radial_corner_bias_bl_ui, 0, 100);
            msettings.radial_corner_bias_br_ui = std::clamp(msettings.radial_corner_bias_br_ui, 0, 100);
            msettings.corner_blend_strength_pct = std::clamp(msettings.corner_blend_strength_pct, 0.0f, 100.0f);
            msettings.corner_blend_zone_pct = std::clamp(msettings.corner_blend_zone_pct, 0.0f, 32.0f);
            
            if(had_existing)
            {
                msettings.group_box = existing_group_box;
                msettings.ref_point_combo = existing_ref_point_combo;
                msettings.scale_slider = existing_scale_slider;
                msettings.scale_label = existing_scale_label;
                msettings.scale_invert_check = existing_scale_invert_check;
                msettings.smoothing_time_slider = existing_smoothing_time_slider;
                msettings.smoothing_time_label = existing_smoothing_time_label;
                msettings.brightness_slider = existing_brightness_slider;
                msettings.brightness_label = existing_brightness_label;
                msettings.brightness_threshold_slider = existing_brightness_threshold_slider;
                msettings.brightness_threshold_label = existing_brightness_threshold_label;
                msettings.white_rolloff_slider = existing_white_rolloff_slider;
                msettings.white_rolloff_label = existing_white_rolloff_label;
                msettings.vibrance_slider = existing_vibrance_slider;
                msettings.vibrance_label = existing_vibrance_label;
                msettings.black_bar_letterbox_slider = existing_black_bar_letterbox_slider;
                msettings.black_bar_letterbox_label = existing_black_bar_letterbox_label;
                msettings.black_bar_pillarbox_slider = existing_black_bar_pillarbox_slider;
                msettings.black_bar_pillarbox_label = existing_black_bar_pillarbox_label;
                msettings.softness_slider = existing_softness_slider;
                msettings.softness_label = existing_softness_label;
                msettings.blend_slider = existing_blend_slider;
                msettings.blend_label = existing_blend_label;
                msettings.propagation_speed_slider = existing_propagation_speed_slider;
                msettings.propagation_speed_label = existing_propagation_speed_label;
                msettings.wave_decay_slider = existing_wave_decay_slider;
                msettings.wave_decay_label = existing_wave_decay_label;
                msettings.wave_time_to_edge_slider = existing_wave_time_to_edge_slider;
                msettings.wave_time_to_edge_label = existing_wave_time_to_edge_label;
                msettings.falloff_curve_slider = existing_falloff_curve_slider;
                msettings.falloff_curve_label = existing_falloff_curve_label;
                msettings.front_back_balance_slider = existing_front_back_balance_slider;
                msettings.front_back_balance_label = existing_front_back_balance_label;
                msettings.left_right_balance_slider = existing_left_right_balance_slider;
                msettings.left_right_balance_label = existing_left_right_balance_label;
                msettings.top_bottom_balance_slider = existing_top_bottom_balance_slider;
                msettings.top_bottom_balance_label = existing_top_bottom_balance_label;
                msettings.calibration_pattern_check = existing_calibration_pattern_check;
                msettings.screen_preview_check = existing_screen_preview_check;
                msettings.capture_area_preview = existing_capture_area_preview;
                msettings.add_zone_button = existing_add_zone_button;
                msettings.capture_zones_widget = existing_capture_zones_widget;
                msettings.screen_map_roll_slider = existing_screen_map_roll_slider;
                msettings.screen_map_roll_label = existing_screen_map_roll_label;
                msettings.radial_corner_expansion_slider = existing_radial_corner_expansion_slider;
                msettings.radial_corner_expansion_label = existing_radial_corner_expansion_label;
                msettings.radial_corner_bias_tl_slider = existing_radial_corner_bias_tl_slider;
                msettings.radial_corner_bias_tl_label = existing_radial_corner_bias_tl_label;
                msettings.radial_corner_bias_tr_slider = existing_radial_corner_bias_tr_slider;
                msettings.radial_corner_bias_tr_label = existing_radial_corner_bias_tr_label;
                msettings.radial_corner_bias_bl_slider = existing_radial_corner_bias_bl_slider;
                msettings.radial_corner_bias_bl_label = existing_radial_corner_bias_bl_label;
                msettings.radial_corner_bias_br_slider = existing_radial_corner_bias_br_slider;
                msettings.radial_corner_bias_br_label = existing_radial_corner_bias_br_label;
                msettings.corner_blend_strength_slider = existing_corner_blend_strength_slider;
                msettings.corner_blend_strength_label = existing_corner_blend_strength_label;
                msettings.corner_blend_zone_slider = existing_corner_blend_zone_slider;
                msettings.corner_blend_zone_label = existing_corner_blend_zone_label;

                if(msettings.capture_zones_widget)
                {
                    msettings.capture_zones_widget->SetCaptureZones(&msettings.capture_zones);
                    msettings.capture_zones_widget->SetValueChangedCallback([this]() { OnParameterChanged(); });
                    std::vector<DisplayPlane3D*> planes = DisplayPlaneManager::instance()->GetDisplayPlanes();
                    for(DisplayPlane3D* plane : planes)
                    {
                        if(plane && plane->GetName() == monitor_name)
                        {
                            msettings.capture_zones_widget->SetDisplayPlane(plane);
                            break;
                        }
                    }
                }
            }

            if(msettings.white_rolloff_slider == nullptr && msettings.group_box != nullptr)
            {
                QGroupBox* bg = msettings.group_box->findChild<QGroupBox*>("ScreenMirror_BrightnessGroup");
                if(!bg)
                {
                    for(QObject* child : msettings.group_box->children())
                    {
                        QGroupBox* g = qobject_cast<QGroupBox*>(child);
                        if(g && g->title() == "Brightness") { bg = g; break; }
                    }
                }
                QFormLayout* form = bg ? qobject_cast<QFormLayout*>(bg->layout()) : nullptr;
                if(form)
                {
                    bool has_capture = !DisplayPlaneManager::instance()->GetDisplayPlanes().empty();
                    QWidget* white_rolloff_widget = new QWidget();
                    QHBoxLayout* white_rolloff_layout = new QHBoxLayout(white_rolloff_widget);
                    white_rolloff_layout->setContentsMargins(0, 0, 0, 0);
                    msettings.white_rolloff_slider = new QSlider(Qt::Horizontal);
                    msettings.white_rolloff_slider->setRange(0, 100);
                    msettings.white_rolloff_slider->setValue((int)std::lround(msettings.white_rolloff * 100.0f));
                    msettings.white_rolloff_slider->setEnabled(has_capture);
                    msettings.white_rolloff_slider->setToolTip("How much to reduce white/gray wash (0-100%). 0 = no rolloff, 100% = strong; keeps RGB/CYM vibrant.");
                    white_rolloff_layout->addWidget(msettings.white_rolloff_slider);
                    msettings.white_rolloff_label = new QLabel(QString::number((int)std::lround(msettings.white_rolloff * 100.0f)) + "%");
                    msettings.white_rolloff_label->setMinimumWidth(45);
                    white_rolloff_layout->addWidget(msettings.white_rolloff_label);
                    connect(msettings.white_rolloff_slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);
                    connect(msettings.white_rolloff_slider, &QSlider::valueChanged, msettings.white_rolloff_label, [&msettings](int v) { msettings.white_rolloff_label->setText(QString::number(v) + "%"); });
                    form->addRow("White rolloff:", white_rolloff_widget);

                    QWidget* vibrance_widget = new QWidget();
                    QHBoxLayout* vibrance_layout = new QHBoxLayout(vibrance_widget);
                    vibrance_layout->setContentsMargins(0, 0, 0, 0);
                    msettings.vibrance_slider = new QSlider(Qt::Horizontal);
                    msettings.vibrance_slider->setRange(0, 200);
                    msettings.vibrance_slider->setValue((int)std::lround(msettings.vibrance * 100.0f));
                    msettings.vibrance_slider->setEnabled(has_capture);
                    msettings.vibrance_slider->setToolTip("Saturation (0-200%). 100% = no change, below 100% = more muted, above 100% = more vivid RGB/CYM.");
                    vibrance_layout->addWidget(msettings.vibrance_slider);
                    msettings.vibrance_label = new QLabel(QString::number((int)std::lround(msettings.vibrance * 100.0f)) + "%");
                    msettings.vibrance_label->setMinimumWidth(45);
                    vibrance_layout->addWidget(msettings.vibrance_label);
                    connect(msettings.vibrance_slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);
                    connect(msettings.vibrance_slider, &QSlider::valueChanged, msettings.vibrance_label, [&msettings](int v) { msettings.vibrance_label->setText(QString::number(v) + "%"); });
                    form->addRow("Vibrance:", vibrance_widget);
                }
            }
    }

    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        SyncMonitorSettingsToUI(it->second);
    }

    RefreshReferencePointDropdowns();
    
    RefreshMonitorStatus();
    
    OnScreenPreviewChanged();
    OnCalibrationPatternChanged();
    
    OnParameterChanged();
}

void ScreenMirror::OnParameterChanged()
{
    if(in_parameter_change_)
    {
        return;
    }
    in_parameter_change_ = true;

    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        MonitorSettings& settings = it->second;
        if(settings.group_box) settings.enabled = settings.group_box->isChecked();
        
        if(settings.scale_slider) settings.scale = std::clamp(settings.scale_slider->value() / 100.0f, 0.0f, 3.0f);
        if(settings.scale_invert_check) settings.scale_inverted = settings.scale_invert_check->isChecked();
        
        if(settings.smoothing_time_slider) settings.smoothing_time_ms = (float)settings.smoothing_time_slider->value();
        if(settings.brightness_slider) settings.brightness_multiplier = std::clamp(settings.brightness_slider->value() / 100.0f, 0.0f, 2.0f);
        if(settings.brightness_threshold_slider) settings.brightness_threshold = (float)settings.brightness_threshold_slider->value();
        if(settings.white_rolloff_slider) settings.white_rolloff = std::clamp((float)settings.white_rolloff_slider->value() / 100.0f, 0.0f, 1.0f);
        if(settings.vibrance_slider) settings.vibrance = std::clamp((float)settings.vibrance_slider->value() / 100.0f, 0.0f, 2.0f);
        if(settings.black_bar_letterbox_slider) settings.black_bar_letterbox_percent = (float)settings.black_bar_letterbox_slider->value();
        if(settings.black_bar_pillarbox_slider) settings.black_bar_pillarbox_percent = (float)settings.black_bar_pillarbox_slider->value();
        
        if(settings.softness_slider) settings.edge_softness = (float)settings.softness_slider->value();
        if(settings.blend_slider) settings.blend = (float)settings.blend_slider->value();
        if(settings.propagation_speed_slider) settings.propagation_speed_mm_per_ms = std::clamp((float)settings.propagation_speed_slider->value(), 0.0f, 100.0f);
        if(settings.wave_decay_slider) settings.wave_decay_ms = (float)settings.wave_decay_slider->value();
        if(settings.wave_time_to_edge_slider) settings.wave_time_to_edge_sec = (float)settings.wave_time_to_edge_slider->value() / 10.0f;
        if(settings.falloff_curve_slider) settings.falloff_curve_exponent = std::clamp((float)settings.falloff_curve_slider->value() / 100.0f, 0.5f, 2.0f);
        if(settings.front_back_balance_slider) settings.front_back_balance = std::clamp((float)settings.front_back_balance_slider->value(), -100.0f, 100.0f);
        if(settings.left_right_balance_slider) settings.left_right_balance = std::clamp((float)settings.left_right_balance_slider->value(), -100.0f, 100.0f);
        if(settings.top_bottom_balance_slider) settings.top_bottom_balance = std::clamp((float)settings.top_bottom_balance_slider->value(), -100.0f, 100.0f);

        if(settings.screen_map_roll_slider)
        {
            settings.screen_map_roll_deg = std::clamp(
                (float)settings.screen_map_roll_slider->value() / (float)kScreenMapRollTicksPerDegree, -180.0f, 180.0f);
        }
        if(settings.radial_corner_expansion_slider)
        {
            settings.radial_corner_expansion_ui = std::clamp(settings.radial_corner_expansion_slider->value(), 0, 100);
        }
        if(settings.radial_corner_bias_tl_slider)
        {
            settings.radial_corner_bias_tl_ui = std::clamp(settings.radial_corner_bias_tl_slider->value(), 0, 100);
        }
        if(settings.radial_corner_bias_tr_slider)
        {
            settings.radial_corner_bias_tr_ui = std::clamp(settings.radial_corner_bias_tr_slider->value(), 0, 100);
        }
        if(settings.radial_corner_bias_bl_slider)
        {
            settings.radial_corner_bias_bl_ui = std::clamp(settings.radial_corner_bias_bl_slider->value(), 0, 100);
        }
        if(settings.radial_corner_bias_br_slider)
        {
            settings.radial_corner_bias_br_ui = std::clamp(settings.radial_corner_bias_br_slider->value(), 0, 100);
        }
        if(settings.corner_blend_strength_slider)
        {
            settings.corner_blend_strength_pct =
                std::clamp((float)settings.corner_blend_strength_slider->value(), 0.0f, 100.0f);
        }
        if(settings.corner_blend_zone_slider)
        {
            settings.corner_blend_zone_pct =
                std::clamp((float)settings.corner_blend_zone_slider->value(), 0.0f, 32.0f);
        }
        
        bool old_calibration_pattern = settings.show_calibration_pattern;
        bool old_screen_preview = settings.show_screen_preview;
        if(settings.calibration_pattern_check) settings.show_calibration_pattern = settings.calibration_pattern_check->isChecked();
        if(settings.screen_preview_check) settings.show_screen_preview = settings.screen_preview_check->isChecked();
        if(settings.show_calibration_pattern && settings.group_box && !settings.group_box->isChecked())
        {
            QSignalBlocker block(settings.group_box);
            settings.group_box->setChecked(true);
            settings.enabled = true;
        }
        if(settings.show_screen_preview && settings.group_box && !settings.group_box->isChecked())
        {
            QSignalBlocker block(settings.group_box);
            settings.group_box->setChecked(true);
            settings.enabled = true;
        }

        if((old_calibration_pattern != settings.show_calibration_pattern || old_screen_preview != settings.show_screen_preview) 
           && settings.capture_area_preview)
        {
            settings.capture_area_preview->update();
        }

        if(settings.ref_point_combo)
        {
            int index = settings.ref_point_combo->currentIndex();
            if(index >= 0)
            {
                settings.reference_point_id = settings.ref_point_combo->itemData(index).toInt();
            }
        }
    }

    RefreshMonitorStatus();
    RefreshReferencePointDropdowns();

    emit ParametersChanged();
    in_parameter_change_ = false;
}

void ScreenMirror::OnScreenPreviewChanged()
{
    bool any_enabled = false;
    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end() && !any_enabled;
        ++it)
    {
        if(it->second.show_screen_preview && it->second.enabled)
        {
            any_enabled = true;
        }
    }
    emit ScreenPreviewChanged(any_enabled);
}

void ScreenMirror::OnCalibrationPatternChanged()
{
    bool any_enabled = false;
    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end() && !any_enabled;
        ++it)
    {
        if(it->second.show_calibration_pattern && it->second.enabled)
        {
            any_enabled = true;
        }
    }
    emit CalibrationPatternChanged(any_enabled);
    
    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        MonitorSettings& settings = it->second;
        if(settings.capture_area_preview)
        {
            settings.capture_area_preview->update();
        }
    }
}

bool ScreenMirror::ShouldShowCalibrationPattern(const std::string& plane_name) const
{
    std::map<std::string, MonitorSettings>::const_iterator it = monitor_settings.find(plane_name);
    if(it != monitor_settings.end())
    {
        const MonitorSettings& m = it->second;
        const bool row_on = m.group_box ? m.group_box->isChecked() : m.enabled;
        return m.show_calibration_pattern && row_on;
    }
    return false;
}

bool ScreenMirror::ShouldShowScreenPreview(const std::string& plane_name) const
{
    std::map<std::string, MonitorSettings>::const_iterator it = monitor_settings.find(plane_name);
    if(it != monitor_settings.end())
    {
        const MonitorSettings& m = it->second;
        const bool row_on = m.group_box ? m.group_box->isChecked() : m.enabled;
        return m.show_screen_preview && row_on;
    }
    return false;
}

void ScreenMirror::SetReferencePoints(std::vector<std::unique_ptr<VirtualReferencePoint3D>>* ref_points)
{
    reference_points = ref_points;
    if(monitors_layout && monitor_settings.size() > 0)
    {
        bool has_ui_widgets = false;
        for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin(); it != monitor_settings.end(); ++it)
        {
            if(it->second.ref_point_combo)
            {
                has_ui_widgets = true;
                break;
            }
        }
        if(has_ui_widgets)
        {
            RefreshReferencePointDropdowns();
        }
    }
}

void ScreenMirror::RefreshReferencePointDropdowns()
{
    if(!reference_points || !monitors_layout)
    {
        return;
    }

    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        MonitorSettings& settings = it->second;
        if(!settings.ref_point_combo)
        {
            continue;
        }

        int desired_id = settings.reference_point_id;

        settings.ref_point_combo->blockSignals(true);
        settings.ref_point_combo->clear();

        settings.ref_point_combo->addItem("Room Center", QVariant(-1));
        settings.ref_point_combo->setItemData(0,
            "Falloff distance is measured from the room center.",
            Qt::ToolTipRole);

        for(size_t i = 0; i < reference_points->size(); i++)
        {
            VirtualReferencePoint3D* ref_point = (*reference_points)[i].get();
            if(!ref_point) continue;

            QString name = QString::fromStdString(ref_point->GetName());
            QString type = QString(VirtualReferencePoint3D::GetTypeName(ref_point->GetType()));
            QString display = QString("%1 (%2)").arg(name, type);
            settings.ref_point_combo->addItem(display, QVariant(ref_point->GetId()));
            const int row = settings.ref_point_combo->count() - 1;
            settings.ref_point_combo->setItemData(row,
                QStringLiteral("Measure reach/falloff from \"%1\" for this monitor.").arg(name),
                Qt::ToolTipRole);
        }

        int restore_index = settings.ref_point_combo->findData(QVariant(desired_id));
        if(restore_index < 0)
        {
            settings.reference_point_id = -1;
            restore_index = settings.ref_point_combo->findData(QVariant(-1));
        }
        if(restore_index >= 0)
        {
            settings.ref_point_combo->setCurrentIndex(restore_index);
        }

        settings.ref_point_combo->blockSignals(false);
    }
}

bool ScreenMirror::ResolveReferencePointById(int id, Vector3D& out) const
{
    if(!reference_points || id <= 0)
    {
        return false;
    }

    for(size_t i = 0; i < reference_points->size(); i++)
    {
        VirtualReferencePoint3D* ref_point = (*reference_points)[i].get();
        if(ref_point && ref_point->GetId() == id)
        {
            out = ref_point->GetPosition();
            return true;
        }
    }
    return false;
}

int ScreenMirror::LookupReferencePointIdByIndex(int index) const
{
    if(!reference_points || index < 0 || index >= (int)reference_points->size())
    {
        return -1;
    }
    VirtualReferencePoint3D* ref_point = (*reference_points)[index].get();
    return ref_point ? ref_point->GetId() : -1;
}

void ScreenMirror::AddFrameToHistory(const std::string& capture_id, const std::shared_ptr<CapturedFrame>& frame)
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

    const size_t max_frames = 180;
    if(history.frames.size() > max_frames)
    {
        history.frames.pop_front();
    }
}

std::shared_ptr<CapturedFrame> ScreenMirror::GetFrameForDelay(const std::string& capture_id, float delay_ms) const
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

float ScreenMirror::GetHistoryRetentionMs() const
{
    float max_retention = 600.0f;
    
    for(std::map<std::string, MonitorSettings>::const_iterator it = monitor_settings.begin();
        it != monitor_settings.end();
        ++it)
    {
        const MonitorSettings& mon_settings = it->second;
        if(!mon_settings.enabled) continue;
        
        float monitor_retention = std::max(mon_settings.wave_decay_ms * 3.0f, mon_settings.smoothing_time_ms * 3.0f);
        if(mon_settings.propagation_speed_mm_per_ms >= 5.0f || mon_settings.wave_time_to_edge_sec > 0.4f)
        {
            float max_distance_mm = 5000.0f;
            if(mon_settings.wave_time_to_edge_sec > 0.4f)
            {
                float t_sec = std::clamp(mon_settings.wave_time_to_edge_sec, 0.5f, 10.0f);
                monitor_retention = std::max(monitor_retention, t_sec * 1000.0f);
            }
            else
            {
                float speed_mm_per_ms = WaveIntensityToSpeedMmPerMs(mon_settings.propagation_speed_mm_per_ms);
                if(speed_mm_per_ms > 0.0f)
                    monitor_retention = std::max(monitor_retention, max_distance_mm / speed_mm_per_ms);
            }
            monitor_retention = std::max(monitor_retention, mon_settings.wave_decay_ms * 2.0f);
        }
        max_retention = std::max(max_retention, monitor_retention);
    }
    
    return std::max(max_retention, 600.0f);
}

ScreenMirror::LEDKey ScreenMirror::MakeLEDKey(float x, float y, float z) const
{
    const float quantize_scale = 1000.0f;
    LEDKey key;
    key.x = (int)std::lround(x * quantize_scale);
    key.y = (int)std::lround(y * quantize_scale);
    key.z = (int)std::lround(z * quantize_scale);
    return key;
}

void ScreenMirror::CreateMonitorSettingsUI(DisplayPlane3D* plane, MonitorSettings& settings)
{
    if(!plane || !monitors_layout)
    {
        return;
    }

    std::string plane_name = plane->GetName();
    bool has_capture_source = !plane->GetCaptureSourceId().empty();

    QString display_name = QString::fromStdString(plane_name);
    if(!has_capture_source)
    {
        display_name += " (No Capture Source)";
    }
    settings.group_box = new QGroupBox(display_name);
    settings.group_box->setCheckable(true);
    settings.group_box->setChecked(settings.enabled &&
                                   (has_capture_source || settings.show_calibration_pattern || settings.show_screen_preview));
    settings.group_box->setEnabled(has_capture_source || settings.show_calibration_pattern || settings.show_screen_preview);
    if(has_capture_source)
    {
        settings.group_box->setToolTip("Enable or disable this monitor's influence.");
    }
    else
    {
        settings.group_box->setToolTip("This monitor needs a capture source assigned in Display Plane settings.");
    }
    connect(settings.group_box, &QGroupBox::toggled, this, &ScreenMirror::OnParameterChanged);

    QVBoxLayout* main_layout = new QVBoxLayout();
    main_layout->setContentsMargins(8, 4, 8, 4);
    main_layout->setSpacing(8);

    QGroupBox* reach_group = new QGroupBox("Reach & Falloff");
    QFormLayout* reach_form = new QFormLayout(reach_group);
    reach_form->setContentsMargins(8, 12, 8, 8);

    QWidget* scale_widget = new QWidget();
    QHBoxLayout* scale_layout = new QHBoxLayout(scale_widget);
    scale_layout->setContentsMargins(0, 0, 0, 0);
    settings.scale_slider = new QSlider(Qt::Horizontal);
    settings.scale_slider->setEnabled(has_capture_source);
    settings.scale_slider->setRange(0, 300);
    settings.scale_slider->setValue((int)(settings.scale * 100));
    settings.scale_slider->setTickPosition(QSlider::TicksBelow);
    settings.scale_slider->setTickInterval(25);
    settings.scale_slider->setToolTip("Global reach: 0-100% = fill room, 101-300% = beyond room (extreme).");
    scale_layout->addWidget(settings.scale_slider);
    settings.scale_label = new QLabel(QString::number((int)(settings.scale * 100)) + "%");
    settings.scale_label->setMinimumWidth(50);
    scale_layout->addWidget(settings.scale_label);
    connect(settings.scale_slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);
    connect(settings.scale_slider, &QSlider::valueChanged, settings.scale_label, [&settings](int value) {
        settings.scale_label->setText(QString::number(value) + "%");
    });
    reach_form->addRow("Global Reach:", scale_widget);

    settings.scale_invert_check = new QCheckBox("Invert Scale Falloff");
    settings.scale_invert_check->setEnabled(has_capture_source);
    settings.scale_invert_check->setChecked(settings.scale_inverted);
    settings.scale_invert_check->setToolTip("Invert the distance falloff (closer = dimmer, farther = brighter).");
    connect(settings.scale_invert_check, &QCheckBox::toggled, this, &ScreenMirror::OnParameterChanged);
    reach_form->addRow("", settings.scale_invert_check);

    QWidget* falloff_curve_widget = new QWidget();
    QHBoxLayout* falloff_curve_layout = new QHBoxLayout(falloff_curve_widget);
    falloff_curve_layout->setContentsMargins(0, 0, 0, 0);
    settings.falloff_curve_slider = new QSlider(Qt::Horizontal);
    settings.falloff_curve_slider->setRange(50, 200);
    settings.falloff_curve_slider->setValue((int)(settings.falloff_curve_exponent * 100.0f));
    settings.falloff_curve_slider->setEnabled(has_capture_source);
    settings.falloff_curve_slider->setTickPosition(QSlider::TicksBelow);
    settings.falloff_curve_slider->setTickInterval(25);
    settings.falloff_curve_slider->setToolTip("Falloff curve: 50% = softer (gradual), 100% = linear, 200% = sharper (sudden edge).");
    falloff_curve_layout->addWidget(settings.falloff_curve_slider);
    settings.falloff_curve_label = new QLabel(QString::number((int)(settings.falloff_curve_exponent * 100.0f)) + "%");
    settings.falloff_curve_label->setMinimumWidth(45);
    falloff_curve_layout->addWidget(settings.falloff_curve_label);
    connect(settings.falloff_curve_slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);
    connect(settings.falloff_curve_slider, &QSlider::valueChanged, settings.falloff_curve_label, [&settings](int value) {
        settings.falloff_curve_label->setText(QString::number(value) + "%");
    });
    reach_form->addRow("Falloff curve:", falloff_curve_widget);

    settings.ref_point_combo = new QComboBox();
    settings.ref_point_combo->addItem("Room Center", QVariant(-1));
    settings.ref_point_combo->setItemData(0,
        "Falloff distance is measured from the room center. Named reference points appear here when available.",
        Qt::ToolTipRole);
    settings.ref_point_combo->setEnabled(has_capture_source);
    settings.ref_point_combo->setToolTip(
        "Anchor for reach/falloff: room center or a saved reference point. "
        "The list refreshes when reference points change (see 3D layout / reference points).");
    connect(settings.ref_point_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ScreenMirror::OnParameterChanged);
    if(settings.reference_point_id <= 0)
    {
        int plane_ref_id = LookupReferencePointIdByIndex(plane->GetReferencePointIndex());
        if(plane_ref_id > 0)
        {
            settings.reference_point_id = plane_ref_id;
        }
    }
    reach_form->addRow("Reference:", settings.ref_point_combo);

    QWidget* softness_widget = new QWidget();
    QHBoxLayout* softness_layout = new QHBoxLayout(softness_widget);
    softness_layout->setContentsMargins(0, 0, 0, 0);
    settings.softness_slider = new QSlider(Qt::Horizontal);
    settings.softness_slider->setRange(0, 100);
    settings.softness_slider->setValue((int)settings.edge_softness);
    settings.softness_slider->setEnabled(has_capture_source);
    settings.softness_slider->setTickPosition(QSlider::TicksBelow);
    settings.softness_slider->setTickInterval(10);
    settings.softness_slider->setToolTip("Edge feathering (0 = hard, 100 = very soft).");
    softness_layout->addWidget(settings.softness_slider);
    settings.softness_label = new QLabel(QString::number((int)settings.edge_softness));
    settings.softness_label->setMinimumWidth(30);
    softness_layout->addWidget(settings.softness_label);
    connect(settings.softness_slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);
    connect(settings.softness_slider, &QSlider::valueChanged, settings.softness_label, [&settings](int value) {
        settings.softness_label->setText(QString::number(value));
    });
    reach_form->addRow("Softness:", softness_widget);

    main_layout->addWidget(reach_group);

    QGroupBox* brightness_group = new QGroupBox("Brightness");
    brightness_group->setObjectName("ScreenMirror_BrightnessGroup");
    QFormLayout* brightness_form = new QFormLayout(brightness_group);
    brightness_form->setContentsMargins(8, 12, 8, 8);

    QWidget* brightness_widget = new QWidget();
    QHBoxLayout* brightness_layout = new QHBoxLayout(brightness_widget);
    brightness_layout->setContentsMargins(0, 0, 0, 0);
    settings.brightness_slider = new QSlider(Qt::Horizontal);
    settings.brightness_slider->setRange(0, 200);
    settings.brightness_slider->setValue((int)(settings.brightness_multiplier * 100));
    settings.brightness_slider->setEnabled(has_capture_source);
    settings.brightness_slider->setTickPosition(QSlider::TicksBelow);
    settings.brightness_slider->setTickInterval(25);
    settings.brightness_slider->setToolTip("Overall output level (0-200%). 100% = neutral. Use White rolloff to reduce wash and keep colors vibrant.");
    brightness_layout->addWidget(settings.brightness_slider);
    settings.brightness_label = new QLabel(QString::number((int)(settings.brightness_multiplier * 100)) + "%");
    settings.brightness_label->setMinimumWidth(50);
    brightness_layout->addWidget(settings.brightness_label);
    connect(settings.brightness_slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);
    connect(settings.brightness_slider, &QSlider::valueChanged, settings.brightness_label, [&settings](int value) {
        settings.brightness_label->setText(QString::number(value) + "%");
    });
    brightness_form->addRow("Brightness:", brightness_widget);

    QWidget* threshold_widget = new QWidget();
    QHBoxLayout* threshold_layout = new QHBoxLayout(threshold_widget);
    threshold_layout->setContentsMargins(0, 0, 0, 0);
    settings.brightness_threshold_slider = new QSlider(Qt::Horizontal);
    settings.brightness_threshold_slider->setRange(0, 255);
    settings.brightness_threshold_slider->setValue((int)settings.brightness_threshold);
    settings.brightness_threshold_slider->setEnabled(has_capture_source);
    settings.brightness_threshold_slider->setTickPosition(QSlider::TicksBelow);
    settings.brightness_threshold_slider->setTickInterval(25);
    settings.brightness_threshold_slider->setToolTip("Minimum brightness (0-255). 0 = all content, 128 = mid, 255 = only bright content.");
    threshold_layout->addWidget(settings.brightness_threshold_slider);
    settings.brightness_threshold_label = new QLabel(QString::number((int)settings.brightness_threshold));
    settings.brightness_threshold_label->setMinimumWidth(50);
    threshold_layout->addWidget(settings.brightness_threshold_label);
    connect(settings.brightness_threshold_slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);
    connect(settings.brightness_threshold_slider, &QSlider::valueChanged, settings.brightness_threshold_label, [&settings](int value) {
        settings.brightness_threshold_label->setText(QString::number(value));
    });
    brightness_form->addRow("Brightness Threshold:", threshold_widget);

    QWidget* white_rolloff_widget = new QWidget();
    QHBoxLayout* white_rolloff_layout = new QHBoxLayout(white_rolloff_widget);
    white_rolloff_layout->setContentsMargins(0, 0, 0, 0);
    settings.white_rolloff_slider = new QSlider(Qt::Horizontal);
    settings.white_rolloff_slider->setRange(0, 100);
    settings.white_rolloff_slider->setValue((int)std::lround(settings.white_rolloff * 100.0f));
    settings.white_rolloff_slider->setEnabled(has_capture_source);
    settings.white_rolloff_slider->setTickPosition(QSlider::TicksBelow);
    settings.white_rolloff_slider->setTickInterval(10);
    settings.white_rolloff_slider->setToolTip("How much to reduce white/gray wash (0-100%). 0 = no rolloff, 100% = strong; keeps RGB/CYM vibrant, only big bright moments pop.");
    white_rolloff_layout->addWidget(settings.white_rolloff_slider);
    settings.white_rolloff_label = new QLabel(QString::number((int)std::lround(settings.white_rolloff * 100.0f)) + "%");
    settings.white_rolloff_label->setMinimumWidth(45);
    white_rolloff_layout->addWidget(settings.white_rolloff_label);
    connect(settings.white_rolloff_slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);
    connect(settings.white_rolloff_slider, &QSlider::valueChanged, settings.white_rolloff_label, [&settings](int value) {
        settings.white_rolloff_label->setText(QString::number(value) + "%");
    });
    brightness_form->addRow("White rolloff:", white_rolloff_widget);

    QWidget* vibrance_widget = new QWidget();
    QHBoxLayout* vibrance_layout = new QHBoxLayout(vibrance_widget);
    vibrance_layout->setContentsMargins(0, 0, 0, 0);
    settings.vibrance_slider = new QSlider(Qt::Horizontal);
    settings.vibrance_slider->setRange(0, 200);
    settings.vibrance_slider->setValue((int)std::lround(settings.vibrance * 100.0f));
    settings.vibrance_slider->setEnabled(has_capture_source);
    settings.vibrance_slider->setTickPosition(QSlider::TicksBelow);
    settings.vibrance_slider->setTickInterval(25);
    settings.vibrance_slider->setToolTip("Saturation (0-200%). 100% = no change, below 100% = more muted, above 100% = more vivid RGB/CYM.");
    vibrance_layout->addWidget(settings.vibrance_slider);
    settings.vibrance_label = new QLabel(QString::number((int)std::lround(settings.vibrance * 100.0f)) + "%");
    settings.vibrance_label->setMinimumWidth(45);
    vibrance_layout->addWidget(settings.vibrance_label);
    connect(settings.vibrance_slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);
    connect(settings.vibrance_slider, &QSlider::valueChanged, settings.vibrance_label, [&settings](int value) {
        settings.vibrance_label->setText(QString::number(value) + "%");
    });
    brightness_form->addRow("Vibrance:", vibrance_widget);

    main_layout->addWidget(brightness_group);

    QGroupBox* blackbars_group = new QGroupBox("Black Bars (Crop)");
    QFormLayout* blackbars_form = new QFormLayout(blackbars_group);
    blackbars_form->setContentsMargins(8, 12, 8, 8);

    QWidget* letterbox_widget = new QWidget();
    QHBoxLayout* letterbox_layout = new QHBoxLayout(letterbox_widget);
    letterbox_layout->setContentsMargins(0, 0, 0, 0);
    settings.black_bar_letterbox_slider = new QSlider(Qt::Horizontal);
    settings.black_bar_letterbox_slider->setRange(0, 50);
    settings.black_bar_letterbox_slider->setValue((int)std::lround(settings.black_bar_letterbox_percent));
    settings.black_bar_letterbox_slider->setEnabled(has_capture_source);
    settings.black_bar_letterbox_slider->setTickPosition(QSlider::TicksBelow);
    settings.black_bar_letterbox_slider->setTickInterval(5);
    settings.black_bar_letterbox_slider->setToolTip("Crop top and bottom (letterbox). 0 = no crop.");
    letterbox_layout->addWidget(settings.black_bar_letterbox_slider);
    settings.black_bar_letterbox_label = new QLabel(QString::number((int)std::lround(settings.black_bar_letterbox_percent)) + "%");
    settings.black_bar_letterbox_label->setMinimumWidth(40);
    letterbox_layout->addWidget(settings.black_bar_letterbox_label);
    connect(settings.black_bar_letterbox_slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);
    connect(settings.black_bar_letterbox_slider, &QSlider::valueChanged, settings.black_bar_letterbox_label, [&settings](int value) {
        settings.black_bar_letterbox_label->setText(QString::number(value) + "%");
    });
    blackbars_form->addRow("Letterbox (top/bottom):", letterbox_widget);

    QWidget* pillarbox_widget = new QWidget();
    QHBoxLayout* pillarbox_layout = new QHBoxLayout(pillarbox_widget);
    pillarbox_layout->setContentsMargins(0, 0, 0, 0);
    settings.black_bar_pillarbox_slider = new QSlider(Qt::Horizontal);
    settings.black_bar_pillarbox_slider->setRange(0, 50);
    settings.black_bar_pillarbox_slider->setValue((int)std::lround(settings.black_bar_pillarbox_percent));
    settings.black_bar_pillarbox_slider->setEnabled(has_capture_source);
    settings.black_bar_pillarbox_slider->setTickPosition(QSlider::TicksBelow);
    settings.black_bar_pillarbox_slider->setTickInterval(5);
    settings.black_bar_pillarbox_slider->setToolTip("Crop left and right (pillarbox). 0 = no crop.");
    pillarbox_layout->addWidget(settings.black_bar_pillarbox_slider);
    settings.black_bar_pillarbox_label = new QLabel(QString::number((int)std::lround(settings.black_bar_pillarbox_percent)) + "%");
    settings.black_bar_pillarbox_label->setMinimumWidth(40);
    pillarbox_layout->addWidget(settings.black_bar_pillarbox_label);
    connect(settings.black_bar_pillarbox_slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);
    connect(settings.black_bar_pillarbox_slider, &QSlider::valueChanged, settings.black_bar_pillarbox_label, [&settings](int value) {
        settings.black_bar_pillarbox_label->setText(QString::number(value) + "%");
    });
    blackbars_form->addRow("Pillarbox (left/right):", pillarbox_widget);

    main_layout->addWidget(blackbars_group);

    QGroupBox* radial_corner_group = new QGroupBox("Radial corner mapping");
    QVBoxLayout* radial_corner_outer = new QVBoxLayout();
    QLabel* radial_corner_info = new QLabel(
        "Per-display UV tweak after radial sampling. Directional mapping is calibrated in geometry (panel basis vs. capture); "
        "Map roll is only extra twist. Corner sliders 0–100%: 0% = tuned baseline; 50% ≈ neutral warp.");
    radial_corner_info->setWordWrap(true);
    PluginUiApplyItalicSecondaryLabel(radial_corner_info);
    radial_corner_outer->addWidget(radial_corner_info);

    {
        QWidget* roll_row = new QWidget();
        QHBoxLayout* roll_layout = new QHBoxLayout(roll_row);
        roll_layout->setContentsMargins(0, 0, 0, 0);
        roll_layout->addWidget(new QLabel("Map roll:"));
        settings.screen_map_roll_slider = new QSlider(Qt::Horizontal);
        settings.screen_map_roll_slider->setRange(-kScreenMapRollSliderMax, kScreenMapRollSliderMax);
        settings.screen_map_roll_slider->setSingleStep(1);
        settings.screen_map_roll_slider->setPageStep(4);
        settings.screen_map_roll_slider->setEnabled(has_capture_source);
        settings.screen_map_roll_slider->setValue(
            (int)std::lround(std::clamp(settings.screen_map_roll_deg, -180.0f, 180.0f) *
                             (float)kScreenMapRollTicksPerDegree));
        settings.screen_map_roll_slider->setToolTip(
            "Optional extra twist in the capture plane after calibrated directional mapping. 0° = geometry only. Steps 0.5°.");
        settings.screen_map_roll_label = new QLabel(
            QString::number(std::clamp(settings.screen_map_roll_deg, -180.0f, 180.0f), 'f', 1) + QChar(0x00B0));
        settings.screen_map_roll_label->setMinimumWidth(52);
        roll_layout->addWidget(settings.screen_map_roll_slider, 1);
        roll_layout->addWidget(settings.screen_map_roll_label);
        connect(settings.screen_map_roll_slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);
        connect(settings.screen_map_roll_slider, &QSlider::valueChanged, this, [&settings](int v) {
            if(settings.screen_map_roll_label)
            {
                const float deg = (float)v / (float)kScreenMapRollTicksPerDegree;
                settings.screen_map_roll_label->setText(QString::number(deg, 'f', 1) + QChar(0x00B0));
            }
        });
        radial_corner_outer->addWidget(roll_row);
    }

    auto add_radial_pct_row = [&](const char* title, QSlider*& slider, QLabel*& label, int stored_ui, const char* tip) {
        QWidget* row = new QWidget();
        QHBoxLayout* row_layout = new QHBoxLayout(row);
        row_layout->setContentsMargins(0, 0, 0, 0);
        row_layout->addWidget(new QLabel(title));
        slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, 100);
        slider->setValue(std::clamp(stored_ui, 0, 100));
        slider->setEnabled(has_capture_source);
        slider->setToolTip(tip);
        label = new QLabel(QString::number(std::clamp(stored_ui, 0, 100)) + "%");
        label->setMinimumWidth(44);
        QLabel* lp = label;
        row_layout->addWidget(slider, 1);
        row_layout->addWidget(label);
        connect(slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);
        connect(slider, &QSlider::valueChanged, this, [lp](int v) { lp->setText(QString::number(v) + "%"); });
        radial_corner_outer->addWidget(row);
    };

    add_radial_pct_row("Corner expansion:",
                       settings.radial_corner_expansion_slider,
                       settings.radial_corner_expansion_label,
                       settings.radial_corner_expansion_ui,
                       "0% = baseline mapping; raise to push toward corners or lower to pinch (internal −50…+50).");
    add_radial_pct_row("Bottom-left:",
                       settings.radial_corner_bias_tl_slider,
                       settings.radial_corner_bias_tl_label,
                       settings.radial_corner_bias_tl_ui,
                       "Bias toward capture bottom-left in that quadrant; 0% = baseline.");
    add_radial_pct_row("Bottom-right:",
                       settings.radial_corner_bias_tr_slider,
                       settings.radial_corner_bias_tr_label,
                       settings.radial_corner_bias_tr_ui,
                       "Bias toward capture bottom-right in that quadrant; 0% = baseline.");
    add_radial_pct_row("Top-left:",
                       settings.radial_corner_bias_bl_slider,
                       settings.radial_corner_bias_bl_label,
                       settings.radial_corner_bias_bl_ui,
                       "Bias toward capture top-left in that quadrant; 0% = baseline.");
    add_radial_pct_row("Top-right:",
                       settings.radial_corner_bias_br_slider,
                       settings.radial_corner_bias_br_label,
                       settings.radial_corner_bias_br_ui,
                       "Bias toward capture top-right in that quadrant; 0% = baseline.");

    radial_corner_group->setLayout(radial_corner_outer);
    main_layout->addWidget(radial_corner_group);

    QGroupBox* corner_sample_group = new QGroupBox("Corner blend (sampling)");
    QVBoxLayout* corner_sample_outer = new QVBoxLayout();
    QWidget* corner_strength_row = new QWidget();
    QHBoxLayout* corner_strength_layout = new QHBoxLayout(corner_strength_row);
    corner_strength_layout->setContentsMargins(0, 0, 0, 0);
    corner_strength_layout->addWidget(new QLabel("Strength:"));
    settings.corner_blend_strength_slider = new QSlider(Qt::Horizontal);
    settings.corner_blend_strength_slider->setRange(0, 100);
    settings.corner_blend_strength_slider->setValue((int)std::lround(settings.corner_blend_strength_pct));
    settings.corner_blend_strength_slider->setEnabled(has_capture_source);
    settings.corner_blend_strength_slider->setToolTip(
        "Mix edge colors near frame corners when reading the capture. 0% = off (center sample only); 100% = full blend.");
    settings.corner_blend_strength_label =
        new QLabel(QString::number((int)std::lround(settings.corner_blend_strength_pct)) + "%");
    settings.corner_blend_strength_label->setMinimumWidth(40);
    corner_strength_layout->addWidget(settings.corner_blend_strength_slider, 1);
    corner_strength_layout->addWidget(settings.corner_blend_strength_label);
    connect(settings.corner_blend_strength_slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);
    connect(settings.corner_blend_strength_slider, &QSlider::valueChanged, this, [&settings](int v) {
        if(settings.corner_blend_strength_label)
        {
            settings.corner_blend_strength_label->setText(QString::number(v) + "%");
        }
    });
    corner_sample_outer->addWidget(corner_strength_row);

    QWidget* corner_zone_row = new QWidget();
    QHBoxLayout* corner_zone_layout = new QHBoxLayout(corner_zone_row);
    corner_zone_layout->setContentsMargins(0, 0, 0, 0);
    corner_zone_layout->addWidget(new QLabel("Zone width:"));
    settings.corner_blend_zone_slider = new QSlider(Qt::Horizontal);
    settings.corner_blend_zone_slider->setRange(0, 32);
    settings.corner_blend_zone_slider->setValue((int)std::lround(settings.corner_blend_zone_pct));
    settings.corner_blend_zone_slider->setEnabled(has_capture_source);
    settings.corner_blend_zone_slider->setToolTip(
        "0% = off. Otherwise corner transition size as % of half the active image (after letterbox). Higher = wider, softer corner.");
    settings.corner_blend_zone_label =
        new QLabel(QString::number((int)std::lround(settings.corner_blend_zone_pct)) + "%");
    settings.corner_blend_zone_label->setMinimumWidth(40);
    corner_zone_layout->addWidget(settings.corner_blend_zone_slider, 1);
    corner_zone_layout->addWidget(settings.corner_blend_zone_label);
    connect(settings.corner_blend_zone_slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);
    connect(settings.corner_blend_zone_slider, &QSlider::valueChanged, this, [&settings](int v) {
        if(settings.corner_blend_zone_label)
        {
            settings.corner_blend_zone_label->setText(QString::number(v) + "%");
        }
    });
    corner_sample_outer->addWidget(corner_zone_row);
    corner_sample_group->setLayout(corner_sample_outer);
    main_layout->addWidget(corner_sample_group);

    QGroupBox* blend_group = new QGroupBox("Blend & Smoothing");
    QFormLayout* blend_form = new QFormLayout(blend_group);
    blend_form->setContentsMargins(8, 12, 8, 8);

    QWidget* smoothing_widget = new QWidget();
    QHBoxLayout* smoothing_layout = new QHBoxLayout(smoothing_widget);
    smoothing_layout->setContentsMargins(0, 0, 0, 0);
    settings.smoothing_time_slider = new QSlider(Qt::Horizontal);
    settings.smoothing_time_slider->setRange(0, 500);
    settings.smoothing_time_slider->setValue((int)settings.smoothing_time_ms);
    settings.smoothing_time_slider->setEnabled(has_capture_source);
    settings.smoothing_time_slider->setTickPosition(QSlider::TicksBelow);
    settings.smoothing_time_slider->setTickInterval(50);
    settings.smoothing_time_slider->setToolTip("Temporal smoothing to reduce flicker (0-500ms).");
    smoothing_layout->addWidget(settings.smoothing_time_slider);
    settings.smoothing_time_label = new QLabel(QString::number((int)settings.smoothing_time_ms) + "ms");
    settings.smoothing_time_label->setMinimumWidth(50);
    smoothing_layout->addWidget(settings.smoothing_time_label);
    connect(settings.smoothing_time_slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);
    connect(settings.smoothing_time_slider, &QSlider::valueChanged, settings.smoothing_time_label, [&settings](int value) {
        settings.smoothing_time_label->setText(QString::number(value) + "ms");
    });
    blend_form->addRow("Smoothing:", smoothing_widget);

    QWidget* blend_widget = new QWidget();
    QHBoxLayout* blend_layout = new QHBoxLayout(blend_widget);
    blend_layout->setContentsMargins(0, 0, 0, 0);
    settings.blend_slider = new QSlider(Qt::Horizontal);
    settings.blend_slider->setRange(0, 100);
    settings.blend_slider->setValue((int)settings.blend);
    settings.blend_slider->setEnabled(has_capture_source);
    settings.blend_slider->setTickPosition(QSlider::TicksBelow);
    settings.blend_slider->setTickInterval(10);
    settings.blend_slider->setToolTip("Blend with other monitors (0 = isolated, 100 = fully shared).");
    blend_layout->addWidget(settings.blend_slider);
    settings.blend_label = new QLabel(QString::number((int)settings.blend));
    settings.blend_label->setMinimumWidth(30);
    blend_layout->addWidget(settings.blend_label);
    connect(settings.blend_slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);
    connect(settings.blend_slider, &QSlider::valueChanged, settings.blend_label, [&settings](int value) {
        settings.blend_label->setText(QString::number(value));
    });
    blend_form->addRow("Blend:", blend_widget);

    main_layout->addWidget(blend_group);

    QGroupBox* preview_group = new QGroupBox("Preview & Calibration");
    QFormLayout* preview_form = new QFormLayout(preview_group);
    preview_form->setContentsMargins(8, 12, 8, 8);

    settings.calibration_pattern_check = new QCheckBox("Show calibration pattern");
    settings.calibration_pattern_check->setEnabled(true);
    settings.calibration_pattern_check->setChecked(settings.show_calibration_pattern);
    settings.calibration_pattern_check->setToolTip(
        "LEDs use a grid, rings, spokes, and quadrant colors (same as the zone preview). "
        "Tune radial corners, map roll, and corner blend until geometry looks straight and corners behave like the preview.");
    connect(settings.calibration_pattern_check,
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
            &QCheckBox::checkStateChanged,
            this, [this]() { OnParameterChanged(); OnCalibrationPatternChanged(); }
#else
            &QCheckBox::stateChanged,
            this, [this](int) { OnParameterChanged(); OnCalibrationPatternChanged(); }
#endif
    );
    preview_form->addRow("Calibration pattern:", settings.calibration_pattern_check);

    settings.screen_preview_check = new QCheckBox("Show Screen Preview");
    settings.screen_preview_check->setEnabled(has_capture_source);
    settings.screen_preview_check->setChecked(settings.show_screen_preview);
    settings.screen_preview_check->setToolTip("Show captured screen on display planes in the 3D viewport. Turn off to save CPU/GPU.");
    connect(settings.screen_preview_check,
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
            &QCheckBox::checkStateChanged,
            this, [this]() { OnParameterChanged(); OnScreenPreviewChanged(); }
#else
            &QCheckBox::stateChanged,
            this, [this](int) { OnParameterChanged(); OnScreenPreviewChanged(); }
#endif
    );
    preview_form->addRow("Screen Preview:", settings.screen_preview_check);

    main_layout->addWidget(preview_group);

    CaptureZonesWidget* zones_widget = new CaptureZonesWidget(
        &settings.capture_zones,
        plane,
        &settings.show_calibration_pattern,
        &settings.show_screen_preview,
        &settings.black_bar_letterbox_percent,
        &settings.black_bar_pillarbox_percent,
        settings.propagation_speed_mm_per_ms,
        settings.wave_decay_ms,
        settings.wave_time_to_edge_sec,
        settings.front_back_balance,
        settings.left_right_balance,
        settings.top_bottom_balance,
        this);
    zones_widget->setEnabled(has_capture_source);
    zones_widget->SetValueChangedCallback([this]() { OnParameterChanged(); });
    connect(zones_widget, &CaptureZonesWidget::valueChanged, this, &ScreenMirror::OnParameterChanged);

    settings.capture_zones_widget = zones_widget;
    settings.propagation_speed_slider = zones_widget->getPropagationSpeedSlider();
    settings.propagation_speed_label = zones_widget->getPropagationSpeedLabel();
    settings.wave_decay_slider = zones_widget->getWaveDecaySlider();
    settings.wave_decay_label = zones_widget->getWaveDecayLabel();
    settings.wave_time_to_edge_slider = zones_widget->getWaveTimeToEdgeSlider();
    settings.wave_time_to_edge_label = zones_widget->getWaveTimeToEdgeLabel();
    settings.front_back_balance_slider = zones_widget->getFrontBackBalanceSlider();
    settings.front_back_balance_label = zones_widget->getFrontBackBalanceLabel();
    settings.left_right_balance_slider = zones_widget->getLeftRightBalanceSlider();
    settings.left_right_balance_label = zones_widget->getLeftRightBalanceLabel();
    settings.top_bottom_balance_slider = zones_widget->getTopBottomBalanceSlider();
    settings.top_bottom_balance_label = zones_widget->getTopBottomBalanceLabel();
    settings.capture_area_preview = zones_widget->getPreviewWidget();
    settings.add_zone_button = zones_widget->getAddZoneButton();

    if(settings.propagation_speed_slider)
        connect(settings.propagation_speed_slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);
    if(settings.wave_decay_slider)
        connect(settings.wave_decay_slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);
    if(settings.wave_time_to_edge_slider)
        connect(settings.wave_time_to_edge_slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);
    if(settings.front_back_balance_slider)
        connect(settings.front_back_balance_slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);
    if(settings.left_right_balance_slider)
        connect(settings.left_right_balance_slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);
    if(settings.top_bottom_balance_slider)
        connect(settings.top_bottom_balance_slider, &QSlider::valueChanged, this, &ScreenMirror::OnParameterChanged);

    main_layout->addWidget(zones_widget);

    settings.group_box->setLayout(main_layout);
    monitors_layout->addWidget(settings.group_box);
    
}

void ScreenMirror::SetGridScaleMM(float mm)
{
    float v = (mm > 0.001f) ? mm : 10.0f;
    if(grid_scale_mm_ == v) return;
    grid_scale_mm_ = v;
}

void ScreenMirror::RefreshMonitorStatus()
{
    if(!monitor_status_label) return;

    std::vector<DisplayPlane3D*> planes = DisplayPlaneManager::instance()->GetDisplayPlanes();
    int total_count = 0;
    int active_count = 0;
    
    for(unsigned int plane_index = 0; plane_index < planes.size(); plane_index++)
    {
        DisplayPlane3D* plane = planes[plane_index];
        if(!plane) continue;
        
        total_count++;
        bool has_capture_source = !plane->GetCaptureSourceId().empty();
        if(has_capture_source)
        {
            active_count++;
        }
        
        std::string plane_name = plane->GetName();
        std::map<std::string, MonitorSettings>::iterator settings_it = monitor_settings.find(plane_name);
        if(settings_it == monitor_settings.end())
        {
            MonitorSettings new_settings;
            int plane_ref_id = LookupReferencePointIdByIndex(plane->GetReferencePointIndex());
            if(plane_ref_id > 0)
            {
                new_settings.reference_point_id = plane_ref_id;
            }
            new_settings.enabled = DefaultMonitorEnabledForPlane(plane);
            settings_it = monitor_settings.emplace(plane_name, new_settings).first;
        }
        MonitorSettings& settings = settings_it->second;

        if(settings.reference_point_id <= 0)
        {
            int plane_ref_id = LookupReferencePointIdByIndex(plane->GetReferencePointIndex());
            if(plane_ref_id > 0)
            {
                settings.reference_point_id = plane_ref_id;
            }
        }

        if(!settings.group_box && monitors_container && monitors_layout)
        {
            CreateMonitorSettingsUI(plane, settings);
        }
        else if(settings.group_box)
        {
            QString display_name = QString::fromStdString(plane_name);
            if(!has_capture_source)
            {
                display_name += " (No Capture Source)";
            }
            settings.group_box->setTitle(display_name);
            settings.group_box->setEnabled(has_capture_source || settings.show_calibration_pattern || settings.show_screen_preview);
            
            if(has_capture_source)
            {
                settings.group_box->setToolTip("Enable or disable this monitor's influence.");
            }
            else
            {
                settings.group_box->setToolTip("This monitor needs a capture source assigned in Display Plane settings.");
            }
            
            if(settings.scale_slider) settings.scale_slider->setEnabled(has_capture_source);
            if(settings.ref_point_combo) settings.ref_point_combo->setEnabled(has_capture_source);
            if(settings.softness_slider) settings.softness_slider->setEnabled(has_capture_source);
            if(settings.blend_slider) settings.blend_slider->setEnabled(has_capture_source);
            if(settings.scale_slider) settings.scale_slider->setEnabled(has_capture_source);
            if(settings.scale_invert_check) settings.scale_invert_check->setEnabled(has_capture_source);
            if(settings.smoothing_time_slider) settings.smoothing_time_slider->setEnabled(has_capture_source);
            if(settings.brightness_slider) settings.brightness_slider->setEnabled(has_capture_source);
            if(settings.brightness_threshold_slider) settings.brightness_threshold_slider->setEnabled(has_capture_source);
            if(settings.black_bar_letterbox_slider) settings.black_bar_letterbox_slider->setEnabled(has_capture_source);
            if(settings.black_bar_pillarbox_slider) settings.black_bar_pillarbox_slider->setEnabled(has_capture_source);
            if(settings.softness_slider) settings.softness_slider->setEnabled(has_capture_source);
            if(settings.blend_slider) settings.blend_slider->setEnabled(has_capture_source);
            if(settings.propagation_speed_slider) settings.propagation_speed_slider->setEnabled(has_capture_source);
            if(settings.wave_decay_slider) settings.wave_decay_slider->setEnabled(has_capture_source);
            if(settings.wave_time_to_edge_slider) settings.wave_time_to_edge_slider->setEnabled(has_capture_source);
            if(settings.falloff_curve_slider) settings.falloff_curve_slider->setEnabled(has_capture_source);
            if(settings.front_back_balance_slider) settings.front_back_balance_slider->setEnabled(has_capture_source);
            if(settings.left_right_balance_slider) settings.left_right_balance_slider->setEnabled(has_capture_source);
            if(settings.top_bottom_balance_slider) settings.top_bottom_balance_slider->setEnabled(has_capture_source);
            if(settings.ref_point_combo) settings.ref_point_combo->setEnabled(has_capture_source);
            if(settings.calibration_pattern_check) settings.calibration_pattern_check->setEnabled(true);
            if(settings.screen_preview_check) settings.screen_preview_check->setEnabled(has_capture_source);
            if(settings.capture_zones_widget)
            {
                settings.capture_zones_widget->setEnabled(has_capture_source);
                settings.capture_zones_widget->SetDisplayPlane(plane);
                settings.capture_zones_widget->SetCaptureZones(&settings.capture_zones);
            }
            if(settings.add_zone_button)
                settings.add_zone_button->setEnabled(has_capture_source);
            
        }
    }

    QString status_text;
    if(total_count == 0)
    {
        status_text = "No Display Planes configured";
    }
    else if(active_count == 0)
    {
        status_text = QString("Display Planes: %1 (none have capture sources)").arg(total_count);
    }
    else
    {
        status_text = QString("Display Planes: %1 total, %2 active").arg(total_count).arg(active_count);
    }
    monitor_status_label->setText(status_text);

    QWidget* parent = monitor_status_label->parentWidget();
    if(parent)
    {
        QGroupBox* status_group = qobject_cast<QGroupBox*>(parent);
        if(status_group && status_group->layout())
        {
            if(total_count > 0 && active_count == 0)
            {
                if(!monitor_help_label)
                {
                    monitor_help_label = new QLabel("Tip: Assign capture sources to Display Planes in the Object Creator tab.");
                    monitor_help_label->setWordWrap(true);
                    PluginUiApplyItalicSecondaryLabel(monitor_help_label);
                    status_group->layout()->addWidget(monitor_help_label);
                }
            }
            else
            {
                if(monitor_help_label)
                {
                    monitor_help_label->deleteLater();
                    monitor_help_label = nullptr;
                }
            }
        }
    }
}
