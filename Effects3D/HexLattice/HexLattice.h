// SPDX-License-Identifier: GPL-2.0-only

#ifndef HEXLATTICE_H
#define HEXLATTICE_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class QSlider;
class QLabel;
class QComboBox;

class HexLattice : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit HexLattice(QWidget* parent = nullptr);
    ~HexLattice() override;

    EFFECT_REGISTERER_3D("HexLattice", "Hex Lattice", "Spatial", []() { return new HexLattice; });

    static std::string const ClassName() { return "HexLattice"; }
    static std::string const UIName() { return "Hex Lattice"; }

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
private:
    static RGBColor Hsv01ToBgr(float h, float s, float v);

    QSlider* breathing_amount_slider = nullptr;
    QLabel* breathing_amount_label = nullptr;
    QSlider* pulse_amount_slider = nullptr;
    QLabel* pulse_amount_label = nullptr;
    QComboBox* flow_mode_combo = nullptr;
    QSlider* turbulence_amount_slider = nullptr;
    QLabel* turbulence_amount_label = nullptr;
    float breathing_amount = 1.0f;
    float pulse_amount = 1.0f;
    int flow_mode = 1;
    float turbulence_amount = 1.0f;
};

#endif
