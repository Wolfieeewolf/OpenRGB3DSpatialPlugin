/*---------------------------------------------------------*\
| ScreenMirror3D.h                                          |
|                                                           |
|   3D Spatial screen mirroring effect with ambilight      |
|                                                           |
|   Date: 2025-10-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef SCREENMIRROR3D_H
#define SCREENMIRROR3D_H

#include <QWidget>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include <map>
#include <memory>
#include <string>

// Forward declarations
class DisplayPlane3D;
class VirtualReferencePoint3D;
struct CaptureSourceInfo;

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

signals:
    void ScreenPreviewChanged(bool enabled);

private slots:
    void OnParameterChanged();
    void OnScreenPreviewChanged();

private:
    void StartCaptureIfNeeded();
    void StopCaptureIfNeeded();

    /*---------------------------------------------------------*\
    | Per-Monitor Settings Structure                           |
    \*---------------------------------------------------------*/
    struct MonitorSettings
    {
        bool enabled;
        float scale;           // 0-2.0 (0-200%), how much this monitor affects LEDs
        float edge_softness;   // 0-100%, feathering percentage
        float blend;           // 0-100%, blending with other monitors
        float edge_zone_depth; // 0.01-0.50, edge sampling depth
        int reference_point_index; // Index into reference_points vector (-1 = use global)

        // UI widgets for this monitor
        QGroupBox* group_box;
        QCheckBox* enabled_check;
        QSlider* scale_slider;
        QSlider* softness_slider;
        QSlider* blend_slider;
        QSlider* edge_zone_slider;
        QComboBox* ref_point_combo;

        MonitorSettings()
            : enabled(true)
            , scale(1.0f)
            , edge_softness(30.0f)
            , blend(50.0f)
            , edge_zone_depth(0.01f)
            , reference_point_index(-1)
            , group_box(nullptr)
            , enabled_check(nullptr)
            , scale_slider(nullptr)
            , softness_slider(nullptr)
            , blend_slider(nullptr)
            , edge_zone_slider(nullptr)
            , ref_point_combo(nullptr)
        {}
    };

    /*---------------------------------------------------------*\
    | UI Controls                                              |
    \*---------------------------------------------------------*/
    QSlider*            global_scale_slider;
    QSlider*            smoothing_time_slider;
    QSlider*            brightness_slider;
    QCheckBox*          test_pattern_check;
    QCheckBox*          screen_preview_check;
    std::map<std::string, MonitorSettings> monitor_settings;  // Monitor name -> settings

    /*---------------------------------------------------------*\
    | Effect parameters                                        |
    \*---------------------------------------------------------*/
    float                       global_scale;           // Master scale multiplier (0-2.0)
    float                       smoothing_time_ms;
    float                       brightness_multiplier;
    bool                        show_test_pattern;
    // Per-monitor settings stored in monitor_settings map

    // Reference points (pointer to main tab's vector)
    std::vector<std::unique_ptr<VirtualReferencePoint3D>>* reference_points;

    // Temporal smoothing per LED (simple exponential moving average)
    struct LEDState
    {
        float r, g, b;
        uint64_t last_update_ms;
    };
    std::map<void*, LEDState> led_states;  // Key is LED pointer
};

#endif // SCREENMIRROR3D_H
