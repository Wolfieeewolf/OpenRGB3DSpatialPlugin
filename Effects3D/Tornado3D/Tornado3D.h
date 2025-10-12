/*---------------------------------------------------------*\
| Tornado3D.h                                              |
|                                                          |
|   Room-scale vortex/tornado effect                       |
|                                                          |
|   SPDX-License-Identifier: GPL-2.0-only                  |
\*---------------------------------------------------------*/

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

private slots:
    void OnTornadoParameterChanged();

private:
    QSlider* core_radius_slider;
    QSlider* height_slider;
    unsigned int core_radius; // 10..300 grid units * 0.01
    unsigned int tornado_height; // 50..500 grid units * 0.01
};

#endif

