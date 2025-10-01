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
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QColorDialog>
#include "SpatialEffect3D.h"

class Wipe3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Wipe3D(QWidget* parent = nullptr);
    ~Wipe3D();

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
    | Wipe-specific controls only                              |
    \*---------------------------------------------------------*/
    QSlider*        thickness_slider;       // Wipe thickness
    QComboBox*      shape_combo;            // Edge shapes (Round/Point/Square)

    /*---------------------------------------------------------*\
    | Wipe-specific parameters                                 |
    \*---------------------------------------------------------*/
    int             wipe_thickness;         // Wipe thickness
    int             edge_shape;             // 0=Round, 1=Point, 2=Square
    float           progress;               // Animation progress

    /*---------------------------------------------------------*\
    | Helper methods                                           |
    \*---------------------------------------------------------*/
    float smoothstep(float edge0, float edge1, float x);
};

#endif