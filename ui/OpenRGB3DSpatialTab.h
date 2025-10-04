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
#include <QTabWidget>
#include <QLineEdit>

#include "ResourceManagerInterface.h"
#include "LEDPosition3D.h"
#include "LEDViewport3D.h"
#include "VirtualController3D.h"
#include "VirtualReferencePoint3D.h"
#include "SpatialEffectTypes.h"
#include "SpatialEffect3D.h"
#include "EffectListManager3D.h"
#include "Effects3D/Wave3D/Wave3D.h"
#include "Effects3D/Wipe3D/Wipe3D.h"
#include "Effects3D/Plasma3D/Plasma3D.h"
#include "Effects3D/Spiral3D/Spiral3D.h"
#include "Effects3D/Spin3D/Spin3D.h"
#include "Effects3D/DNAHelix3D/DNAHelix3D.h"
#include "Effects3D/BreathingSphere3D/BreathingSphere3D.h"
#include "Effects3D/Explosion3D/Explosion3D.h"
#include "ZoneManager3D.h"

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
    void on_led_spacing_preset_changed(int index);
    void on_remove_controller_clicked();
    void on_remove_controller_from_viewport(int index);
    void on_clear_all_clicked();

    void on_save_layout_clicked();
    void on_load_layout_clicked();
    void on_delete_layout_clicked();
    void on_layout_profile_changed(int index);
    void on_create_custom_controller_clicked();
    void on_import_custom_controller_clicked();
    void on_export_custom_controller_clicked();
    void on_edit_custom_controller_clicked();
    void on_add_ref_point_clicked();
    void on_remove_ref_point_clicked();
    void on_ref_point_selected(int index);
    void on_ref_point_position_changed(int index, float x, float y, float z);
    void on_ref_point_color_clicked();

    void on_create_zone_clicked();
    void on_edit_zone_clicked();
    void on_delete_zone_clicked();
    void on_zone_selected(int index);

    void on_effect_updated();
    void on_effect_timer_timeout();
    void on_grid_dimensions_changed();
    void on_grid_snap_toggled(bool enabled);
    void on_effect_changed(int index);
    void on_effect_origin_changed(int index);
    void UpdateSelectionInfo();
    void on_apply_spacing_clicked();
    void UpdateEffectOriginCombo();

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
    void UpdateCustomControllersList();
    void UpdateReferencePointsList();
    void SaveReferencePoints();
    void LoadReferencePoints();
    void UpdateZonesList();
    void UpdateEffectZoneCombo();
    void SaveZones();
    void LoadZones();
    bool IsItemInScene(RGBController* controller, int granularity, int item_idx);
    int GetUnassignedZoneCount(RGBController* controller);
    int GetUnassignedLEDCount(RGBController* controller);
    void SetupCustomEffectUI(int effect_type);
    void SetupCustomEffectUI_NEW(const std::string& effect_class_name);  // NEW: Simplified version
    void SetupEffectSignals(SpatialEffect3D* effect, int effect_type);
    void ClearCustomEffectUI();
    void RegenerateLEDPositions(ControllerTransform* transform);

    Ui::OpenRGB3DSpatialTab*    ui;
    ResourceManagerInterface*   resource_manager;

    LEDViewport3D*              viewport;

    std::vector<ControllerTransform*> controller_transforms;

    QListWidget*                available_controllers_list;
    QListWidget*                custom_controllers_list;
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
    QDoubleSpinBox*             grid_scale_spin;
    QLabel*                     selection_info_label;
    int                         custom_grid_x;
    int                         custom_grid_y;
    int                         custom_grid_z;
    float                       grid_scale_mm;

    /*---------------------------------------------------------*\
    | LED Spacing Controls (for adding controllers)           |
    \*---------------------------------------------------------*/
    QDoubleSpinBox*             led_spacing_x_spin;
    QDoubleSpinBox*             led_spacing_y_spin;
    QDoubleSpinBox*             led_spacing_z_spin;
    QComboBox*                  led_spacing_preset_combo;

    /*---------------------------------------------------------*\
    | LED Spacing Edit Controls (for selected controller)     |
    \*---------------------------------------------------------*/
    QDoubleSpinBox*             edit_led_spacing_x_spin;
    QDoubleSpinBox*             edit_led_spacing_y_spin;
    QDoubleSpinBox*             edit_led_spacing_z_spin;
    QPushButton*                apply_spacing_button;

    /*---------------------------------------------------------*\
    | User Position Reference Point (legacy - now part of     |
    | reference points system)                                 |
    \*---------------------------------------------------------*/
    UserPosition3D              user_position;

    /*---------------------------------------------------------*\
    | Effects Section Controls                                 |
    \*---------------------------------------------------------*/
    QComboBox*                  effect_combo;
    QComboBox*                  effect_origin_combo;
    QWidget*                    effect_controls_widget;
    QVBoxLayout*                effect_controls_layout;

    /*---------------------------------------------------------*\
    | Reference Points Section Controls                        |
    \*---------------------------------------------------------*/
    QListWidget*                reference_points_list;
    QLineEdit*                  ref_point_name_edit;
    QComboBox*                  ref_point_type_combo;
    QPushButton*                ref_point_color_button;
    QPushButton*                add_ref_point_button;
    QPushButton*                remove_ref_point_button;
    RGBColor                    selected_ref_point_color;

    /*---------------------------------------------------------*\
    | Zone Section Controls                                    |
    \*---------------------------------------------------------*/
    QListWidget*                zones_list;
    QPushButton*                create_zone_button;
    QPushButton*                edit_zone_button;
    QPushButton*                delete_zone_button;
    QComboBox*                  effect_zone_combo;

    std::vector<VirtualController3D*> virtual_controllers;
    std::vector<VirtualReferencePoint3D*> reference_points;
    ZoneManager3D*              zone_manager;
};

#endif