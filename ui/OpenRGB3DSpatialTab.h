// SPDX-License-Identifier: GPL-2.0-only

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
#include <QProgressBar>
#include <QCompleter>
#include <vector>
#include <string>
#include <memory>
#include <nlohmann/json.hpp>

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

class OpenRGB3DSpatialTab : public QWidget
{
    Q_OBJECT

public:
    explicit OpenRGB3DSpatialTab(ResourceManagerInterface* rm, QWidget *parent = nullptr);
    ~OpenRGB3DSpatialTab();

    /** Current grid scale (mm per unit) used by viewport and effects for consistent math. */
    float GetGridScaleMM() const { return grid_scale_mm; }

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

    void on_quick_save_layout_clicked();
    void on_save_layout_clicked();
    void on_load_layout_clicked();
    void on_delete_layout_clicked();
    void on_layout_profile_changed(int index);
    void on_save_effect_profile_clicked();
    void on_load_effect_profile_clicked();
    void on_delete_effect_profile_clicked();
    void on_effect_profile_changed(int index);
    void on_create_custom_controller_clicked();
    void on_add_from_preset_clicked();
    void on_import_custom_controller_clicked();
    void on_export_custom_controller_clicked();
    void on_edit_custom_controller_clicked();
    void on_delete_custom_controller_clicked();
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
    void on_effect_zone_changed(int index);
    void UpdateSelectionInfo();
    void on_apply_spacing_clicked();
    void UpdateEffectOriginCombo();

    // Effect library slots
    void on_effect_library_category_changed(int index);
    void on_effect_library_add_clicked();
    void on_effect_library_item_double_clicked(QListWidgetItem* item);
    void on_effect_library_selection_changed(int row);

    // Effect Stack slots
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
    static const int kEffectRoleClassName = Qt::UserRole;
    static const int kEffectRoleInstanceId = Qt::UserRole + 1;

    void SetupUI();
    void LoadDevices();
    void SaveLayout(const std::string& filename);
    void LoadLayout(const std::string& filename);
    void LoadLayoutFromJSON(const nlohmann::json& layout_json);
    std::string GetLayoutPath(const std::string& layout_name);
    void TryAutoLoadLayout();
    void PopulateLayoutDropdown();
    void SaveCurrentLayoutName();
    
    // Dirty flag system
    void SetLayoutDirty(bool dirty = true);
    void ClearLayoutDirty();
    bool IsLayoutDirty() const { return layout_dirty; }
    bool PromptSaveIfDirty();
    void SaveCustomControllers();
    void LoadCustomControllers();
    void UpdateAvailableItemCombo();
    void UpdateAvailableControllersList();
    void UpdateCustomControllersList();
    int  FindDisplayPlaneIndexById(int plane_id) const;
    void RemoveDisplayPlaneControllerEntries(int plane_id);
    void RemoveReferencePointControllerEntries(int removed_index);
    /** Maps controller_list row to controller_transforms index; -1 if that row is not a transform. */
    int  ControllerListRowToTransformIndex(int row) const;
    /** Maps controller_transforms index to controller_list row; -1 if not found. */
    int  TransformIndexToControllerListRow(int transform_index) const;
    void SetObjectCreatorStatus(const QString& message, bool is_error = false);
    void UpdateReferencePointsList();
    void SaveReferencePoints();
    void LoadReferencePoints();
    void UpdateZonesList();
    void UpdateEffectZoneCombo();
    void PopulateZoneTargetCombo(QComboBox* combo, int saved_value);
    void RefreshHiddenControllerStates();
    int ResolveZoneTargetSelection(const QComboBox* combo) const;
    void UpdateEffectCombo();
    void SaveZones();
    void LoadZones();
    bool IsItemInScene(RGBController* controller, int granularity, int item_idx);
    int GetUnassignedZoneCount(RGBController* controller);
    int GetUnassignedLEDCount(RGBController* controller);
    void SetupCustomEffectUI(const QString& class_name);
    void SetupStackPresetUI();
    void ClearCustomEffectUI();
    void RegenerateLEDPositions(ControllerTransform* transform);
    void ApplyPositionComponent(int axis, double value);
    void ApplyRotationComponent(int axis, double value);
    void RemoveWidgetFromParentLayout(QWidget* w);

