// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPIRAL_H
#define SPIRAL_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

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
    QSlider*        arms_slider;
    QLabel*         arms_label;
    QComboBox*      pattern_combo;
    QSlider*        gap_slider;
    QLabel*         gap_label;

    unsigned int    num_arms;
    int             pattern_type;
    unsigned int    gap_size;
    float           progress;
};

#endif
