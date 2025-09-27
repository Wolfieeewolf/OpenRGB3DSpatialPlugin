/*---------------------------------------------------------*\
| Plasma3D.h                                                |
|                                                           |
|   3D Plasma effect with custom UI controls               |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef PLASMA3D_H
#define PLASMA3D_H

#include <QWidget>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include "../SpatialEffect3D.h"

class Plasma3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Plasma3D(QWidget* parent = nullptr);
    ~Plasma3D();

    /*---------------------------------------------------------*\
    | Pure virtual implementations                             |
    \*---------------------------------------------------------*/
    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;

private slots:
    void OnPlasmaParameterChanged();

private:
    /*---------------------------------------------------------*\
    | Plasma-specific controls                                 |
    \*---------------------------------------------------------*/
    QSlider*        complexity_slider;      // Plasma complexity/detail
    QSlider*        time_scale_slider;      // Time scaling factor
    QSlider*        noise_scale_slider;     // Noise scale
    QComboBox*      pattern_combo;          // Pattern type
    QCheckBox*      smooth_check;           // Smooth interpolation
    QSlider*        color_shift_slider;     // Color shifting

    /*---------------------------------------------------------*\
    | Current plasma parameters                                |
    \*---------------------------------------------------------*/
    float           complexity;
    float           time_scale;
    float           noise_scale;
    int             pattern_type;           // 0=Classic, 1=Swirl, 2=Ripple, 3=Organic
    bool            smooth_interpolation;
    float           color_shift;
};

#endif