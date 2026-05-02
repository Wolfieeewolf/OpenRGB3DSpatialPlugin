// SPDX-License-Identifier: GPL-2.0-only

#include "ScreenMirror.h"
#include "ScreenCaptureManager.h"
#include "DisplayPlane3D.h"
#include "DisplayPlaneManager.h"
#include "Geometry3DUtils.h"
#include "GridSpaceUtils.h"
#include "VirtualReferencePoint3D.h"
#include "OpenRGB3DSpatialTab.h"
#include "StratumBandPanel.h"
#include "SpatialLayerCore.h"
#include "PluginUiUtils.h"

REGISTER_EFFECT_3D(ScreenMirror);

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
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
#include <unordered_set>

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
    , show_test_pattern(false)
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
        s.test_pattern_check = nullptr;
        s.screen_preview_check = nullptr;
        s.capture_area_preview = nullptr;
        s.add_zone_button = nullptr;
        s.capture_zones_widget = nullptr;
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
    capture_group->setLayout(capture_layout);
    main_layout->addWidget(capture_group);
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

    stratum_panel = new StratumBandPanel(container);
    stratum_panel->setLayoutMode(stratum_layout_mode);
    stratum_panel->setTuning(stratum_tuning_);
    main_layout->addWidget(stratum_panel);
    connect(stratum_panel, &StratumBandPanel::bandParametersChanged, this, &ScreenMirror::OnStratumBandChanged);
    OnStratumBandChanged();

    main_layout->addStretch();

    AddWidgetToParent(container, parent);

    /* Capture threads start only when the effect runs (CalculateColorGrid). Starting DXGI/GDI here
     * only to populate the UI caused a brief full-screen compositor flicker on some GPUs. */
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

namespace
{
    size_t MirrorOverlayCoarseIndex(int i, int j, int k, int cy, int cz)
    {
        return (size_t)i * (size_t)cy * (size_t)cz + (size_t)j * (size_t)cz + (size_t)k;
    }

    float LerpF(float a, float b, float t)
    {
        return (1.0f - t) * a + t * b;
    }

