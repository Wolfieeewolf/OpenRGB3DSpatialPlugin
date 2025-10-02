/*---------------------------------------------------------*\
| BreathingSphere3D.h                                       |
|                                                           |
|   3D Breathing Sphere effect - pulsing sphere from origin |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef BREATHINGSPHERE3D_H
#define BREATHINGSPHERE3D_H

#include <QWidget>
#include <QSlider>
#include <QLabel>
#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class BreathingSphere3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit BreathingSphere3D(QWidget* parent = nullptr);
    ~BreathingSphere3D();

    /*---------------------------------------------------------*\
    | Auto-registration system                                 |
    \*---------------------------------------------------------*/
    EFFECT_REGISTERER_3D("BreathingSphere3D", "3D Breathing Sphere", "3D Spatial", [](){return new BreathingSphere3D;});

    static std::string const ClassName() { return "BreathingSphere3D"; }
    static std::string const UIName() { return "3D Breathing Sphere"; }

    /*---------------------------------------------------------*\
    | Pure virtual implementations                             |
    \*---------------------------------------------------------*/
    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;

private slots:
    void OnBreathingParameterChanged();

private:
    /*---------------------------------------------------------*\
    | Breathing-specific controls                              |
    \*---------------------------------------------------------*/
    QSlider*        size_slider;

    /*---------------------------------------------------------*\
    | Breathing-specific parameters                            |
    | (frequency, rainbow_mode, colors are in base class)     |
    \*---------------------------------------------------------*/
    unsigned int    sphere_size;
    float           progress;
};

#endif
