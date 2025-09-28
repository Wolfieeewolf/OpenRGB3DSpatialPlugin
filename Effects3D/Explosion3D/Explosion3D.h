/*---------------------------------------------------------*\
| Explosion3D.h                                             |
|                                                           |
|   3D Explosion effect with enhanced controls             |
|                                                           |
|   Date: 2025-09-28                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef Explosion3D_H
#define Explosion3D_H

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

class Explosion3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Explosion3D(QWidget* parent = nullptr);
    ~Explosion3D();

    /*---------------------------------------------------------*\
    | Pure virtual implementations                             |
    \*---------------------------------------------------------*/
    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;

private slots:
    void OnExplosionParameterChanged();
    void OnAddColorClicked();
    void OnRemoveColorClicked();
    void OnColorButtonClicked();
    void OnRainbowModeChanged();

private:
    /*---------------------------------------------------------*\
    | Explosion-specific controls                              |
    \*---------------------------------------------------------*/
    QSlider*        intensity_slider;       // Explosion intensity
    QSlider*        speed_slider;           // Effect speed
    QSlider*        brightness_slider;      // Effect brightness
    QSlider*        frequency_slider;       // Shockwave frequency
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
    | Current explosion parameters                             |
    \*---------------------------------------------------------*/
    unsigned int    explosion_intensity;
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