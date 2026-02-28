// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPIRAL3D_H
#define SPIRAL3D_H

#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class Spiral3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Spiral3D(QWidget* parent = nullptr);
    ~Spiral3D();

    EFFECT_REGISTERER_3D("Spiral3D", "3D Spiral", "3D Spatial", [](){return new Spiral3D;});

    static std::string const ClassName() { return "Spiral3D"; }
    static std::string const UIName() { return "3D Spiral"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
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