    // Effect Stack helpers
    void SetupEffectStackPanel(QVBoxLayout* parent_layout);
    void UpdateEffectStackList();
    void UpdateStackEffectZoneCombo();
    void LoadStackEffectControls(EffectInstance3D* instance);
    void DisplayEffectInstanceDetails(EffectInstance3D* instance);
    bool PrepareStackForPlayback();
    void SetControllersToCustomMode(bool& has_valid_controller);

    // Stack Preset helpers
    void LoadStackPresets();
    void SaveStackPresets();
    void UpdateStackPresetsList();
    std::string GetStackPresetsPath();

    nlohmann::json GetPluginSettings() const;
    void SetPluginSettings(const nlohmann::json& settings);
    void SetPluginSettingsNoSave(const nlohmann::json& settings);
    void RefreshEffectDisplay();

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
    void SetupAudioPanel(QVBoxLayout* parent_layout);
    void SetupZonesPanel(QVBoxLayout* parent_layout);
    void SetupEffectLibraryPanel(QVBoxLayout* parent_layout);
    void PopulateEffectLibraryCategories();
    void PopulateEffectLibrary();
    void AddEffectInstanceToStack(const QString& class_name,
                                  const QString& ui_name,
                                  int zone_index = -1,
                                  BlendMode blend_mode = BlendMode::NO_BLEND,
                                  const nlohmann::json* preset_settings = nullptr,
                                  bool enabled = true);

    Ui::OpenRGB3DSpatialTab*    ui;
    ResourceManagerInterface*   resource_manager;

    QTabWidget*                 left_tabs;
    LEDViewport3D*              viewport;

    std::vector<std::unique_ptr<ControllerTransform>> controller_transforms;

    QListWidget*                available_controllers_list;
    QListWidget*                custom_controllers_list;
    QLabel*                     custom_controllers_empty_label;
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

    // Effect Library
    QComboBox*                  effect_category_combo;
    QListWidget*                effect_library_list;
    QPushButton*                effect_library_add_button;

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
    bool                        stack_settings_updating;

    // Grid (custom dimensions)
    QSpinBox*                   grid_x_spin;
    QSpinBox*                   grid_y_spin;
    QSpinBox*                   grid_z_spin;
    QCheckBox*                  grid_snap_checkbox;
    QDoubleSpinBox*             grid_scale_spin;
    QLabel*                     selection_info_label;
    QCheckBox*                  room_grid_overlay_checkbox;
    QSlider*                    room_grid_brightness_slider;
    QLabel*                     room_grid_brightness_label;
    QSlider*                    room_grid_point_size_slider;
    QLabel*                     room_grid_point_size_label;
    QSlider*                    room_grid_step_slider;
    QLabel*                     room_grid_step_label;
    int                         custom_grid_x;
    int                         custom_grid_y;
    int                         custom_grid_z;
    float                       grid_scale_mm;

    // Room dimensions (manual)
    QDoubleSpinBox*             room_width_spin;
    QDoubleSpinBox*             room_depth_spin;
    QDoubleSpinBox*             room_height_spin;
    QCheckBox*                  use_manual_room_size_checkbox;
    float                       manual_room_width;
    float                       manual_room_depth;
    float                       manual_room_height;
    bool                        use_manual_room_size;

    // LED spacing (add)
    QDoubleSpinBox*             led_spacing_x_spin;
    QDoubleSpinBox*             led_spacing_y_spin;
    QDoubleSpinBox*             led_spacing_z_spin;
    QComboBox*                  led_spacing_preset_combo;

    // LED spacing (edit selected)
    QDoubleSpinBox*             edit_led_spacing_x_spin;
    QDoubleSpinBox*             edit_led_spacing_y_spin;
    QDoubleSpinBox*             edit_led_spacing_z_spin;
    QPushButton*                apply_spacing_button;

    // Effect configuration
    QComboBox*                  effect_combo;
    QLabel*                     origin_label;
    QComboBox*                  effect_origin_combo;
    QWidget*                    effect_controls_widget;
    QVBoxLayout*                effect_controls_layout;

    // Reference points
    QListWidget*                reference_points_list;
    QLabel*                     ref_points_empty_label;
    QLineEdit*                  ref_point_name_edit;
    QComboBox*                  ref_point_type_combo;
    QPushButton*                ref_point_color_button;
    QPushButton*                add_ref_point_button;
    QPushButton*                remove_ref_point_button;
    RGBColor                    selected_ref_point_color;

