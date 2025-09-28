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
#include "../SpatialEffect3D.h"

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

private slots:
    void OnDNAParameterChanged();
    void OnAddColorClicked();
    void OnRemoveColorClicked();
    void OnColorButtonClicked();
    void OnRainbowModeChanged();

private:
    /*---------------------------------------------------------*\
    | DNA-specific controls                                    |
    \*---------------------------------------------------------*/
    QSlider*        radius_slider;          // Helix radius
    QSlider*        speed_slider;           // Effect speed
    QSlider*        brightness_slider;      // Effect brightness
    QSlider*        frequency_slider;       // DNA pitch frequency
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
    | Current DNA parameters                                   |
    \*---------------------------------------------------------*/
    unsigned int    helix_radius;
    unsigned int    frequency;
    bool            rainbow_mode;
    float           progress;

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
