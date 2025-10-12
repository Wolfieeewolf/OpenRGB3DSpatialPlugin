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
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <memory>
#include <atomic>

#include "ResourceManagerInterface.h"
#include "LEDPosition3D.h"
#include "LEDViewport3D.h"
#include "VirtualController3D.h"
#include "VirtualReferencePoint3D.h"
#include "SpatialEffectTypes.h"
#include "SpatialEffect3D.h"
#include "EffectListManager3D.h"
#include "EffectInstance3D.h"
#include "StackPreset3D.h"
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

// Forward declaration for worker thread
class EffectWorkerThread3D;

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
    void on_save_effect_profile_clicked();
    void on_load_effect_profile_clicked();
    void on_delete_effect_profile_clicked();
    void on_effect_profile_changed(int index);
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
    void RenderEffectStack();
    void on_grid_dimensions_changed();
    void on_grid_snap_toggled(bool enabled);
    void on_effect_changed(int index);
    void on_effect_origin_changed(int index);
    void UpdateSelectionInfo();
    void on_apply_spacing_clicked();
    void UpdateEffectOriginCombo();

    // Effect Stack slots
    void on_add_effect_to_stack_clicked();
    void on_remove_effect_from_stack_clicked();
    void on_effect_stack_item_double_clicked(QListWidgetItem* item);
    void on_effect_stack_selection_changed(int index);
    void on_stack_effect_type_changed(int index);
    void on_stack_effect_zone_changed(int index);
    void on_stack_effect_blend_changed(int index);

    // Stack Preset slots
    void on_save_stack_preset_clicked();
    void on_load_stack_preset_clicked();
    void on_delete_stack_preset_clicked();

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
    void UpdateEffectCombo();
    void SaveZones();
    void LoadZones();
    bool IsItemInScene(RGBController* controller, int granularity, int item_idx);
    int GetUnassignedZoneCount(RGBController* controller);
    int GetUnassignedLEDCount(RGBController* controller);
    void SetupCustomEffectUI(int effect_type);
    void SetupStackPresetUI();
    void ClearCustomEffectUI();
    void RegenerateLEDPositions(ControllerTransform* transform);

    // Effect Stack helpers
    void SetupEffectStackTab(QTabWidget* tab_widget);
    void UpdateEffectStackList();
    void UpdateStackEffectZoneCombo();
    void LoadStackEffectControls(EffectInstance3D* instance);

    // Stack Preset helpers
    void LoadStackPresets();
    void SaveStackPresets();
    void UpdateStackPresetsList();
    std::string GetStackPresetsPath();

    // Effect Stack persistence
    void SaveEffectStack();
    void LoadEffectStack();
    std::string GetEffectStackPath();

    // Effect Profile helpers
    void SaveEffectProfile(const std::string& filename);
    void LoadEffectProfile(const std::string& filename);
    std::string GetEffectProfilePath(const std::string& profile_name);
    void PopulateEffectProfileDropdown();
    void SaveCurrentEffectProfileName();
    void TryAutoLoadEffectProfile();

    // Profiles tab setup
    void SetupProfilesTab(QTabWidget* tab_widget);

    Ui::OpenRGB3DSpatialTab*    ui;
    ResourceManagerInterface*   resource_manager;

    QTabWidget*                 left_tabs;
    LEDViewport3D*              viewport;

    std::vector<std::unique_ptr<ControllerTransform>> controller_transforms;

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
    QComboBox*                  effect_profiles_combo;
    QCheckBox*                  effect_auto_load_checkbox;
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
    | Room Dimension Settings (Manual room size)              |
    \*---------------------------------------------------------*/
    QDoubleSpinBox*             room_width_spin;
    QDoubleSpinBox*             room_depth_spin;
    QDoubleSpinBox*             room_height_spin;
    QCheckBox*                  use_manual_room_size_checkbox;
    float                       manual_room_width;
    float                       manual_room_depth;
    float                       manual_room_height;
    bool                        use_manual_room_size;

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

    std::vector<std::unique_ptr<VirtualController3D>> virtual_controllers;
    std::vector<std::unique_ptr<VirtualReferencePoint3D>> reference_points;
    std::unique_ptr<ZoneManager3D> zone_manager;

    /*---------------------------------------------------------*\
    | Effect Stack Section Controls                            |
    \*---------------------------------------------------------*/
    QListWidget*                effect_stack_list;
    QComboBox*                  stack_effect_type_combo;
    QComboBox*                  stack_effect_zone_combo;
    QComboBox*                  stack_effect_blend_combo;
    QWidget*                    stack_effect_controls_container;
    QVBoxLayout*                stack_effect_controls_layout;

    /*---------------------------------------------------------*\
    | Effect Stack Data                                        |
    \*---------------------------------------------------------*/
    std::vector<std::unique_ptr<EffectInstance3D>> effect_stack;
    int next_effect_instance_id;

    /*---------------------------------------------------------*\
    | Stack Presets                                            |
    \*---------------------------------------------------------*/
    QListWidget*                stack_presets_list;
    std::vector<std::unique_ptr<StackPreset3D>> stack_presets;

    /*---------------------------------------------------------*\
    | Background Threading for Effect Calculation              |
    \*---------------------------------------------------------*/
    EffectWorkerThread3D* worker_thread;
    void ApplyColorsFromWorker();
};

/*---------------------------------------------------------*\
| Background Effect Worker Thread (outside main class)     |
\*---------------------------------------------------------*/
class EffectWorkerThread3D : public QThread
{
    Q_OBJECT
public:
    EffectWorkerThread3D(QObject* parent = nullptr);
    ~EffectWorkerThread3D();

    void StartEffect(SpatialEffect3D* effect,
                    const std::vector<std::unique_ptr<ControllerTransform>>& transforms,
                    const std::vector<std::unique_ptr<VirtualReferencePoint3D>>& ref_points,
                    ZoneManager3D* zone_mgr,
                    int active_zone_idx);
    void StopEffect();
    void UpdateTime(float time);

    // Get calculated colors (thread-safe)
    bool GetColors(std::vector<RGBColor>& out_colors, std::vector<LEDPosition3D*>& out_leds);

signals:
    void ColorsReady();

protected:
    void run() override;

private:
    struct ColorBuffer
    {
        std::vector<RGBColor> colors;
        std::vector<LEDPosition3D*> leds;
    };

    std::atomic<bool> running{false};
    std::atomic<bool> should_stop{false};
    std::atomic<float> current_time{0.0f};

    QMutex state_mutex;
    QMutex buffer_mutex;
    QWaitCondition start_condition;

    SpatialEffect3D* effect;
    std::vector<std::unique_ptr<ControllerTransform>> transform_snapshots;
    std::vector<std::unique_ptr<VirtualReferencePoint3D>> ref_point_snapshots;
    std::unique_ptr<ZoneManager3D> zone_snapshot;
    int active_zone;

    ColorBuffer front_buffer;
    ColorBuffer back_buffer;
};

#endif