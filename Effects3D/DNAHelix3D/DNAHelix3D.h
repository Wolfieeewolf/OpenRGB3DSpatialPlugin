/*---------------------------------------------------------*\
| DNAHelix3D.h                                              |
|                                                           |
|   3D DNA Helix effect with enhanced controls             |
|                                                           |
|   Date: 2025-09-28                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef DNAHELIX3D_H
#define DNAHELIX3D_H

#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QColorDialog>
#include "SpatialEffect3D.h"

class DNAHelix3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit DNAHelix3D(QWidget* parent = nullptr);
    ~DNAHelix3D();

    /*---------------------------------------------------------*\
    | Pure virtual implementations                             |
    \*---------------------------------------------------------*/
    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

private slots:
    void OnDNAParameterChanged();

private:
    /*---------------------------------------------------------*\
    | DNA-specific controls only                               |
    \*---------------------------------------------------------*/
    QSlider*        radius_slider;          // Helix radius

    /*---------------------------------------------------------*\
    | Current DNA parameters                                   |
    \*---------------------------------------------------------*/
    unsigned int    helix_radius;
    float           progress;
};

#endif
