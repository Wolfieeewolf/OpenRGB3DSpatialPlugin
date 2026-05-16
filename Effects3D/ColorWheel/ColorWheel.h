// SPDX-License-Identifier: GPL-2.0-only

#ifndef COLORWHEEL_H
#define COLORWHEEL_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

#include <array>

class QSlider;
class QWidget;
class QComboBox;
class QLabel;
class ColorWheel : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit ColorWheel(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("ColorWheel", "Color Wheel", "Spatial", [](){ return new ColorWheel; })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
private:
    int direction = 0;
    int hue_geometry_mode = 0;
    int wheel_layout_mode = 0;
    std::array<int, 3> band_speed_pct{{100, 100, 100}};
    std::array<int, 3> band_size_pct{{100, 100, 100}};
    std::array<int, 3> band_phase_deg{{0, 0, 0}};

    QComboBox* wheel_layout_combo = nullptr;
    QWidget* layered_settings_widget = nullptr;
    QSlider* band_speed_slider[3]{};
    QSlider* band_size_slider[3]{};
    QSlider* band_phase_slider[3]{};
    QLabel* band_speed_value_lbl[3]{};
    QLabel* band_size_value_lbl[3]{};
    QLabel* band_phase_value_lbl[3]{};
    void SyncLayeredSliderWidgets();
};

#endif
