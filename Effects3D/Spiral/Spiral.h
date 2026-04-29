// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPIRAL_H
#define SPIRAL_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

#include <array>

class QSlider;
class QLabel;
class QWidget;
class QComboBox;

class Spiral : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Spiral(QWidget* parent = nullptr);
    ~Spiral();

    EFFECT_REGISTERER_3D("Spiral", "Spiral", "Spatial", [](){return new Spiral;});

    static std::string const ClassName() { return "Spiral"; }
    static std::string const UIName() { return "Spiral"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnSpiralParameterChanged();

private:
    static constexpr int kSpiralPatternCount = 6;

    void SyncLayeredBandWidgets();

    QSlider*        arms_slider;
    QLabel*         arms_label;
    QComboBox*      pattern_combo;
    QSlider*        gap_slider;
    QLabel*         gap_label;
    QComboBox*      spiral_layout_combo = nullptr;
    QWidget*        layered_band_widget = nullptr;
    QSlider*        band_speed_slider[3]{};
    QSlider*        band_tight_slider[3]{};
    QSlider*        band_phase_slider[3]{};
    QLabel*         band_speed_lbl[3]{};
    QLabel*         band_tight_lbl[3]{};
    QLabel*         band_phase_lbl[3]{};

    unsigned int    num_arms;
    int             pattern_type;
    unsigned int    gap_size;
    float           progress;

    /** 0 = single field; 1 = floor / mid / ceiling blend their own speed, tightness, phase. */
    int             spiral_layout_mode = 0;
    std::array<int, 3> band_speed_pct{{100, 100, 100}};
    std::array<int, 3> band_tight_pct{{100, 100, 100}};
    std::array<int, 3> band_phase_deg{{0, 0, 0}};
};

#endif
