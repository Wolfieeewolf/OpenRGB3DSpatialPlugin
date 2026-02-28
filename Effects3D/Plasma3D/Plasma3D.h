// SPDX-License-Identifier: GPL-2.0-only

#ifndef PLASMA3D_H
#define PLASMA3D_H

#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class Plasma3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Plasma3D(QWidget* parent = nullptr);
    ~Plasma3D();

    EFFECT_REGISTERER_3D("Plasma3D", "3D Plasma", "3D Spatial", [](){return new Plasma3D;});

    static std::string const ClassName() { return "Plasma3D"; }
    static std::string const UIName() { return "3D Plasma"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnPlasmaParameterChanged();

private:
    QComboBox*      pattern_combo;

    int             pattern_type;
    float           progress;
};

#endif
