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
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QColorDialog>
#include "SpatialEffect3D.h"

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
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

private slots:
    void OnPlasmaParameterChanged();
    void OnAddColorClicked();
    void OnRemoveColorClicked();
    void OnColorButtonClicked();
    void OnRainbowModeChanged();

private:
    /*---------------------------------------------------------*\
    | Plasma-specific controls                                 |
    \*---------------------------------------------------------*/
    QComboBox*      pattern_combo;          // Pattern type
    QSlider*        speed_slider;           // Effect speed
    QSlider*        brightness_slider;      // Effect brightness
    QSlider*        frequency_slider;       // Plasma frequency
    QCheckBox*      rainbow_mode_check;     // Rainbow mode toggle

    /*---------------------------------------------------------*\
    | Color controls                                           |
    \*---------------------------------------------------------*/
    QWidget*        color_controls_widget;
    QHBoxLayout*    color_controls_layout;
    QPushButton*    add_color_button;
    QPushButton*    remove_color_button;
    std::vector<QPushButton*> color_buttons;
    std::vector<RGBColor> colors;

    /*---------------------------------------------------------*\
    | Current plasma parameters                                |
    \*---------------------------------------------------------*/
    int             pattern_type;           // 0=Classic, 1=Swirl, 2=Ripple, 3=Organic
    unsigned int    frequency;              // Plasma frequency
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