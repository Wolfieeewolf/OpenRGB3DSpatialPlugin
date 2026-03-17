// SPDX-License-Identifier: GPL-2.0-only

#ifndef PLASMA_H
#define PLASMA_H

#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class Plasma : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Plasma(QWidget* parent = nullptr);
    ~Plasma();

    EFFECT_REGISTERER_3D("Plasma", "Plasma", "Spatial", [](){return new Plasma;});

    static std::string const ClassName() { return "Plasma"; }
    static std::string const UIName() { return "Plasma"; }

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
