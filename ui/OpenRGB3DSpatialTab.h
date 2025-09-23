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
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QListWidget>

#include "ResourceManagerInterface.h"
#include "SpatialEffects.h"
#include "LEDPosition3D.h"
#include "LEDViewport3D.h"

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
    void on_effect_type_changed(int index);
    void on_effect_speed_changed(int value);
    void on_effect_brightness_changed(int value);
    void on_start_effect_clicked();
    void on_stop_effect_clicked();

    void on_color_start_clicked();
    void on_color_end_clicked();

    void on_controller_selected(int index);
    void on_controller_position_changed(int index, float x, float y, float z);

    void on_effect_updated();

private:
    void SetupUI();
    void LoadDevices();

    Ui::OpenRGB3DSpatialTab*    ui;
    ResourceManagerInterface*   resource_manager;

    SpatialEffects*             effects;
    LEDViewport3D*              viewport;

    std::vector<ControllerTransform*> controller_transforms;

    QListWidget*                controller_list;
    QDoubleSpinBox*             pos_x_spin;
    QDoubleSpinBox*             pos_y_spin;
    QDoubleSpinBox*             pos_z_spin;
    QDoubleSpinBox*             rot_x_spin;
    QDoubleSpinBox*             rot_y_spin;
    QDoubleSpinBox*             rot_z_spin;

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