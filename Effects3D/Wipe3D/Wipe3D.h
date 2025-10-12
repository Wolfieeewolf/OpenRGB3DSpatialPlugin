/*---------------------------------------------------------*\
| Wipe3D.h                                                  |
|                                                           |
|   3D Wipe effect with directional transitions           |
|                                                           |
|   Date: 2025-09-28                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef WIPE3D_H
#define WIPE3D_H

#include <QWidget>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class Wipe3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Wipe3D(QWidget* parent = nullptr);
    ~Wipe3D();

    /*---------------------------------------------------------*\
    | Auto-registration system                                 |
    \*---------------------------------------------------------*/
    EFFECT_REGISTERER_3D("Wipe3D", "3D Wipe", "3D Spatial", [](){return new Wipe3D;});

    static std::string const ClassName() { return "Wipe3D"; }
    static std::string const UIName() { return "3D Wipe"; }

    /*---------------------------------------------------------*\
    | Pure virtual implementations                             |
    \*---------------------------------------------------------*/
    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

private slots:
    void OnWipeParameterChanged();

private:
    /*---------------------------------------------------------*\
    | Wipe-specific controls                                   |
    \*---------------------------------------------------------*/
    QSlider*        thickness_slider;
    QComboBox*      shape_combo;

    /*---------------------------------------------------------*\
    | Wipe-specific parameters                                 |
    \*---------------------------------------------------------*/
    int             wipe_thickness;
    int             edge_shape;
    float           progress;

    /*---------------------------------------------------------*\
    | Helper methods                                           |
    \*---------------------------------------------------------*/
    float smoothstep(float edge0, float edge1, float x);
};

#endif