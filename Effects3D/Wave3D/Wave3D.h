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
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QColorDialog>
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
    RGBColor CalculateColor(float x, float y, float z, float time) override;

private slots:
    void OnWaveParameterChanged();
    void OnAddColorClicked();
    void OnRemoveColorClicked();
    void OnColorButtonClicked();
    void OnRainbowModeChanged();

private:
    /*---------------------------------------------------------*\
    | Wave-specific controls                                   |
    \*---------------------------------------------------------*/
    QComboBox*      direction_combo;        // Direction presets (X/Y/Z/Radial)
    QSlider*        speed_slider;           // Effect speed
    QSlider*        brightness_slider;      // Effect brightness
    QSlider*        frequency_slider;       // Wave frequency
    QComboBox*      shape_combo;            // Circles/Squares
    QCheckBox*      reverse_check;          // Reverse direction
    QCheckBox*      rainbow_mode_check;     // Rainbow mode toggle

    /*---------------------------------------------------------*\
    | Color controls (like RadialRainbow)                     |
    \*---------------------------------------------------------*/
    QWidget*        color_controls_widget;
    QHBoxLayout*    color_controls_layout;
    QPushButton*    add_color_button;
    QPushButton*    remove_color_button;
    std::vector<QPushButton*> color_buttons;
    std::vector<RGBColor> colors;

    /*---------------------------------------------------------*\
    | Current wave parameters                                  |
    \*---------------------------------------------------------*/
    int             direction_type;         // 0=X, 1=Y, 2=Z, 3=Radial
    unsigned int    frequency;
    int             shape_type;             // 0=Circles, 1=Squares
    bool            reverse_mode;           // Reverse direction
    bool            rainbow_mode;           // Rainbow mode enabled
    float           progress;               // Animation progress

    /*---------------------------------------------------------*\
    | Helper methods                                           |
    \*---------------------------------------------------------*/
    void SetupColorControls(QWidget* parent);
    void CreateColorButton(RGBColor color);
    void RemoveLastColorButton();
    RGBColor GetRainbowColor(float hue);
    RGBColor GetColorAtPosition(float position);
};

#endif