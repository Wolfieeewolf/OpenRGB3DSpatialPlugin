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

    // Auto-registration system
    EFFECT_REGISTERER_3D("ScreenMirror3D", "Screen Mirror 3D", "Ambilight", [](){return new ScreenMirror3D;});

    static std::string const ClassName() { return "ScreenMirror3D"; }
    static std::string const UIName() { return "Screen Mirror 3D"; }
    static void ForceLink() {}  // Dummy to force linker inclusion

    // Pure virtual implementations
    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return true; }
    bool RequiresWorldSpaceGridBounds() const override { return true; }

    /** Set grid scale (mm/unit) so ambilight preview uses same math as viewport. Tab calls this when grid scale changes. */
    void SetGridScaleMM(float mm);

    /** When set, ambilight preview uses this effect's CalculateColorGrid so the preview matches what the running effect outputs to LEDs. Tab sets this when displaying the effect. */
    void SetRunningEffectForPreview(ScreenMirror3D* source);

    // Settings persistence
    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

    // Reference Points Access
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
        float brightness_threshold;     // Minimum brightness to trigger (0-1020; at max ambilight nearly off)
        float black_bar_letterbox_percent;   // Crop top+bottom (0-50%). Content V = [letterbox/100, 1-letterbox/100]
        float black_bar_pillarbox_percent;   // Crop left+right (0-50%). Content U = [pillarbox/100, 1-pillarbox/100]
        
        // Light & Motion
        float edge_softness;            // Edge softness (0-100%)
        float blend;                    // Blend percentage (0-100%)
        float propagation_speed_mm_per_ms;  // Wave intensity 0-800 (0=off, higher=bigger wave)
        float wave_decay_ms;            // Wave decay time (0-20000ms)
        float wave_time_to_edge_sec;    // 0=use intensity; 0.5-30 = time (s) for wave to reach farthest LED
        float falloff_curve_exponent;   // 0.5-2.0: softer to sharper falloff (100%=linear)
        float front_back_balance;       // -100..+100: favor front(+) or back(-) LEDs
        float left_right_balance;       // -100..+100: favor right(+) or left(-) LEDs
        float top_bottom_balance;       // -100..+100: favor top(+) or bottom(-) LEDs
        
        int reference_point_index;
        
        // Preview settings
        bool show_test_pattern;      // Show test pattern for this monitor
        bool show_test_pattern_pulse;  // Pulse test pattern (for tuning wave intensity/decay)
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
        QSlider* black_bar_letterbox_slider;
        QLabel* black_bar_letterbox_label;
        QSlider* black_bar_pillarbox_slider;
        QLabel* black_bar_pillarbox_label;
        QSlider* softness_slider;
        QLabel* softness_label;
        QSlider* blend_slider;
        QLabel* blend_label;
        QSlider* propagation_speed_slider;
        QLabel* propagation_speed_label;
        QSlider* wave_decay_slider;
        QLabel* wave_decay_label;
        QSlider* wave_time_to_edge_slider;
        QLabel* wave_time_to_edge_label;
        QSlider* falloff_curve_slider;
        QLabel* falloff_curve_label;
        QSlider* front_back_balance_slider;
        QLabel* front_back_balance_label;
        QSlider* left_right_balance_slider;
        QLabel* left_right_balance_label;
        QSlider* top_bottom_balance_slider;
        QLabel* top_bottom_balance_label;
        QComboBox* ref_point_combo;
        QCheckBox* test_pattern_check;
        QCheckBox* test_pattern_pulse_check;
        QCheckBox* screen_preview_check;
        QWidget* capture_area_preview;
        QPushButton* add_zone_button;
        QWidget* ambilight_preview_3d;

        MonitorSettings()
            : enabled(true)
            , scale(1.0f)
            , scale_inverted(true)  // Invert scale falloff by default
            , smoothing_time_ms(50.0f)
            , brightness_multiplier(1.0f)
            , brightness_threshold(0.0f)
            , black_bar_letterbox_percent(0.0f)
            , black_bar_pillarbox_percent(0.0f)
            , edge_softness(30.0f)
            , blend(50.0f)
            , propagation_speed_mm_per_ms(0.0f)   // 0 = no wave (instant 3D ambilight); use sliders to add wave
            , wave_decay_ms(0.0f)
            , wave_time_to_edge_sec(0.0f)
            , falloff_curve_exponent(1.0f)
            , front_back_balance(0.0f)
            , left_right_balance(0.0f)
            , top_bottom_balance(0.0f)
            , reference_point_index(-1)
            , show_test_pattern(false)
            , show_test_pattern_pulse(false)
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
            , black_bar_letterbox_slider(nullptr)
            , black_bar_letterbox_label(nullptr)
            , black_bar_pillarbox_slider(nullptr)
            , black_bar_pillarbox_label(nullptr)
            , softness_slider(nullptr)
            , softness_label(nullptr)
            , blend_slider(nullptr)
            , blend_label(nullptr)
            , propagation_speed_slider(nullptr)
            , propagation_speed_label(nullptr)
            , wave_decay_slider(nullptr)
            , wave_decay_label(nullptr)
            , wave_time_to_edge_slider(nullptr)
            , wave_time_to_edge_label(nullptr)
            , falloff_curve_slider(nullptr)
            , falloff_curve_label(nullptr)
            , front_back_balance_slider(nullptr)
            , front_back_balance_label(nullptr)
            , left_right_balance_slider(nullptr)
            , left_right_balance_label(nullptr)
            , top_bottom_balance_slider(nullptr)
            , top_bottom_balance_label(nullptr)
            , ref_point_combo(nullptr)
            , test_pattern_check(nullptr)
            , test_pattern_pulse_check(nullptr)
            , screen_preview_check(nullptr)
            , capture_area_preview(nullptr)
            , add_zone_button(nullptr)
            , ambilight_preview_3d(nullptr)
        {
            // Default: one zone covering the entire screen
            capture_zones.push_back(CaptureZone(0.0f, 1.0f, 0.0f, 1.0f));
        }
    };

    void StartCaptureIfNeeded();
    void StopCaptureIfNeeded();
    void CreateMonitorSettingsUI(DisplayPlane3D* plane, MonitorSettings& settings);
    void SyncMonitorSettingsToUI(MonitorSettings& msettings);
    void UpdateAmbilightPreviews();

    // Capture quality (effect-level)
    int                 capture_quality;       // 0=Low 320x180 .. 5=1080p, 6=1440p, 7=4K
    QComboBox*          capture_quality_combo;

    // UI Controls
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
    QGroupBox*          monitors_container;
    QTimer*             ambilight_preview_timer;
    QVBoxLayout*        monitors_layout;        // Layout for monitor settings
    std::map<std::string, MonitorSettings> monitor_settings;

    float               grid_scale_mm_;         // Same as viewport for consistent preview/effect math

    /** When non-null, UpdateAmbilightPreviews uses this effect's CalculateColorGrid so the preview matches the running effect's LED output. */
    ScreenMirror3D*     source_effect_for_preview_;

    // Effect parameters
    bool                        show_test_pattern;

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

    // Helpers
    bool ResolveReferencePoint(int index, Vector3D& out) const;
    void AddFrameToHistory(const std::string& capture_id, const std::shared_ptr<CapturedFrame>& frame);
    std::shared_ptr<CapturedFrame> GetFrameForDelay(const std::string& capture_id, float delay_ms) const;
    float GetHistoryRetentionMs() const;
    LEDKey MakeLEDKey(float x, float y, float z) const;

    /** Internal: single-point color with optional pre-filled frame cache (nullptr = fetch per plane).
        If pre_fetched_planes != nullptr, use it instead of GetDisplayPlanes() (batch optimization). */
    RGBColor CalculateColorGridInternal(float x, float y, float z, float time, const GridContext3D& grid,
                                       const std::unordered_map<std::string, std::shared_ptr<CapturedFrame>>* frame_cache,
                                       const std::vector<DisplayPlane3D*>* pre_fetched_planes = nullptr);
    /** Build current frame per capture_id for batch sampling. Call once per batch. */
    void BuildFrameCache(std::unordered_map<std::string, std::shared_ptr<CapturedFrame>>* out_cache);
    /** Batch version: pre-fetches frames once, then computes color per position. Use for ambilight preview. */
    void CalculateColorGridBatch(const std::vector<Vector3D>& positions, float time, const GridContext3D& grid,
                                std::vector<RGBColor>& out);
};

#endif // SCREENMIRROR3D_H
