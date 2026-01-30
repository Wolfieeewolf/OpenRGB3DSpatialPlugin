// SPDX-License-Identifier: GPL-2.0-only

#ifndef SCREENMIRROR3D_H
#define SCREENMIRROR3D_H

#include <QWidget>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QCheckBox>
#include <QPaintEvent>
#include <QPainter>
#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include <map>
#include <unordered_map>
#include <deque>
#include <memory>
#include <string>
#include "ScreenCaptureManager.h"

// Forward declarations
class DisplayPlane3D;
class VirtualReferencePoint3D;
struct CaptureSourceInfo;
struct CapturedFrame;
class CaptureAreaPreviewWidget;

class ScreenMirror3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit ScreenMirror3D(QWidget* parent = nullptr);
    ~ScreenMirror3D();

    /*---------------------------------------------------------*\
    | Auto-registration system                                 |
    \*---------------------------------------------------------*/
    EFFECT_REGISTERER_3D("ScreenMirror3D", "Screen Mirror 3D", "Ambilight", [](){return new ScreenMirror3D;});

    static std::string const ClassName() { return "ScreenMirror3D"; }
    static std::string const UIName() { return "Screen Mirror 3D"; }
    static void ForceLink() {}  // Dummy to force linker inclusion

    /*---------------------------------------------------------*\
    | Pure virtual implementations                             |
    \*---------------------------------------------------------*/
    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return true; }
    bool RequiresWorldSpaceGridBounds() const override { return true; }

    /*---------------------------------------------------------*\
    | Settings persistence                                     |
    \*---------------------------------------------------------*/
    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

    /*---------------------------------------------------------*\
    | Reference Points Access                                  |
    \*---------------------------------------------------------*/
    void SetReferencePoints(std::vector<std::unique_ptr<VirtualReferencePoint3D>>* ref_points);
    void RefreshReferencePointDropdowns();
    void RefreshMonitorStatus();

    // Capture zone: a rectangular area on the screen (UV coordinates 0-1)
    struct CaptureZone
    {
        float u_min;      // Left edge (0.0 = left, 1.0 = right)
        float u_max;      // Right edge
        float v_min;      // Bottom edge (0.0 = bottom, 1.0 = top)
        float v_max;      // Top edge
        bool enabled;      // Is this zone active?
        std::string name;  // Optional name for the zone
        
        CaptureZone()
            : u_min(0.0f)
            , u_max(1.0f)
            , v_min(0.0f)
            , v_max(1.0f)
            , enabled(true)
            , name("Zone")
        {}
        
        CaptureZone(float u0, float u1, float v0, float v1)
            : u_min(std::min(u0, u1))
            , u_max(std::max(u0, u1))
            , v_min(std::min(v0, v1))
            , v_max(std::max(v0, v1))
            , enabled(true)
            , name("Zone")
        {}
        
        bool Contains(float u, float v) const
        {
            return enabled && u >= u_min && u <= u_max && v >= v_min && v <= v_max;
        }
    };

signals:
    void ScreenPreviewChanged(bool enabled);
    void TestPatternChanged(bool enabled);
    
public:
    // Methods for viewport to check per-monitor preview settings
    bool ShouldShowTestPattern(const std::string& plane_name) const;
    bool ShouldShowScreenPreview(const std::string& plane_name) const;

private slots:
    void OnParameterChanged();
    void OnScreenPreviewChanged();
    void OnTestPatternChanged();