    RGBColor TrilinearMirrorCoarse(const std::vector<RGBColor>& vol, int cx, int cy, int cz, float fx, float fy, float fz)
    {
        if(vol.empty() || cx < 1 || cy < 1 || cz < 1)
        {
            return ToRGBColor(0, 0, 0);
        }
        fx = std::clamp(fx, 0.0f, (float)(std::max(0, cx - 1)));
        fy = std::clamp(fy, 0.0f, (float)(std::max(0, cy - 1)));
        fz = std::clamp(fz, 0.0f, (float)(std::max(0, cz - 1)));
        const int x0 = (std::min)((int)std::floor(fx), std::max(0, cx - 2));
        const int y0 = (std::min)((int)std::floor(fy), std::max(0, cy - 2));
        const int z0 = (std::min)((int)std::floor(fz), std::max(0, cz - 2));
        const int x1 = (cx > 1) ? (std::min)(x0 + 1, cx - 1) : x0;
        const int y1 = (cy > 1) ? (std::min)(y0 + 1, cy - 1) : y0;
        const int z1 = (cz > 1) ? (std::min)(z0 + 1, cz - 1) : z0;
        const float tx = (cx > 1) ? (fx - (float)x0) : 0.0f;
        const float ty = (cy > 1) ? (fy - (float)y0) : 0.0f;
        const float tz = (cz > 1) ? (fz - (float)z0) : 0.0f;

        RGBColor c000 = vol[MirrorOverlayCoarseIndex(x0, y0, z0, cy, cz)];
        RGBColor c100 = vol[MirrorOverlayCoarseIndex(x1, y0, z0, cy, cz)];
        RGBColor c010 = vol[MirrorOverlayCoarseIndex(x0, y1, z0, cy, cz)];
        RGBColor c110 = vol[MirrorOverlayCoarseIndex(x1, y1, z0, cy, cz)];
        RGBColor c001 = vol[MirrorOverlayCoarseIndex(x0, y0, z1, cy, cz)];
        RGBColor c101 = vol[MirrorOverlayCoarseIndex(x1, y0, z1, cy, cz)];
        RGBColor c011 = vol[MirrorOverlayCoarseIndex(x0, y1, z1, cy, cz)];
        RGBColor c111 = vol[MirrorOverlayCoarseIndex(x1, y1, z1, cy, cz)];

        const float r00 = LerpF((float)RGBGetRValue(c000), (float)RGBGetRValue(c100), tx);
        const float g00 = LerpF((float)RGBGetGValue(c000), (float)RGBGetGValue(c100), tx);
        const float b00 = LerpF((float)RGBGetBValue(c000), (float)RGBGetBValue(c100), tx);
        const float r10 = LerpF((float)RGBGetRValue(c010), (float)RGBGetRValue(c110), tx);
        const float g10 = LerpF((float)RGBGetGValue(c010), (float)RGBGetGValue(c110), tx);
        const float b10 = LerpF((float)RGBGetBValue(c010), (float)RGBGetBValue(c110), tx);
        const float r01 = LerpF((float)RGBGetRValue(c001), (float)RGBGetRValue(c101), tx);
        const float g01 = LerpF((float)RGBGetGValue(c001), (float)RGBGetGValue(c101), tx);
        const float b01 = LerpF((float)RGBGetBValue(c001), (float)RGBGetBValue(c101), tx);
        const float r11 = LerpF((float)RGBGetRValue(c011), (float)RGBGetRValue(c111), tx);
        const float g11 = LerpF((float)RGBGetGValue(c011), (float)RGBGetGValue(c111), tx);
        const float b11 = LerpF((float)RGBGetBValue(c011), (float)RGBGetBValue(c111), tx);

        const float r0 = LerpF(r00, r10, ty);
        const float g0 = LerpF(g00, g10, ty);
        const float b0 = LerpF(b00, b10, ty);
        const float r1 = LerpF(r01, r11, ty);
        const float g1 = LerpF(g01, g11, ty);
        const float b1 = LerpF(b01, b11, ty);

        const float rf = LerpF(r0, r1, tz);
        const float gf = LerpF(g0, g1, tz);
        const float bf = LerpF(b0, b1, tz);
        return ToRGBColor((uint8_t)std::clamp((int)std::lround(rf), 0, 255),
                          (uint8_t)std::clamp((int)std::lround(gf), 0, 255),
                          (uint8_t)std::clamp((int)std::lround(bf), 0, 255));
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

void ScreenMirror::PrepareMirrorRoomGridOverlay(uint64_t render_sequence,
                                                 float time,
                                                 int nx,
                                                 int ny,
                                                 int nz,
                                                 float /*room_min_x*/,
                                                 float /*room_max_x*/,
                                                 float /*room_min_y*/,
                                                 float /*room_max_y*/,
                                                 float /*room_min_z*/,
                                                 float /*room_max_z*/,
                                                 const GridContext3D& world_grid,
                                                 const GridContext3D& /*room_grid*/)
{
    GridContext3D wg = world_grid;
    if(render_sequence != 0)
    {
        wg.render_sequence = render_sequence;
    }
    RefreshFrameCacheForRenderSequence(wg);

    if(nx < 1 || ny < 1 || nz < 1)
    {
        mirror_overlay_coarse_.clear();
        mirror_overlay_cx_ = mirror_overlay_cy_ = mirror_overlay_cz_ = 0;
        mirror_overlay_pass_seq_ = 0;
        return;
    }

    const int denom_x = (std::max)(nx - 1, 1);
    const int denom_y = (std::max)(ny - 1, 1);
    const int denom_z = (std::max)(nz - 1, 1);

    int stride = 1;
    const long long fine_total = (long long)nx * (long long)ny * (long long)nz;
    if(fine_total > 12000) { stride = 2; }
    if(fine_total > 48000) { stride = 3; }
    if(fine_total > 120000) { stride = 4; }

    mirror_overlay_stride_ = stride;
    mirror_overlay_cx_ = (nx + stride - 1) / stride;
    mirror_overlay_cy_ = (ny + stride - 1) / stride;
    mirror_overlay_cz_ = (nz + stride - 1) / stride;
    mirror_overlay_nx_ = nx;
    mirror_overlay_ny_ = ny;
    mirror_overlay_nz_ = nz;
    mirror_overlay_pass_seq_ = render_sequence;

    const size_t nc = (size_t)mirror_overlay_cx_ * (size_t)mirror_overlay_cy_ * (size_t)mirror_overlay_cz_;
    mirror_overlay_coarse_.resize(nc);

    for(int ic = 0; ic < mirror_overlay_cx_; ic++)
    {
        const int ix_lo = ic * stride;
        const int ix_hi = (std::min)(ix_lo + stride - 1, nx - 1);
        const int ix_c = (ix_lo + ix_hi) / 2;
        const float u = (float)ix_c / (float)denom_x;
        const float wx = world_grid.min_x + u * (world_grid.max_x - world_grid.min_x);
        for(int jc = 0; jc < mirror_overlay_cy_; jc++)
        {
            const int iy_lo = jc * stride;
            const int iy_hi = (std::min)(iy_lo + stride - 1, ny - 1);
            const int iy_c = (iy_lo + iy_hi) / 2;
            const float v = (float)iy_c / (float)denom_y;
            const float wy = world_grid.min_y + v * (world_grid.max_y - world_grid.min_y);
            for(int kc = 0; kc < mirror_overlay_cz_; kc++)
            {
                const int iz_lo = kc * stride;
                const int iz_hi = (std::min)(iz_lo + stride - 1, nz - 1);
                const int iz_c = (iz_lo + iz_hi) / 2;
                const float wn = (float)iz_c / (float)denom_z;
                const float wz = world_grid.min_z + wn * (world_grid.max_z - world_grid.min_z);

                const size_t idx =
                    (size_t)ic * (size_t)mirror_overlay_cy_ * (size_t)mirror_overlay_cz_ +
                    (size_t)jc * (size_t)mirror_overlay_cz_ + (size_t)kc;
                mirror_overlay_coarse_[idx] =
                    CalculateColorGridInternal(wx, wy, wz, time, world_grid, &frame_cache_, &frame_cache_planes_, false);
            }
        }
    }
}

RGBColor ScreenMirror::SampleMirrorRoomGridOverlay(uint64_t render_sequence, int ix, int iy, int iz, int nx, int ny, int nz) const
{
    if(render_sequence != mirror_overlay_pass_seq_ || mirror_overlay_coarse_.empty())
    {
        return ToRGBColor(0, 0, 0);
    }
    if(nx != mirror_overlay_nx_ || ny != mirror_overlay_ny_ || nz != mirror_overlay_nz_)
    {
        return ToRGBColor(0, 0, 0);
    }
    if(ix < 0 || iy < 0 || iz < 0 || ix >= nx || iy >= ny || iz >= nz)
    {
        return ToRGBColor(0, 0, 0);
    }
    const float fx = ((float)ix + 0.5f) * (float)mirror_overlay_cx_ / (float)nx - 0.5f;
    const float fy = ((float)iy + 0.5f) * (float)mirror_overlay_cy_ / (float)ny - 0.5f;
    const float fz = ((float)iz + 0.5f) * (float)mirror_overlay_cz_ / (float)nz - 0.5f;
    return TrilinearMirrorCoarse(mirror_overlay_coarse_, mirror_overlay_cx_, mirror_overlay_cy_, mirror_overlay_cz_, fx, fy, fz);
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
        bool use_test_pattern;
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

    Vector3D stratum_origin = GetEffectOriginGrid(grid);
    Vector3D rp_stratum = TransformPointByRotation(x, y, z, stratum_origin);
    const float stratum_y_norm = NormalizeGridAxis01(rp_stratum.y, grid.min_y, grid.max_y);
    SpatialLayerCore::MapperSettings strat_st;
    EffectStratumBlend::InitStratumBreaks(strat_st);
    float stratum_w[3];
    EffectStratumBlend::WeightsForYNorm(stratum_y_norm, strat_st, stratum_w);
    const EffectStratumBlend::BandBlendScalars bb =
        EffectStratumBlend::BlendBands(stratum_layout_mode, stratum_w, stratum_tuning_);
    
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

        bool monitor_test_pattern = mon_settings.show_test_pattern;
        
        std::string capture_id = plane->GetCaptureSourceId();
        std::shared_ptr<CapturedFrame> frame = nullptr;

        if(!monitor_test_pattern)
        {
            if(capture_id.empty())
            {
                continue;
            }
            if(frame_cache)
            {
                auto it = frame_cache->find(capture_id);
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

        Geometry3D::PlaneProjection proj = Geometry3D::SpatialMapToScreen(led_pos, *plane, 0.0f, falloff_ref, scale_mm, true);

        if(!proj.is_valid) continue;

        float u = proj.u;
        float v = proj.v;
        
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
        {
            const float ph_uv = (bb.phase_deg / 360.0f) * 0.02f;
            u = std::clamp(u + ph_uv, 0.0f, 1.0f);
            v = std::clamp(v + ph_uv * (stratum_y_norm - 0.5f) * 2.0f, 0.0f, 1.0f);
        }
        
        proj.u = u;
        proj.v = v;

        float monitor_scale = std::clamp(mon_settings.scale, 0.0f, 3.0f);
        float coverage = monitor_scale;
        float curve_exp = std::clamp(mon_settings.falloff_curve_exponent, 0.5f, 2.0f);
        float distance_falloff = 0.0f;
        const float edge_soft_stratum = mon_settings.edge_softness / std::max(0.25f, bb.tight_mul);

        if(mon_settings.scale_inverted)
        {
            if(coverage > 0.0001f)
            {
                float effective_range = reference_max_distance_mm * coverage;
                effective_range = std::max(effective_range, 10.0f);
                distance_falloff = Geometry3D::ComputeFalloff(proj.distance, effective_range, edge_soft_stratum);
            }
        }
        else
        {
            distance_falloff = ComputeInvertedShellFalloff(proj.distance, reference_max_distance_mm, coverage, edge_soft_stratum, curve_exp);

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
        bool use_wave = !monitor_test_pattern && !capture_id.empty();
        if(use_wave && mon_settings.wave_time_to_edge_sec > 0.4f)
        {
            float t_sec = std::clamp(mon_settings.wave_time_to_edge_sec, 0.5f, 10.0f);
            speed_mm_per_ms = reference_max_distance_mm / (t_sec * 1000.0f);
            speed_mm_per_ms *= std::max(0.05f, bb.speed_mul);
            speed_mm_per_ms = std::max(speed_mm_per_ms, 0.1f);
            delay_ms = proj.distance / speed_mm_per_ms;
            delay_ms = std::clamp(delay_ms, 0.0f, 60000.0f);
        }
        else if(use_wave && mon_settings.propagation_speed_mm_per_ms >= 5.0f)
        {
            speed_mm_per_ms = WaveIntensityToSpeedMmPerMs(mon_settings.propagation_speed_mm_per_ms);
            speed_mm_per_ms *= std::max(0.05f, bb.speed_mul);
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
                    speed_mm_per_ms *= std::max(0.05f, bb.speed_mul);
                    delay_ms = proj.distance / std::max(speed_mm_per_ms, 0.1f);
                }
                else
                {
                    speed_mm_per_ms = WaveIntensityToSpeedMmPerMs(mon_settings.propagation_speed_mm_per_ms);
                    speed_mm_per_ms *= std::max(0.05f, bb.speed_mul);
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
            contrib.use_test_pattern = mon_settings.show_test_pattern;
            contributions.push_back(contrib);
        }
    }

    if(contributions.empty())
    {
        if(show_test_pattern)
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

        float r, g, b;

        if(contrib.use_test_pattern)
        {
            float clamped_u = std::clamp(sample_u, 0.0f, 1.0f);
            float clamped_v = std::clamp(sample_v, 0.0f, 1.0f);
            const unsigned int samp = GetSamplingResolution();
            if(samp < 100u)
            {
                Geometry3D::QuantizeMediaUV01(clamped_u, clamped_v, 256, 256, samp);
            }

            bool left_half = (clamped_u < 0.5f);
            bool bottom_half = (clamped_v < 0.5f);

            if(bottom_half && left_half)
            {
                r = 255.0f;
                g = 0.0f;
                b = 0.0f;
            }
            else if(bottom_half && !left_half)
            {
                r = 0.0f;
                g = 255.0f;
                b = 0.0f;
            }
            else if(!bottom_half && !left_half)
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
            float u_s = sample_u_clamped;
            float v_s = flipped_v;
            if(samp < 100u)
            {
                Geometry3D::QuantizeMediaUV01(u_s, v_s, contrib.frame->width, contrib.frame->height, samp);
            }

            RGBColor sampled_color = Geometry3D::SampleFrame(
                contrib.frame->data.data(),
                contrib.frame->width,
                contrib.frame->height,
                u_s,
                v_s,
                true
            );

            r = (float)RGBGetRValue(sampled_color);
            g = (float)RGBGetGValue(sampled_color);
            b = (float)RGBGetBValue(sampled_color);

            if(contrib.frame_blend && !contrib.frame_blend->data.empty() && contrib.blend_t > 0.01f)
            {
                float u_b = std::clamp(sample_u, u_min, u_max);
                float v_b = std::clamp(flipped_v, v_min, v_max);
                if(samp < 100u)
                {
                    Geometry3D::QuantizeMediaUV01(u_b, v_b, contrib.frame_blend->width, contrib.frame_blend->height, samp);
                }
                RGBColor sampled_blend = Geometry3D::SampleFrame(
                    contrib.frame_blend->data.data(),
                    contrib.frame_blend->width,
                    contrib.frame_blend->height,
                    u_b, v_b, true
                );
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
        mon["show_test_pattern"] = mon_settings.show_test_pattern;
        mon["show_screen_preview"] = mon_settings.show_screen_preview;
        
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

    int sm = stratum_layout_mode;
    EffectStratumBlend::BandTuningPct st = stratum_tuning_;
    if(stratum_panel)
    {
        sm = stratum_panel->layoutMode();
        st = stratum_panel->tuning();
    }
    EffectStratumBlend::SaveBandTuningJson(settings,
                                           "screen_mirror_stratum_layout_mode",
                                           sm,
                                           st,
                                           "screen_mirror_stratum_band_speed_pct",
                                           "screen_mirror_stratum_band_tight_pct",
                                           "screen_mirror_stratum_band_phase_deg");

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
    if(msettings.test_pattern_check)
    {
        QSignalBlocker blocker(msettings.test_pattern_check);
        msettings.test_pattern_check->setChecked(msettings.show_test_pattern);
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

    EffectStratumBlend::LoadBandTuningJson(settings,
                                            "screen_mirror_stratum_layout_mode",
                                            stratum_layout_mode,
                                            stratum_tuning_,
                                            "screen_mirror_stratum_band_speed_pct",
                                            "screen_mirror_stratum_band_tight_pct",
                                            "screen_mirror_stratum_band_phase_deg");

    if(!settings.contains("monitor_settings"))
    {
        if(stratum_panel)
        {
            stratum_panel->setLayoutMode(stratum_layout_mode);
            stratum_panel->setTuning(stratum_tuning_);
        }
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
            QCheckBox* existing_test_pattern_check = nullptr;
            QCheckBox* existing_screen_preview_check = nullptr;
            QWidget* existing_capture_area_preview = nullptr;
            QPushButton* existing_add_zone_button = nullptr;
            CaptureZonesWidget* existing_capture_zones_widget = nullptr;

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
            }
            else
            {
                monitor_settings[monitor_name] = MonitorSettings();
                existing_it = monitor_settings.find(monitor_name);
            }
            MonitorSettings& msettings = existing_it->second;

            if(mon.contains("enabled")) msettings.enabled = mon["enabled"].get<bool>();
            
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
                float v = mon["brightness_threshold"].get<float>();
                if(v > 255.0f) v = v * 255.0f / 1020.0f;
                msettings.brightness_threshold = v;
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
            if(mon.contains("blend")) msettings.blend = mon["blend"].get<float>();
            if(mon.contains("propagation_speed_mm_per_ms"))
            {
                float v = mon["propagation_speed_mm_per_ms"].get<float>();
                if(v > 100.0f) v = v * 100.0f / 800.0f;
                msettings.propagation_speed_mm_per_ms = v;
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
            
            if(mon.contains("show_test_pattern")) msettings.show_test_pattern = mon["show_test_pattern"].get<bool>();
            if(mon.contains("show_screen_preview")) msettings.show_screen_preview = mon["show_screen_preview"].get<bool>();
            
            if(mon.contains("capture_zones") && mon["capture_zones"].is_array())
            {
                msettings.capture_zones.clear();
                for(const auto& zone_json : mon["capture_zones"])
                {
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
                msettings.test_pattern_check = existing_test_pattern_check;
                msettings.screen_preview_check = existing_screen_preview_check;
                msettings.capture_area_preview = existing_capture_area_preview;
                msettings.add_zone_button = existing_add_zone_button;
                msettings.capture_zones_widget = existing_capture_zones_widget;

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
    OnTestPatternChanged();
    
    OnParameterChanged();

    if(stratum_panel)
    {
        stratum_panel->setLayoutMode(stratum_layout_mode);
        stratum_panel->setTuning(stratum_tuning_);
    }
}

void ScreenMirror::OnStratumBandChanged()
{
    if(stratum_panel)
    {
        stratum_layout_mode = stratum_panel->layoutMode();
        stratum_tuning_ = stratum_panel->tuning();
    }
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
        
        bool old_test_pattern = settings.show_test_pattern;
        bool old_screen_preview = settings.show_screen_preview;
        if(settings.test_pattern_check) settings.show_test_pattern = settings.test_pattern_check->isChecked();
        if(settings.screen_preview_check) settings.show_screen_preview = settings.screen_preview_check->isChecked();
        if(settings.show_test_pattern && settings.group_box && !settings.group_box->isChecked())
        {
            QSignalBlocker block(settings.group_box);
            settings.group_box->setChecked(true);
            settings.enabled = true;
        }
        
        if((old_test_pattern != settings.show_test_pattern || old_screen_preview != settings.show_screen_preview) 
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

void ScreenMirror::OnTestPatternChanged()
{
    bool any_enabled = false;
    for(std::map<std::string, MonitorSettings>::iterator it = monitor_settings.begin();
        it != monitor_settings.end() && !any_enabled;
        ++it)
    {
        if(it->second.show_test_pattern && it->second.enabled)
        {
            any_enabled = true;
        }
    }
    emit TestPatternChanged(any_enabled);
    
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

bool ScreenMirror::ShouldShowTestPattern(const std::string& plane_name) const
{
    std::map<std::string, MonitorSettings>::const_iterator it = monitor_settings.find(plane_name);
    if(it != monitor_settings.end())
    {
        return it->second.show_test_pattern && it->second.enabled;
    }
    return false;
}

bool ScreenMirror::ShouldShowScreenPreview(const std::string& plane_name) const
{
    std::map<std::string, MonitorSettings>::const_iterator it = monitor_settings.find(plane_name);
    if(it != monitor_settings.end())
    {
        return it->second.show_screen_preview && it->second.enabled;
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
            // Selected ref point no longer exists; snap state back to "Room Center" so
            // persisted state stays in sync with what the dropdown shows.
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
    settings.group_box->setChecked(settings.enabled && (has_capture_source || settings.show_test_pattern));
    settings.group_box->setEnabled(has_capture_source || settings.show_test_pattern);
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

    QGroupBox* preview_group = new QGroupBox("Preview & Test");
    QFormLayout* preview_form = new QFormLayout(preview_group);
    preview_form->setContentsMargins(8, 12, 8, 8);

    settings.test_pattern_check = new QCheckBox("Show Test Pattern");
    settings.test_pattern_check->setEnabled(true);
    settings.test_pattern_check->setChecked(settings.show_test_pattern);
    settings.test_pattern_check->setToolTip("Display a fixed color quadrant pattern for calibration.");
    connect(settings.test_pattern_check,
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
            &QCheckBox::checkStateChanged,
            this, [this]() { OnParameterChanged(); OnTestPatternChanged(); }
#else
            &QCheckBox::stateChanged,
            this, [this](int) { OnParameterChanged(); OnTestPatternChanged(); }
#endif
    );
    preview_form->addRow("Test Pattern:", settings.test_pattern_check);

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
        &settings.show_test_pattern,
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
            settings.group_box->setEnabled(has_capture_source || settings.show_test_pattern);
            
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
            if(settings.test_pattern_check) settings.test_pattern_check->setEnabled(true);
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
