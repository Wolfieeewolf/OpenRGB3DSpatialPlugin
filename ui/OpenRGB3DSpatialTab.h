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
#include <QCheckBox>
#include <QTimer>

#include "ResourceManagerInterface.h"
#include "SpatialEffects.h"
#include "LEDPosition3D.h"
#include "LEDViewport3D.h"
#include "VirtualController3D.h"
#include "SpatialEffect3D.h"
#include "Wave3D.h"

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
    void on_effect_speed_changed(int value);
    void on_effect_brightness_changed(int value);
    void on_effect_type_changed(int index);
    void on_start_effect_clicked();
    void on_stop_effect_clicked();

    void on_color_start_clicked();
    void on_color_end_clicked();

    void on_controller_selected(int index);
    void on_controller_position_changed(int index, float x, float y, float z);
    void on_controller_scale_changed(int index, float x, float y, float z);

    void on_add_clicked();
    void on_granularity_changed(int index);
    void on_remove_controller_clicked();
    void on_clear_all_clicked();

    void on_save_layout_clicked();
    void on_load_layout_clicked();
    void on_delete_layout_clicked();
    void on_layout_profile_changed(int index);
    void on_create_custom_controller_clicked();
    void on_import_custom_controller_clicked();
    void on_export_custom_controller_clicked();
    void on_edit_custom_controller_clicked();

    void on_effect_updated();

private:
    void SetupUI();
    void LoadDevices();
    void SaveLayout(const std::string& filename);
    void LoadLayout(const std::string& filename);
    std::string GetLayoutPath(const std::string& layout_name);
    void TryAutoLoadLayout();
    void PopulateLayoutDropdown();
    void SaveCurrentLayoutName();
    void SaveCustomControllers();
    void LoadCustomControllers();
    void UpdateAvailableItemCombo();
    void UpdateAvailableControllersList();
    bool IsItemInScene(RGBController* controller, int granularity, int item_idx);
    int GetUnassignedZoneCount(RGBController* controller);
    int GetUnassignedLEDCount(RGBController* controller);
    void SetupCustomEffectUI(int effect_type);
    void ClearCustomEffectUI();

    Ui::OpenRGB3DSpatialTab*    ui;
    ResourceManagerInterface*   resource_manager;

    SpatialEffects*             effects;
    LEDViewport3D*              viewport;

    std::vector<ControllerTransform*> controller_transforms;

    QListWidget*                available_controllers_list;
    QComboBox*                  granularity_combo;
    QComboBox*                  item_combo;
    QListWidget*                controller_list;
    QDoubleSpinBox*             pos_x_spin;
    QDoubleSpinBox*             pos_y_spin;
    QDoubleSpinBox*             pos_z_spin;
    QSlider*                    pos_x_slider;
    QSlider*                    pos_y_slider;
    QSlider*                    pos_z_slider;
    QDoubleSpinBox*             rot_x_spin;
    QDoubleSpinBox*             rot_y_spin;
    QDoubleSpinBox*             rot_z_spin;
    QSlider*                    rot_x_slider;
    QSlider*                    rot_y_slider;
    QSlider*                    rot_z_slider;
    QDoubleSpinBox*             scale_x_spin;
    QDoubleSpinBox*             scale_y_spin;
    QDoubleSpinBox*             scale_z_spin;
    QSlider*                    scale_x_slider;
    QSlider*                    scale_y_slider;
    QSlider*                    scale_z_slider;

    QComboBox*                  effect_type_combo;
    QSlider*                    effect_speed_slider;
    QSlider*                    effect_brightness_slider;
    QLabel*                     speed_label;
    QLabel*                     brightness_label;

    QPushButton*                start_effect_button;
    QPushButton*                stop_effect_button;
    QPushButton*                color_start_button;
    QPushButton*                color_end_button;

    QWidget*                    custom_effect_container;
    SpatialEffect3D*            current_effect_ui;
    Wave3D*                     wave3d_effect;

    QComboBox*                  layout_profiles_combo;
    QCheckBox*                  auto_load_checkbox;
    QTimer*                     auto_load_timer;
    bool                        first_load;

    std::vector<VirtualController3D*> virtual_controllers;

    RGBColor                    current_color_start;
    RGBColor                    current_color_end;
};

#endif