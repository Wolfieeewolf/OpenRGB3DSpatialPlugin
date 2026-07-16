// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPIRAL_H
#define SPIRAL_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

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

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnSpiralParameterChanged();
private:
    static constexpr int kSpiralPatternCount = 6;

    QSlider*   arms_slider = nullptr;
    QComboBox* pattern_combo = nullptr;
    QSlider*   gap_slider = nullptr;

    unsigned int    num_arms;
    int             pattern_type;
    unsigned int    gap_size;
    float           progress;
};

#endif
