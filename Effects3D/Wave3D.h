/*---------------------------------------------------------*\
| Wave3D.h                                                  |
|                                                           |
|   3D Wave effect with custom UI controls                 |
|                                                           |
|   Date: 2025-09-27                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef WAVE3D_H
#define WAVE3D_H

#include <QWidget>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include "../SpatialEffect3D.h"

class Wave3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Wave3D(QWidget* parent = nullptr);
    ~Wave3D();

    /*---------------------------------------------------------*\
    | Pure virtual implementations                             |
    \*---------------------------------------------------------*/
    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;

private slots:
    void OnWaveParameterChanged();

private:
    /*---------------------------------------------------------*\
    | Wave-specific controls                                   |
    \*---------------------------------------------------------*/
    QComboBox*      wave_type_combo;        // X, Y, Z, Radial, Spherical
    QSlider*        frequency_slider;       // Wave frequency
    QSlider*        amplitude_slider;       // Wave amplitude
    QSlider*        phase_slider;           // Phase offset
    QCheckBox*      standing_wave_check;    // Standing vs traveling wave
    QSlider*        thickness_slider;       // Wave thickness in 3D
    QComboBox*      falloff_combo;          // Distance falloff type

    /*---------------------------------------------------------*\
    | Current wave parameters                                  |
    \*---------------------------------------------------------*/
    int             wave_type;              // 0=X, 1=Y, 2=Z, 3=Radial, 4=Spherical
    unsigned int    frequency;
    float           amplitude;
    float           phase;
    bool            standing_wave;
    float           thickness;
    int             falloff_type;           // 0=Linear, 1=Quadratic, 2=Exponential
};

#endif