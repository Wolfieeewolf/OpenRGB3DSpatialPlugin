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
#include <QElapsedTimer>
#include <QScrollArea>
#include <QTabWidget>
#include <QLineEdit>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QProgressBar>
#include <QCompleter>
#include <vector>
#include <string>
#include <memory>
#include <atomic>

class QStackedWidget;

#include "ResourceManagerInterface.h"
#include "LEDPosition3D.h"
#include "LEDViewport3D.h"
#include "VirtualController3D.h"
#include "VirtualReferencePoint3D.h"
#include "DisplayPlane3D.h"
#include "SpatialEffectTypes.h"
#include "SpatialEffect3D.h"
#include "EffectListManager3D.h"
#include "EffectInstance3D.h"
#include "StackPreset3D.h"
// Effect headers are included in the .cpp for registration
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

    // Minimal getters for SDK surface
    float SDK_GetGridScaleMM() const { return grid_scale_mm; }
    void  SDK_GetRoomDimensions(float& w, float& d, float& h, bool& use_manual);
    size_t SDK_GetControllerCount() const { return controller_transforms.size(); }
    bool  SDK_GetControllerName(size_t idx, std::string& out) const;
    bool  SDK_IsControllerVirtual(size_t idx) const;
    int   SDK_GetControllerGranularity(size_t idx) const;
    int   SDK_GetControllerItemIndex(size_t idx) const;
    size_t SDK_GetLEDCount(size_t ctrl_idx) const;
    bool  SDK_GetLEDWorldPosition(size_t ctrl_idx, size_t led_idx, float& x, float& y, float& z) const;
    // Bulk + order helpers
    bool  SDK_GetLEDWorldPositions(size_t ctrl_idx, float* xyz_interleaved, size_t max_triplets, size_t& out_count) const;
    size_t SDK_GetTotalLEDCount() const;
    bool  SDK_GetAllLEDWorldPositions(float* xyz_interleaved, size_t max_triplets, size_t& out_count) const;
    bool  SDK_GetAllLEDWorldPositionsWithOffsets(float* xyz_interleaved, size_t max_triplets, size_t& out_triplets, size_t* ctrl_offsets, size_t offsets_capacity, size_t& out_controllers) const;
    bool  SDK_RegisterGridLayoutCallback(void (*cb)(void*), void* user);
    bool  SDK_UnregisterGridLayoutCallback(void (*cb)(void*), void* user);
    bool  SDK_SetControllerColors(size_t ctrl_idx, const unsigned int* bgr_colors, size_t count);
    bool  SDK_SetSingleLEDColor(size_t ctrl_idx, size_t led_idx, unsigned int bgr_color);
    bool  SDK_SetGridOrderColors(const unsigned int* bgr_colors_by_grid, size_t count);
    bool  SDK_SetGridOrderColorsWithOrder(int order, const unsigned int* bgr_colors_by_grid, size_t count);

signals:
    void GridLayoutChanged();

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

    // Device list update (called via QMetaObject::invokeMethod)
    void UpdateDeviceList();

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
    void SetObjectCreatorStatus(const QString& message, bool is_error = false);
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
    void SetupAudioTab(QTabWidget* tab_widget);

    Ui::OpenRGB3DSpatialTab*    ui;
    ResourceManagerInterface*   resource_manager;

    QTabWidget*                 left_tabs;
    LEDViewport3D*              viewport;

    std::vector<std::unique_ptr<ControllerTransform>> controller_transforms;

    QListWidget*                available_controllers_list;
    QListWidget*                custom_controllers_list;
    QLabel*                     object_creator_status_label;
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
    QElapsedTimer               effect_elapsed;

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
    Vector3D ComputeWorldPositionForSDK(const ControllerTransform* transform, size_t led_idx) const;
    void ComputeAutoRoomExtents(float& width_mm, float& depth_mm, float& height_mm) const;

    /*---------------------------------------------------------*\
    | Audio Tab Controls                                      |
    \*---------------------------------------------------------*/
    QWidget*        audio_tab = nullptr;
    QComboBox*      audio_device_combo = nullptr;
    QSlider*        audio_gain_slider = nullptr;   // 1..100 maps to 0.1..10.0
    // Removed input smoothing; smoothing is per-effect now
    QProgressBar*   audio_level_bar = nullptr;
    QPushButton*    audio_start_button = nullptr;
    QPushButton*    audio_stop_button = nullptr;
    QLabel*         audio_gain_value_label = nullptr;     // shows gain as e.g. 1.0x

    // Bands (crossovers removed from UI)
    QComboBox*      audio_bands_combo = nullptr;   // 8/16/32

    // Removed legacy Bass/Mid/Treble mapping combos (unused)

    // Audio effects section (moved to Audio tab)
    QComboBox*      audio_effect_combo = nullptr;
    QComboBox*      audio_effect_zone_combo = nullptr; // includes All Controllers
    QComboBox*      audio_effect_origin_combo = nullptr; // Room Center + reference points
    QWidget*        audio_effect_controls_widget = nullptr; // dynamic effect UI (like main Effects tab)
    QVBoxLayout*    audio_effect_controls_layout = nullptr;
    SpatialEffect3D* current_audio_effect_ui = nullptr;
    SpatialEffect3D* running_audio_effect = nullptr; // active instance in the renderer
    QPushButton*    audio_effect_start_button = nullptr; // start selected audio effect
    QPushButton*    audio_effect_stop_button = nullptr;  // stop selected audio effect

    // Removed legacy Areas, Channel routing, and Band Mapping UI (unused)

    /*---------------------------------------------------------*\
    | Custom Audio Effects (save/load)                        |
    \*---------------------------------------------------------*/
    QGroupBox*      audio_custom_group = nullptr;
    QListWidget*    audio_custom_list = nullptr;
    QPushButton*    audio_custom_save_btn = nullptr;
    QPushButton*    audio_custom_load_btn = nullptr;
    QPushButton*    audio_custom_delete_btn = nullptr;
    QPushButton*    audio_custom_add_to_stack_btn = nullptr;
    QLineEdit*      audio_custom_name_edit = nullptr;

