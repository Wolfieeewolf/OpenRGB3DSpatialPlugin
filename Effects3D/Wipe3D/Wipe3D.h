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
    void OnAddColorClicked();
    void OnRemoveColorClicked();
    void OnColorButtonClicked();

private:
    /*---------------------------------------------------------*\
    | Wipe-specific controls                                   |
    \*---------------------------------------------------------*/
    QComboBox*      direction_combo;        // Direction presets (X/Y/Z/Radial)
    QSlider*        speed_slider;           // Effect speed
    QSlider*        brightness_slider;      // Effect brightness
    QSlider*        thickness_slider;       // Wipe thickness
    QComboBox*      shape_combo;            // Edge shapes (Round/Point/Square)
    QCheckBox*      reverse_check;          // Reverse direction

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
    | Current wipe parameters                                  |
    \*---------------------------------------------------------*/
    int             direction_type;         // 0=X, 1=Y, 2=Z, 3=Radial
    unsigned int    thickness;              // Wipe thickness
    int             shape_type;             // 0=Round, 1=Point, 2=Square
    bool            reverse_mode;           // Reverse direction
    float           progress;               // Animation progress

    /*---------------------------------------------------------*\
    | Helper methods                                           |
    \*---------------------------------------------------------*/
    void SetupColorControls(QWidget* parent);
    void CreateColorButton(RGBColor color);
    void RemoveLastColorButton();
    RGBColor GetColorAtPosition(float position);
    float CalculateWipeDistance(float x, float y, float z);
    float CalculateWipeDistanceGrid(float x, float y, float z, const GridContext3D& grid);
    float ApplyEdgeShape(float distance, float edge_distance);
};

#endif