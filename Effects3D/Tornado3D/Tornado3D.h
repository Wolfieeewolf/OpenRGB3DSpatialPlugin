// SPDX-License-Identifier: GPL-2.0-only

#ifndef TORNADO3D_H
#define TORNADO3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class Tornado3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Tornado3D(QWidget* parent = nullptr);
    ~Tornado3D();

    EFFECT_REGISTERER_3D("Tornado3D", "3D Tornado", "3D Spatial", [](){return new Tornado3D;});

    static std::string const ClassName() { return "Tornado3D"; }
    static std::string const UIName() { return "3D Tornado"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnTornadoParameterChanged();

private:
    QSlider* core_radius_slider;
    QLabel* core_radius_label;
    QSlider* height_slider;
    QLabel* height_label;
    unsigned int core_radius;
    unsigned int tornado_height;
};

#endif

