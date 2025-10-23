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
#include <QPushButton>
#include <QScrollArea>
#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class Wave3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Wave3D(QWidget* parent = nullptr);
    ~Wave3D();

    /*---------------------------------------------------------*\
    | Auto-registration system                                 |
    \*---------------------------------------------------------*/
    EFFECT_REGISTERER_3D("Wave3D", "3D Wave", "3D Spatial", [](){return new Wave3D;});

    static std::string const ClassName() { return "Wave3D"; }
    static std::string const UIName() { return "3D Wave"; }

    /*---------------------------------------------------------*\
    | Pure virtual implementations                             |
    \*---------------------------------------------------------*/
    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    /*---------------------------------------------------------*\
    | Settings persistence                                     |
    \*---------------------------------------------------------*/
    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnWaveParameterChanged();

private:
    /*---------------------------------------------------------*\
    | Wave-specific controls                                   |
    \*---------------------------------------------------------*/
    QComboBox*      shape_combo;            // Circles/Squares

    /*---------------------------------------------------------*\
    | Wave-specific parameters only                            |
    | (frequency, rainbow_mode, colors, axis are in base class)|
    \*---------------------------------------------------------*/
    int             shape_type;             // 0=Circles, 1=Squares
    float           progress;               // Animation progress
};

#endif