private:
    struct MonitorSettings
    {
        bool enabled;
        
        // Global Reach / Scale
        float scale;                    // Scale/reach multiplier (0-2.0)
        bool scale_inverted;            // Invert scale falloff
        
        // Calibration
        float smoothing_time_ms;        // Temporal smoothing (0-500ms)
        float brightness_multiplier;    // Brightness multiplier (0-200%)
        float brightness_threshold;     // Minimum brightness to trigger (0-255)
        
        // Light & Motion
        float edge_softness;            // Edge softness (0-100%)
        float blend;                    // Blend percentage (0-100%)
        float propagation_speed_mm_per_ms;  // Wave propagation speed (0-200 mm/ms)
        float wave_decay_ms;            // Wave decay time (0-4000ms)
        
        int reference_point_index;
        
        // Preview settings
        bool show_test_pattern;      // Show test pattern for this monitor
        bool show_screen_preview;    // Show screen preview for this monitor
        
        // Multiple independent capture zones
        std::vector<CaptureZone> capture_zones;

        QGroupBox* group_box;
        QSlider* scale_slider;
        QLabel* scale_label;
        QCheckBox* scale_invert_check;
        QSlider* smoothing_time_slider;
        QLabel* smoothing_time_label;
        QSlider* brightness_slider;
        QLabel* brightness_label;
        QSlider* brightness_threshold_slider;
        QLabel* brightness_threshold_label;
        QSlider* softness_slider;
        QLabel* softness_label;
        QSlider* blend_slider;
        QLabel* blend_label;
        QSlider* propagation_speed_slider;
        QLabel* propagation_speed_label;
        QSlider* wave_decay_slider;
        QLabel* wave_decay_label;
        QComboBox* ref_point_combo;
        QCheckBox* test_pattern_check;
        QCheckBox* screen_preview_check;
        QWidget* capture_area_preview;
        QPushButton* add_zone_button;

        MonitorSettings()
            : enabled(true)
            , scale(1.0f)
            , scale_inverted(false)
            , smoothing_time_ms(50.0f)
            , brightness_multiplier(1.0f)
            , brightness_threshold(0.0f)
            , edge_softness(30.0f)
            , blend(50.0f)
            , propagation_speed_mm_per_ms(10.0f)
            , wave_decay_ms(1000.0f)  // Increased default for more noticeable wave
            , reference_point_index(-1)
            , show_test_pattern(false)
            , show_screen_preview(false)
            , group_box(nullptr)
            , scale_slider(nullptr)
            , scale_label(nullptr)
            , scale_invert_check(nullptr)
            , smoothing_time_slider(nullptr)
            , smoothing_time_label(nullptr)
            , brightness_slider(nullptr)
            , brightness_label(nullptr)
            , brightness_threshold_slider(nullptr)
            , brightness_threshold_label(nullptr)
            , softness_slider(nullptr)
            , softness_label(nullptr)
            , blend_slider(nullptr)
            , blend_label(nullptr)
            , propagation_speed_slider(nullptr)
            , propagation_speed_label(nullptr)
            , wave_decay_slider(nullptr)
            , wave_decay_label(nullptr)
            , ref_point_combo(nullptr)
            , test_pattern_check(nullptr)
            , screen_preview_check(nullptr)
            , capture_area_preview(nullptr)
            , add_zone_button(nullptr)
        {
            // Default: one zone covering the entire screen
            capture_zones.push_back(CaptureZone(0.0f, 1.0f, 0.0f, 1.0f));
        }
    };

    void StartCaptureIfNeeded();
    void StopCaptureIfNeeded();
    void CreateMonitorSettingsUI(DisplayPlane3D* plane, MonitorSettings& settings);

    /*---------------------------------------------------------*\
    | UI Controls                                              |
    \*---------------------------------------------------------*/
    QSlider*            global_scale_slider;
    QLabel*             global_scale_label;
    QSlider*            smoothing_time_slider;
    QLabel*             smoothing_time_label;
    QSlider*            brightness_slider;
    QLabel*             brightness_label;
    QSlider*            propagation_speed_slider;
    QLabel*             propagation_speed_label;
    QSlider*            wave_decay_slider;
    QLabel*             wave_decay_label;
    QSlider*            brightness_threshold_slider;
    QLabel*             brightness_threshold_label;
    QCheckBox*          global_scale_invert_check;
    QLabel*             monitor_status_label;
    QLabel*             monitor_help_label;
    QGroupBox*          monitors_container;     // Container for per-monitor settings
    QVBoxLayout*        monitors_layout;        // Layout for monitor settings
    std::map<std::string, MonitorSettings> monitor_settings;

    /*---------------------------------------------------------*\
    | Effect parameters                                        |
    \*---------------------------------------------------------*/
    // NOTE: The following global variables are kept ONLY for backward compatibility
    // when loading old settings files. They are NOT used in calculations.
    // All calculations now use per-monitor settings from monitor_settings map.
    float                       global_scale;           // DEPRECATED: Only for legacy loading
    float                       smoothing_time_ms;      // DEPRECATED: Only for legacy loading
    float                       brightness_multiplier;  // DEPRECATED: Only for legacy loading
    float                       brightness_threshold;   // DEPRECATED: Only for legacy loading
    float                       propagation_speed_mm_per_ms; // DEPRECATED: Only for legacy loading
    float                       wave_decay_ms;         // DEPRECATED: Only for legacy loading
    bool                        show_test_pattern;     // Still global (affects all monitors)
    // Per-monitor settings stored in monitor_settings map - these are used in all calculations

    // Reference points (pointer to main tab's vector)
    std::vector<std::unique_ptr<VirtualReferencePoint3D>>* reference_points;

    struct FrameHistory
    {
        std::deque<std::shared_ptr<CapturedFrame>> frames;
        float cached_avg_frame_time_ms;  // Cached frame rate to avoid recalculating
        uint64_t last_frame_rate_update; // Timestamp of last frame rate calculation
        
        FrameHistory()
            : cached_avg_frame_time_ms(16.67f)  // Default 60fps
            , last_frame_rate_update(0)
        {
        }
    };
    std::unordered_map<std::string, FrameHistory> capture_history;

    // Temporal smoothing per LED (simple exponential moving average)
    struct LEDKey
    {
        int x;
        int y;
        int z;

        bool operator<(const LEDKey& other) const
        {
            if(x != other.x) return x < other.x;
            if(y != other.y) return y < other.y;
            return z < other.z;
        }
    };

    struct LEDState
    {
        float r, g, b;
        uint64_t last_update_ms;
    };
    std::map<LEDKey, LEDState> led_states;  // Key by quantized LED position

    /*---------------------------------------------------------*\
    | Helpers                                                  |
    \*---------------------------------------------------------*/
    bool ResolveReferencePoint(int index, Vector3D& out) const;
    bool GetEffectReferencePoint(Vector3D& out) const;
    void AddFrameToHistory(const std::string& capture_id, const std::shared_ptr<CapturedFrame>& frame);
    std::shared_ptr<CapturedFrame> GetFrameForDelay(const std::string& capture_id, float delay_ms) const;
    float GetHistoryRetentionMs() const;
    LEDKey MakeLEDKey(float x, float y, float z) const;
};

#endif // SCREENMIRROR3D_H
