/*---------------------------------------------------------*\
| OpenRGB3DSpatialTab.h                                     |
|                                                           |
|   Main UI tab for 3D spatial control                     |
|                                                           |
|   Date: 2025-09-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef OPENRGB3DSPATIALTAB_H
#define OPENRGB3DSPATIALTAB_H

#include <QWidget>
#include <QPushButton>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QSpinBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>

#include "ResourceManagerInterface.h"
#include "SpatialGrid3D.h"
#include "SpatialEffects.h"
#include "Grid3DWidget.h"

namespace Ui
{
    class OpenRGB3DSpatialTab;
}

class OpenRGB3DSpatialTab : public QWidget
{
    Q_OBJECT

public:
    explicit OpenRGB3DSpatialTab(ResourceManagerInterface* rm, QWidget *parent = nullptr);
    ~OpenRGB3DSpatialTab();

public slots:
    void UpdateDeviceList();

private slots:
    void on_grid_width_changed(int value);
    void on_grid_height_changed(int value);
    void on_grid_depth_changed(int value);

    void on_effect_type_changed(int index);
    void on_effect_speed_changed(int value);
    void on_effect_brightness_changed(int value);
    void on_start_effect_clicked();
    void on_stop_effect_clicked();

    void on_color_start_clicked();
    void on_color_end_clicked();

private:
    void SetupUI();
    void LoadDevices();

    Ui::OpenRGB3DSpatialTab*    ui;
    ResourceManagerInterface*   resource_manager;

    SpatialGrid3D*              grid;
    SpatialEffects*             effects;
    Grid3DWidget*               grid_widget;

    QSpinBox*                   grid_width_spin;
    QSpinBox*                   grid_height_spin;
    QSpinBox*                   grid_depth_spin;

    QComboBox*                  effect_type_combo;
    QSlider*                    effect_speed_slider;
    QSlider*                    effect_brightness_slider;
    QLabel*                     speed_label;
    QLabel*                     brightness_label;

    QPushButton*                start_effect_button;
    QPushButton*                stop_effect_button;
    QPushButton*                color_start_button;
    QPushButton*                color_end_button;

    RGBColor                    current_color_start;
    RGBColor                    current_color_end;
};

#endif