    // Zones
    QListWidget*                zones_list;
    QPushButton*                create_zone_button;
    QPushButton*                edit_zone_button;
    QPushButton*                delete_zone_button;
    QComboBox*                  effect_zone_combo;

    std::vector<std::unique_ptr<VirtualController3D>> virtual_controllers;
    std::unique_ptr<VirtualController3D> preview_virtual_controller;
    std::vector<std::unique_ptr<VirtualReferencePoint3D>> reference_points;
    std::unique_ptr<ZoneManager3D> zone_manager;

    // Effect stack
    QListWidget*                effect_stack_list;
    QComboBox*                  stack_effect_type_combo;
    QComboBox*                  stack_effect_zone_combo;
    QComboBox*                  stack_effect_blend_combo;
    QWidget*                    stack_blend_container;

    // Effect stack data
    std::vector<std::unique_ptr<EffectInstance3D>> effect_stack;
    int next_effect_instance_id;

    // Stack presets
    QListWidget*                stack_presets_list;
    std::vector<std::unique_ptr<StackPreset3D>> stack_presets;

    void ComputeAutoRoomExtents(float& width_mm, float& depth_mm, float& height_mm) const;

    // Audio tab
    QGroupBox*      audio_panel_group = nullptr;
    QComboBox*      audio_device_combo = nullptr;
    QSlider*        audio_gain_slider = nullptr;   // 1..100 maps to 0.1..10.0
    QProgressBar*   audio_level_bar = nullptr;
    QPushButton*    audio_start_button = nullptr;
    QPushButton*    audio_stop_button = nullptr;
    QLabel*         audio_gain_value_label = nullptr;     // shows gain as e.g. 1.0x

    QComboBox*      audio_bands_combo = nullptr;   // 8/16/32

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

    // Audio custom effects
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
    void on_audio_start_clicked();
    void on_audio_stop_clicked();
    void on_audio_level_updated(float level);
    void on_audio_bands_changed(int index);
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
    void UpdateAudioPanelVisibility(EffectInstance3D* instance);
    void on_audio_std_low_changed(double v);
    void on_audio_std_high_changed(double v);
    void on_audio_std_smooth_changed(int v);
    void on_audio_std_falloff_changed(int v);
    void on_audio_fft_changed(int index);

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
    void on_monitor_filter_or_sort_changed(int);

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

    // Dirty flag tracking
    bool layout_dirty = false;
    QPushButton* save_layout_btn = nullptr;

    // Display planes
    std::vector<std::unique_ptr<DisplayPlane3D>> display_planes;
    QListWidget*    display_planes_list = nullptr;
    QLabel*         display_planes_empty_label = nullptr;
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
    /** Update only the current display plane list item label (name + dimensions); avoids full repopulate + selection chain. */
    void UpdateCurrentDisplayPlaneListItemLabel();
    void RefreshDisplayPlaneDetails();
    DisplayPlane3D* GetSelectedDisplayPlane();
    void NotifyDisplayPlaneChanged();
    void SyncDisplayPlaneControls(DisplayPlane3D* plane);
    void RefreshDisplayPlaneCaptureSourceList();
    void LoadMonitorPresets();
    void PopulateMonitorPresetCombo();
    int  FindAvailableControllerRow(int type_code, int object_index) const;
    void SelectAvailableControllerEntry(int type_code, int object_index);
    /** Add a custom controller (by virtual_controllers index) to the 3D scene. Returns true if added. */
    bool AddCustomControllerToScene(int virtual_controller_index);
    /** Add a temporary preview of the current dialog state to the 3D viewport. Removed on ClearCustomControllerPreview(). */
    void AddCustomControllerPreview(CustomControllerDialog* dialog);
    /** Remove the temporary custom controller preview from the viewport, if any. */
    void ClearCustomControllerPreview();
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

    static bool ParseMonitorPresetEntry(const nlohmann::json& entry, const QString& file_id, MonitorPreset& out_preset);

    std::vector<MonitorPreset> monitor_presets;
    QComboBox*      display_plane_monitor_combo = nullptr;
    QCompleter*     monitor_preset_completer = nullptr;
    QComboBox*      display_plane_monitor_brand_filter = nullptr;
    QComboBox*      display_plane_monitor_sort_combo = nullptr;
};

#endif






