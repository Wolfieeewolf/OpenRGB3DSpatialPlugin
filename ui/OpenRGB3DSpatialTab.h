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
#include <QScrollArea>

#include "ResourceManagerInterface.h"
#include "LEDPosition3D.h"
#include "LEDViewport3D.h"
#include "VirtualController3D.h"
#include "SpatialEffectTypes.h"
#include "SpatialEffect3D.h"
#include "EffectListManager3D.h"
#include "Effects3D/Wave3D/Wave3D.h"
#include "Effects3D/Wipe3D/Wipe3D.h"
#include "Effects3D/Plasma3D/Plasma3D.h"
#include "Effects3D/Spiral3D/Spiral3D.h"
#include "Effects3D/DNAHelix3D/DNAHelix3D.h"
#include "Effects3D/BreathingSphere3D/BreathingSphere3D.h"
#include "Effects3D/Explosion3D/Explosion3D.h"

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
    void on_start_effect_clicked();
    void on_stop_effect_clicked();

    void on_controller_selected(int index);
    void on_controller_position_changed(int index, float x, float y, float z);
    void on_controller_rotation_changed(int index, float x, float y, float z);

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
    void on_effect_timer_timeout();
    void on_grid_dimensions_changed();
    void on_grid_snap_toggled(bool enabled);
    void on_user_position_changed();
    void on_user_visibility_toggled(bool visible);
    void on_user_center_clicked();
    void on_effect_changed(int index);
    void UpdateSelectionInfo();

private:
    void SetupUI();
    void LoadDevices();
    void SaveLayout(const std::string& filename);
    void LoadLayout(const std::string& filename);
    void LoadLayoutFromJSON(const nlohmann::json& layout_json);
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
    void SetupCustomEffectUI_NEW(const std::string& effect_class_name);  // NEW: Simplified version
    void SetupEffectSignals(SpatialEffect3D* effect, int effect_type);
    void ClearCustomEffectUI();

    Ui::OpenRGB3DSpatialTab*    ui;
    ResourceManagerInterface*   resource_manager;

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

    QComboBox*                  effect_type_combo;
    QPushButton*                start_effect_button;
    QPushButton*                stop_effect_button;

    QWidget*                    custom_effect_container;
    SpatialEffect3D*            current_effect_ui;
    Wave3D*                     wave3d_effect;
    Wipe3D*                     wipe3d_effect;
    Plasma3D*                   plasma3d_effect;
    Spiral3D*                   spiral3d_effect;
    Explosion3D*                explosion3d_effect;
    BreathingSphere3D*          breathingsphere3d_effect;
    DNAHelix3D*                 dnahelix3d_effect;

    QComboBox*                  layout_profiles_combo;
    QCheckBox*                  auto_load_checkbox;
    QTimer*                     auto_load_timer;
    QTimer*                     effect_timer;
    bool                        first_load;
    bool                        effect_running;
    float                       effect_time;

    /*---------------------------------------------------------*\
    | Custom grid dimensions (always 1:1 LED mapping)         |
    \*---------------------------------------------------------*/
    QSpinBox*                   grid_x_spin;
    QSpinBox*                   grid_y_spin;
    QSpinBox*                   grid_z_spin;
    QCheckBox*                  grid_snap_checkbox;
    QLabel*                     selection_info_label;
    int                         custom_grid_x;
    int                         custom_grid_y;
    int                         custom_grid_z;

    /*---------------------------------------------------------*\
    | User Position Reference Point                            |
    \*---------------------------------------------------------*/
    QDoubleSpinBox*             user_pos_x_spin;
    QDoubleSpinBox*             user_pos_y_spin;
    QDoubleSpinBox*             user_pos_z_spin;
    QCheckBox*                  user_visible_checkbox;
    QPushButton*                user_center_button;
    UserPosition3D              user_position;

    /*---------------------------------------------------------*\
    | Effects Section Controls                                 |
    \*---------------------------------------------------------*/
    QComboBox*                  effect_combo;
    QWidget*                    effect_controls_widget;
    QVBoxLayout*                effect_controls_layout;

    std::vector<VirtualController3D*> virtual_controllers;
};

#endif