private slots:
    void on_audio_device_changed(int index);
    void on_audio_gain_changed(int value);
    // Removed: on_audio_smooth_changed (smoothing moved to effect settings)
    void on_audio_start_clicked();
    void on_audio_stop_clicked();
    void on_audio_level_updated(float level);
    void on_audio_bands_changed(int index);
    // Removed: on_audio_crossovers_changed, on_apply_audio_mapping_clicked
    void on_audio_effect_start_clicked();
    void on_audio_effect_stop_clicked();
    void SetupAudioEffectUI(int eff_index);
    void on_audio_effect_origin_changed(int index);
    void UpdateAudioEffectOriginCombo();
    void on_audio_effect_zone_changed(int index);
    void UpdateAudioEffectZoneCombo();
    void OnAudioEffectParamsChanged();

    // Standard Audio Controls (Hz, smoothing, falloff)
    void SetupStandardAudioControls(QVBoxLayout* parent_layout);
    void on_audio_std_low_changed(double v);
    void on_audio_std_high_changed(double v);
    void on_audio_std_smooth_changed(int v);
    void on_audio_std_falloff_changed(int v);
    void on_audio_fft_changed(int index);
    // Removed: UpdateAudioZoneCombos
    // Removed legacy channel/band mapping slots

    // Custom audio effects helpers and slots
    void SetupAudioCustomEffectsUI(QVBoxLayout* parent_layout);
    void UpdateAudioCustomEffectsList();
    void on_audio_custom_save_clicked();
    void on_audio_custom_load_clicked();
    void on_audio_custom_delete_clicked();
    void on_audio_custom_add_to_stack_clicked();

    // Display plane management
    void on_display_plane_selected(int index);
    void on_add_display_plane_clicked();
    void on_remove_display_plane_clicked();
    void on_display_plane_name_edited(const QString& text);
    void on_display_plane_width_changed(double value);
    void on_display_plane_height_changed(double value);
    void on_display_plane_capture_changed(int index);
    void on_display_plane_refresh_capture_clicked();
    void on_display_plane_visible_toggled(Qt::CheckState state);
    void on_display_plane_position_signal(int index, float x, float y, float z);
    void on_display_plane_rotation_signal(int index, float x, float y, float z);
    void on_display_plane_monitor_preset_selected(int index);

    void on_monitor_preset_text_edited(const QString& text);
private:
    // Audio Standard Controls (data members)
    QGroupBox*      audio_std_group = nullptr;
    QDoubleSpinBox* audio_low_spin = nullptr;
    QDoubleSpinBox* audio_high_spin = nullptr;
    QSlider*        audio_smooth_slider = nullptr;
    QSlider*        audio_falloff_slider = nullptr;
    QComboBox*      audio_fft_combo = nullptr;
    QLabel*         audio_smooth_value_label = nullptr;   // shows smoothing as percent
    QLabel*         audio_falloff_value_label = nullptr;  // shows falloff mapped (e.g., 1.00x)
    std::string GetAudioCustomEffectsDir();
    std::string GetAudioCustomEffectPath(const std::string& name);

    // SDK callback listeners
    std::vector<std::pair<void (*)(void*), void*>> grid_layout_callbacks;

    /*---------------------------------------------------------*\
    | Display Plane Management                                 |
    \*---------------------------------------------------------*/
    std::vector<std::unique_ptr<DisplayPlane3D>> display_planes;
    QListWidget*    display_planes_list = nullptr;
    QLineEdit*      display_plane_name_edit = nullptr;
    QDoubleSpinBox* display_plane_width_spin = nullptr;
    QDoubleSpinBox* display_plane_height_spin = nullptr;
    QComboBox*      display_plane_capture_combo = nullptr;
    QPushButton*    display_plane_refresh_capture_btn = nullptr;
    QCheckBox*      display_plane_visible_check = nullptr;
    QPushButton*    add_display_plane_button = nullptr;
    QPushButton*    remove_display_plane_button = nullptr;
    int             current_display_plane_index = -1;

    void UpdateDisplayPlanesList();
    void RefreshDisplayPlaneDetails();
    DisplayPlane3D* GetSelectedDisplayPlane();
    void NotifyDisplayPlaneChanged();
    void SyncDisplayPlaneControls(DisplayPlane3D* plane);
    void RefreshDisplayPlaneCaptureSourceList();
    void LoadMonitorPresets();
    void PopulateMonitorPresetCombo();
    int  FindAvailableControllerRow(int type_code, int object_index) const;
    void SelectAvailableControllerEntry(int type_code, int object_index);
    void ClearMonitorPresetSelectionIfManualEdit();

    struct MonitorPreset
    {
        QString id;
        QString brand;
        QString model;
        double  width_mm;
        double  height_mm;

        QString DisplayLabel() const
        {
            return QString("%1 %2 (%3 x %4 mm)")
                .arg(brand)
                .arg(model)
                .arg(width_mm, 0, 'f', 0)
                .arg(height_mm, 0, 'f', 0);
        }
    };

    std::vector<MonitorPreset> monitor_presets;
    QComboBox*      display_plane_monitor_combo = nullptr;
    QCompleter*     monitor_preset_completer = nullptr;
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
    void GridLayoutChanged();
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













