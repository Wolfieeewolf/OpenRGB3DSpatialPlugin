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

    EFFECT_REGISTERER_3D("ScreenMirror3D", "Screen Mirror 3D", "Ambilight", [](){return new ScreenMirror3D;});

    static std::string const ClassName() { return "ScreenMirror3D"; }
    static std::string const UIName() { return "Screen Mirror 3D"; }
    static void ForceLink() {}

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return true; }
    bool RequiresWorldSpaceGridBounds() const override { return true; }

    void SetGridScaleMM(float mm);
    void SetRunningEffectForPreview(ScreenMirror3D* source);

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

    void SetReferencePoints(std::vector<std::unique_ptr<VirtualReferencePoint3D>>* ref_points);
    void RefreshReferencePointDropdowns();
    void RefreshMonitorStatus();

    struct CaptureZone
    {
        float u_min;
        float u_max;
        float v_min;
        float v_max;
        bool enabled;
        std::string name;
        
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

        float scale;
        bool scale_inverted;

        float smoothing_time_ms;
        float brightness_multiplier;
        float brightness_threshold;
        float black_bar_letterbox_percent;
        float black_bar_pillarbox_percent;

        float edge_softness;
        float blend;
        float propagation_speed_mm_per_ms;
        float wave_decay_ms;
        float wave_time_to_edge_sec;
        float falloff_curve_exponent;
        float front_back_balance;
        float left_right_balance;
        float top_bottom_balance;

        int reference_point_index;

        bool show_test_pattern;
        bool show_test_pattern_pulse;
        bool show_screen_preview;

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
            , scale_inverted(true)
            , smoothing_time_ms(50.0f)
            , brightness_multiplier(1.0f)
            , brightness_threshold(0.0f)
            , black_bar_letterbox_percent(0.0f)
            , black_bar_pillarbox_percent(0.0f)
            , edge_softness(30.0f)
            , blend(50.0f)
            , propagation_speed_mm_per_ms(0.0f)
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
            capture_zones.push_back(CaptureZone(0.0f, 1.0f, 0.0f, 1.0f));
        }
    };

    void StartCaptureIfNeeded();
    void StopCaptureIfNeeded();
    void CreateMonitorSettingsUI(DisplayPlane3D* plane, MonitorSettings& settings);
    void SyncMonitorSettingsToUI(MonitorSettings& msettings);
    void UpdateAmbilightPreviews();

    int                 capture_quality;
    QComboBox*          capture_quality_combo;

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
    QVBoxLayout*        monitors_layout;
    std::map<std::string, MonitorSettings> monitor_settings;

    float               grid_scale_mm_;
    ScreenMirror3D*     source_effect_for_preview_;

    bool                        show_test_pattern;

    std::vector<std::unique_ptr<VirtualReferencePoint3D>>* reference_points;

    struct FrameHistory
    {
        std::deque<std::shared_ptr<CapturedFrame>> frames;
        float cached_avg_frame_time_ms;
        uint64_t last_frame_rate_update;

        FrameHistory()
            : cached_avg_frame_time_ms(16.67f)
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
    std::map<LEDKey, LEDState> led_states;

    bool ResolveReferencePoint(int index, Vector3D& out) const;
    void AddFrameToHistory(const std::string& capture_id, const std::shared_ptr<CapturedFrame>& frame);
    std::shared_ptr<CapturedFrame> GetFrameForDelay(const std::string& capture_id, float delay_ms) const;
    float GetHistoryRetentionMs() const;
    LEDKey MakeLEDKey(float x, float y, float z) const;

    RGBColor CalculateColorGridInternal(float x, float y, float z, float time, const GridContext3D& grid,
                                       const std::unordered_map<std::string, std::shared_ptr<CapturedFrame>>* frame_cache,
                                       const std::vector<DisplayPlane3D*>* pre_fetched_planes = nullptr);
    void BuildFrameCache(std::unordered_map<std::string, std::shared_ptr<CapturedFrame>>* out_cache);
    void CalculateColorGridBatch(const std::vector<Vector3D>& positions, float time, const GridContext3D& grid,
                                std::vector<RGBColor>& out);
};

#endif // SCREENMIRROR3D_H
