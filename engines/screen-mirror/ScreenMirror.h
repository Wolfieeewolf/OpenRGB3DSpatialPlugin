// SPDX-License-Identifier: GPL-2.0-only

#ifndef SCREENMIRROR_H
#define SCREENMIRROR_H

#include "SpatialEffect3D.h"
#include <map>
#include <unordered_map>
#include <deque>
#include <memory>
#include <string>
#include <vector>
#include "ScreenCaptureManager.h"
#include "ui/screen/CaptureZonesWidget.h"

class DisplayPlane3D;
class QDoubleSpinBox;
class QStackedWidget;
struct CapturedFrame;

class ScreenMirrorMonitorPanel;

class ScreenMirror : public SpatialEffect3D
{
    Q_OBJECT

    friend class ScreenMirrorMonitorPanel;

public:
    explicit ScreenMirror(QWidget* parent = nullptr);
    ~ScreenMirror();

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool UsesSpatialSamplingQuantization() const override { return false; }
    bool RequiresWorldSpaceCoordinates() const override { return true; }
    bool RequiresWorldSpaceGridBounds() const override { return true; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

    void RefreshMonitorStatus();

    using CaptureZone = ::CaptureZone;

signals:
    void ScreenPreviewChanged(bool enabled);
    void CalibrationPatternChanged(bool enabled);

public:
    bool ShouldShowCalibrationPattern(const std::string& plane_name) const;
    bool ShouldShowScreenPreview(const std::string& plane_name) const;

    struct MonitorSettings
    {
        bool enabled;

        float scale;
        bool scale_inverted;

        float smoothing_time_ms;
        float brightness_multiplier;
        float brightness_threshold;
        float white_rolloff;
        float vibrance;
        float led_output_gain_r;
        float led_output_gain_g;
        float led_output_gain_b;
        float black_bar_top_percent;
        float black_bar_bottom_percent;
        float black_bar_left_percent;
        float black_bar_right_percent;
        bool black_bar_link_vertical;
        bool black_bar_link_horizontal;

        float edge_softness;
        float propagation_speed_mm_per_ms;
        float wave_decay_ms;
        float wave_time_to_edge_sec;
        float falloff_curve_exponent;

        bool show_calibration_pattern;
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
        QSlider* white_rolloff_slider;
        QLabel* white_rolloff_label;
        QSlider* vibrance_slider;
        QLabel* vibrance_label;
        QSlider* led_output_gain_r_slider;
        QLabel* led_output_gain_r_label;
        QSlider* led_output_gain_g_slider;
        QLabel* led_output_gain_g_label;
        QSlider* led_output_gain_b_slider;
        QLabel* led_output_gain_b_label;
        QSlider* black_bar_top_slider;
        QLabel* black_bar_top_label;
        QSlider* black_bar_bottom_slider;
        QLabel* black_bar_bottom_label;
        QSlider* black_bar_left_slider;
        QLabel* black_bar_left_label;
        QSlider* black_bar_right_slider;
        QLabel* black_bar_right_label;
        QCheckBox* black_bar_link_vertical_check;
        QCheckBox* black_bar_link_horizontal_check;
        QSlider* softness_slider;
        QLabel* softness_label;
        QSlider* propagation_speed_slider;
        QLabel* propagation_speed_label;
        QSlider* wave_decay_slider;
        QLabel* wave_decay_label;
        QSlider* wave_time_to_edge_slider;
        QDoubleSpinBox* wave_time_to_edge_spin;
        QSlider* falloff_curve_slider;
        QLabel* falloff_curve_label;
        QComboBox* capture_combo;
        QPushButton* capture_refresh_button;
        QCheckBox* calibration_pattern_check;
        QCheckBox* screen_preview_check;
        QWidget* capture_area_preview;
        QPushButton* add_zone_button;
        QPushButton* remove_zone_button;
        CaptureZonesWidget* capture_zones_widget;

        MonitorSettings()
            : enabled(false)
            , scale(1.0f)
            , scale_inverted(true)
            , smoothing_time_ms(0.0f)
            , brightness_multiplier(1.0f)
            , brightness_threshold(0.0f)
            , white_rolloff(0.0f)
            , vibrance(1.0f)
            , led_output_gain_r(1.0f)
            , led_output_gain_g(1.0f)
            , led_output_gain_b(1.0f)
            , black_bar_top_percent(0.0f)
            , black_bar_bottom_percent(0.0f)
            , black_bar_left_percent(0.0f)
            , black_bar_right_percent(0.0f)
            , black_bar_link_vertical(true)
            , black_bar_link_horizontal(true)
            , edge_softness(70.0f)
            , propagation_speed_mm_per_ms(0.0f)
            , wave_decay_ms(0.0f)
            , wave_time_to_edge_sec(0.0f)
            , falloff_curve_exponent(1.0f)
            , show_calibration_pattern(false)
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
            , white_rolloff_slider(nullptr)
            , white_rolloff_label(nullptr)
            , vibrance_slider(nullptr)
            , vibrance_label(nullptr)
            , led_output_gain_r_slider(nullptr)
            , led_output_gain_r_label(nullptr)
            , led_output_gain_g_slider(nullptr)
            , led_output_gain_g_label(nullptr)
            , led_output_gain_b_slider(nullptr)
            , led_output_gain_b_label(nullptr)
            , black_bar_top_slider(nullptr)
            , black_bar_top_label(nullptr)
            , black_bar_bottom_slider(nullptr)
            , black_bar_bottom_label(nullptr)
            , black_bar_left_slider(nullptr)
            , black_bar_left_label(nullptr)
            , black_bar_right_slider(nullptr)
            , black_bar_right_label(nullptr)
            , black_bar_link_vertical_check(nullptr)
            , black_bar_link_horizontal_check(nullptr)
            , softness_slider(nullptr)
            , softness_label(nullptr)
            , propagation_speed_slider(nullptr)
            , propagation_speed_label(nullptr)
            , wave_decay_slider(nullptr)
            , wave_decay_label(nullptr)
            , wave_time_to_edge_slider(nullptr)
            , wave_time_to_edge_spin(nullptr)
            , falloff_curve_slider(nullptr)
            , falloff_curve_label(nullptr)
            , capture_combo(nullptr)
            , capture_refresh_button(nullptr)
            , calibration_pattern_check(nullptr)
            , screen_preview_check(nullptr)
            , capture_area_preview(nullptr)
            , add_zone_button(nullptr)
            , remove_zone_button(nullptr)
            , capture_zones_widget(nullptr)
        {
            capture_zones.push_back(CaptureZone(0.0f, 1.0f, 0.0f, 1.0f));
        }
    };

private slots:
    void OnParameterChanged();
    void OnScreenPreviewChanged();
    void OnCalibrationPatternChanged();

private:
    void RefreshFrameCacheForRenderSequence(const GridContext3D& grid);
    void CreateMonitorSettingsUI(DisplayPlane3D* plane, MonitorSettings& settings);
    void EnsureMonitorPicker();
    void SyncMonitorPickerLabels();
    void ShowMonitorSettingsPage(const std::string& plane_name);
    void SyncMonitorSettingsToUI(MonitorSettings& msettings);
    void SelectCaptureQualityCombo(int quality);

    int                 capture_quality;
    QComboBox*          capture_quality_combo;
    int                 capture_backend_mode;
    QComboBox*          capture_backend_combo;

    QLabel*             monitor_status_label;
    QLabel*             monitor_help_label;
    QGroupBox*          monitors_container;
    QVBoxLayout*        monitors_layout;
    QComboBox*          monitor_select_combo;
    QStackedWidget*     monitor_stack;
    std::map<std::string, MonitorSettings> monitor_settings;

    bool                        in_parameter_change_;

    struct FrameHistory
    {
        std::deque<std::shared_ptr<CapturedFrame>> frames;
    };
    std::unordered_map<std::string, FrameHistory> capture_history;

    uint64_t                                                             frame_cache_refresh_ms_;
    uint64_t                                                             frame_cache_last_render_seq_;
    std::unordered_map<std::string, std::shared_ptr<CapturedFrame>>      frame_cache_;
    std::vector<DisplayPlane3D*>                                         frame_cache_planes_;

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
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        uint64_t smooth_last_tick_ms = 0;
    };
    std::map<LEDKey, LEDState> led_states;

    void AddFrameToHistory(const std::string& capture_id, const std::shared_ptr<CapturedFrame>& frame);
    float GetHistoryRetentionMs() const;
    LEDKey MakeLEDKey(float x, float y, float z) const;

    struct SampleMonitor
    {
        DisplayPlane3D* plane;
        MonitorSettings* settings;
        Vector3D falloff_origin;
        bool calibration;
        float wave_speed;
        float max_distance_mm;
        std::shared_ptr<CapturedFrame> latest;
        const uint8_t* cal_rgba;
        int cal_w;
        int cal_h;
    };
    std::vector<SampleMonitor> sample_tick_;
    float tick_scale_mm_;
    float smooth_ms_;
};

#endif
