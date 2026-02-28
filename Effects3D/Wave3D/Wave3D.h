// SPDX-License-Identifier: GPL-2.0-only

#ifndef WAVE3D_H
#define WAVE3D_H

#include <QWidget>
#include <QComboBox>
#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class Wave3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Wave3D(QWidget* parent = nullptr);
    ~Wave3D();

    EFFECT_REGISTERER_3D("Wave3D", "3D Wave", "3D Spatial", [](){return new Wave3D;});

    static std::string const ClassName() { return "Wave3D"; }
    static std::string const UIName() { return "3D Wave"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnWaveParameterChanged();

private:
    QComboBox* shape_combo;
    int shape_type;
    float progress;
};

#endif
