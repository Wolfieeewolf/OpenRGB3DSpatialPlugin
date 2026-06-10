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
#include <QHash>
#include <QVector3D>
#include <vector>
#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include "filesystem.h"

class QHideEvent;
class QShowEvent;
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
#include "ZoneManager3D.h"
#include "SpatialControllerEntryKey.h"
#include "SpatialControllerListBacking.h"

class SpatialControllerCardList;
class SpatialControllerCardWidget;
class GridSettingsPanel;
class SceneTransformPanel;
class SceneObjectEditHostPanel;
class ObjectCreatorTabPanel;
class ControllerListPanel;
class PositionAxisDragController;

namespace Ui
{
class OpenRGB3DSpatialTab;
}

class OpenRGB3DSpatialTab : public QWidget
{
    Q_OBJECT

    friend class ProfilesTabPanel;
    friend class GridSettingsPanel;
    friend class SceneTransformPanel;
    friend class ObjectCreatorTabPanel;
    friend class ControllerListPanel;
    friend class EffectLibraryPanel;
    friend class EffectStackPanel;
    friend class ZonesPanel;
    friend class EffectGlobalSettingsPanel;
    friend class MinecraftLibraryPanel;
    friend class AudioInputPanel;
    friend class AudioAdvancedSettingsDialog;
    friend class EffectControlsHostPanel;
    friend class SceneObjectEditHostPanel;
    friend class SceneObjectSpacingPanel;
    friend class PositionAxisDragController;
    friend class DisplayPlaneDialog;

public:
    explicit OpenRGB3DSpatialTab(ResourceManagerInterface* rm, QWidget *parent = nullptr);
    ~OpenRGB3DSpatialTab();

    float GetGridScaleMM() const { return grid_scale_mm; }

    void SavePluginUiSettings();

    double EffectiveGridScaleMm() const;
    void SetScenePositionControlsMm(double x_mm, double y_mm, double z_mm);
    void ApplyScenePositionAbsoluteMm(int axis, double value_mm);
    void ScenePositionAxisLimitsMm(int axis, double& min_mm, double& max_mm) const;

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

signals:
    void GridLayoutChanged();

private slots:
    void on_start_effect_clicked();
    void on_stop_effect_clicked();

    void on_controller_selected(int index);
    void on_viewport_controller_selected(int transform_index);
    void on_viewport_display_plane_selected(int plane_index);
    void on_controller_position_changed(int index, float x, float y, float z);
    void on_controller_rotation_changed(int index, float x, float y, float z);

    void on_available_card_add(SpatialControllerEntryKey key,
                             int                            granularity,
                             int                            item_index,
                             float                          spacing_x_mm,
                             float                          spacing_y_mm,
                             float                          spacing_z_mm);
    void on_scene_card_remove(int scene_list_row);
    void on_remove_controller_clicked();
    void on_remove_controller_from_viewport(int index);
    void on_clear_all_clicked();
    void on_scene_controller_cards_selection_changed(int scene_list_row);
    void on_scene_card_edit(int scene_list_row);

    void ShowSceneObjectEditPanel(int scene_list_row, bool sync_scene_selection = true);
    void HideSceneObjectEditPanel();
    void SyncSceneObjectSpacingPanel();
    void EditCustomControllerForCurrentSceneSelection();
    void EditReferencePointForCurrentSceneSelection();
    void EditDisplayPlaneForCurrentSceneSelection();
    void FocusObjectCreatorTab();

    void on_quick_save_layout_clicked();
    void on_save_layout_clicked();
    void on_load_layout_clicked();
    void on_delete_layout_clicked();
    void on_layout_profile_changed(int index);
    void on_save_effect_profile_clicked();
    void on_load_effect_profile_clicked();
    void on_delete_effect_profile_clicked();
    void on_effect_profile_changed(int index);
    void on_open_config_folder_clicked();
    void on_create_custom_controller_clicked();
    void on_import_custom_controller_clicked();
    void on_export_custom_controller_clicked();
    void on_edit_custom_controller_clicked();
    void on_delete_custom_controller_clicked();
    void on_custom_controller_selection_changed(int row);
    void on_add_ref_point_clicked();
    void on_edit_reference_point_clicked();
    void on_remove_ref_point_clicked();
    void on_reference_points_list_selection_changed(int row);
    void on_ref_point_selected(int index, bool from_scene_controller_list = false);
    void on_ref_point_position_changed(int index, float x, float y, float z);

    void on_create_zone_clicked();
    void on_edit_zone_clicked();
    void on_delete_zone_clicked();
    void on_zone_selected(int index);

    void on_effect_timer_timeout();
    void RenderEffectStack();
    void on_grid_dimensions_changed();
    void on_grid_snap_toggled(bool enabled);
    void on_gpu_labels_toggled(bool enabled);
    void on_gpu_scene_toggled(bool enabled);
    void on_room_guide_labels_toggled(bool enabled);
    void on_frame_selection_in_view();
    void on_reset_viewport_camera();
    void on_effect_changed(int index);
    void on_effect_origin_changed(int index);
    void on_effect_bounds_changed(int index);
    void on_effect_zone_changed(int index);
    void UpdateSelectionInfo();
    void UpdateEffectOriginCombo();

    void on_effect_library_category_changed(int index);
    void on_effect_library_search_changed(const QString& text);
    void on_effect_library_add_clicked();
    void on_effect_library_item_double_clicked(QListWidgetItem* item);
    void on_effect_library_selection_changed(int row);
    void on_minecraft_library_layer_combo_changed(int index);
    void on_minecraft_library_add_clicked();

    void on_start_all_effects_clicked();
    void on_stop_all_effects_clicked();
    void UpdateStartStopAllButtons();
    void on_remove_effect_from_stack_clicked();
    void on_effect_stack_item_double_clicked(QListWidgetItem* item);
    void on_effect_stack_selection_changed(int index);
    void on_stack_effect_type_changed(int index);
    void on_stack_effect_zone_changed(int index);
    void on_stack_effect_blend_changed(int index);

    void on_save_stack_preset_clicked();
    void on_load_stack_preset_clicked();
    void on_delete_stack_preset_clicked();

    void UpdateDeviceList();

    void PopulateAvailableItemCombo(const SpatialControllerEntryKey& key, int granularity, QComboBox* combo);
    QList<SpatialControllerEntryKey> GetAvailableControllerKeys() const;
    QList<QString>                   GetAvailableControllerTitles() const;
    QList<bool>                      GetAvailableControllerGranularityFlags() const;
    void GetSuggestedSpacingForAvailableRgb(int controller_index, float& x_mm, float& y_mm, float& z_mm) const;
    void RememberAvailableRgbSpacingDraft(int controller_index, float x_mm, float y_mm, float z_mm);
    bool GetTransformLedSpacing(int transform_index, float& x_mm, float& y_mm, float& z_mm) const;
    void ApplyLedSpacingToTransform(int transform_index, float x_mm, float y_mm, float z_mm);
    int  ControllerListRowToTransformIndex(int row) const;

    void ApplyPositionComponent(int axis, double value);
    void ApplyRotationComponent(int axis, double value);

private:
    friend class SpatialControllerCardList;
    friend class SpatialControllerCardWidget;
    static const int kEffectRoleClassName = Qt::UserRole;
    static const int kEffectRoleInstanceId = Qt::UserRole + 1;

    void bindUiPanels();
    void RunDeferredStartupTasks();
    void LoadDevices();
    void SaveLayout(const std::string& filename);
    void LoadLayout(const std::string& filename);
    void LoadLayoutFromJSON(const nlohmann::json& layout_json);
    std::string GetLayoutPath(const std::string& layout_name);
    void TryAutoLoadLayout();
    void PopulateLayoutDropdown();
    void SaveCurrentLayoutName();
    
    void SetLayoutDirty(bool dirty = true);
    void ClearLayoutDirty();
    bool IsLayoutDirty() const { return layout_dirty; }
    bool PromptSaveIfDirty();
    void SaveCustomControllers();
    void LoadCustomControllers();
    void RebindCustomControllerDeviceMappings();
    bool LoadControllerFromJsonFile(const filesystem::path& json_path, bool replace_if_same_filename);
    void RemoveVirtualControllerFromLibrary(int index);
    static std::string SafeControllerJsonFilename(const std::string& controller_name);
    void UpdateAvailableItemCombo();
    void RebuildAvailableControllerCards();
    void RebuildSceneControllerCards();

    int                       sceneControllerRowCount() const { return scene_controllers_.count(); }
    int                       sceneControllerCurrentRow() const { return scene_controllers_.currentRow(); }
    QString                   sceneControllerRowText(int row) const { return scene_controllers_.textAt(row); }
    bool                      sceneControllerRowHasUserRole(int row) const { return scene_controllers_.hasUserRole(row); }
    SpatialControllerEntryKey sceneControllerRowKey(int row) const;
    void AddControllerEntryToScene(int  ctrl_idx,
                                   int  granularity,
                                   int  item_row,
                                   float spacing_x_mm,
                                   float spacing_y_mm,
                                   float spacing_z_mm,
                                   bool  show_messages = true);
    void LoadDefaultLedSpacingFromSettings();
    void SaveDefaultLedSpacingToSettings();
    int  CreateControllerLinkedReferencePoint(int transform_index, const QString& base_name);
    void EnsureControllerLinkedReferencePoint(int transform_index, const QString& base_name);
    void SyncControllerLinkedReferencePoint(int transform_index);
    void SyncAllControllerLinkedReferencePoints();
    void RemoveControllerLinkedReferencePoint(int transform_index);
    bool IsDeviceLinkedReferencePoint(int ref_index) const;
    int  ReferencePointIndexFromListRow(int list_row) const;
    int  FindReferencePointListRow(int ref_index) const;
    void UpdateAvailableControllersList();
    void UpdateCustomControllersList();
    int  FindDisplayPlaneIndexById(int plane_id) const;
    int  FindSceneRowForReferencePoint(int ref_index) const;
    int  FindSceneRowForDisplayPlane(int plane_index) const;
    void RemoveDisplayPlaneControllerEntries(int plane_id);
    void RemoveReferencePointControllerEntries(int removed_index);
    int  TransformIndexToControllerListRow(int transform_index) const;
    void SetObjectCreatorStatus(const QString& message, bool is_error = false);
    void UpdateReferencePointsList();
    void SaveReferencePoints();
    void UpdateZonesList();
    void UpdateEffectZoneCombo();
    void PopulateZoneTargetCombo(QComboBox* combo, int saved_value);
    void RefreshHiddenControllerStates();
    int ResolveZoneTargetSelection(const QComboBox* combo) const;
    void UpdateEffectCombo();
    void SaveZones();
    bool IsItemInScene(RGBController* controller, int granularity, int item_idx) const;
    int GetUnassignedZoneCount(RGBController* controller) const;
    int GetUnassignedLEDCount(RGBController* controller) const;
    struct EffectSettingsUiMount
    {
        QWidget* container = nullptr;
        SpatialEffect3D* effect = nullptr;
    };
    SpatialEffectSettingsLayout settingsLayoutForClass(const std::string& class_name) const;
    EffectSettingsUiMount createEffectSettingsUi(QWidget* parent,
                                                 QBoxLayout* target_layout,
                                                 const std::string& class_name,
                                                 SpatialEffectSettingsLayout layout);
    void configureScreenMirrorEffectUi(SpatialEffect3D* effect);
    void setStackLayerGlobalChromeVisible(bool visible);
    void ClearCustomEffectUI();
    void RegenerateLEDPositions(ControllerTransform* transform);

    bool effectLibraryRowIsMinecraftHub(int row) const;
    void ShowMinecraftHubConfigurator();
    void rebuildMinecraftHubPreviewEffect();
    void ClearMinecraftLibraryPanel();
    void UpdateEffectStackRowSelectorVisibility();

    void UpdateEffectStackList();
    void UpdateStackEffectZoneCombo();
    void LoadStackEffectControls(EffectInstance3D* instance);
    void DisplayEffectInstanceDetails(EffectInstance3D* instance);
    bool PrepareStackForPlayback();
    void SetControllersToCustomMode(bool& has_valid_controller);

    void LoadStackPresets();
    void SaveStackPresets();
    void UpdateStackPresetsList();
    std::string GetStackPresetsPath();

    nlohmann::json GetPluginSettings() const;
    void SetPluginSettings(const nlohmann::json& settings);
    void SetPluginSettingsNoSave(const nlohmann::json& settings);
    void PersistRoomGridOverlayToSettings();
    void MergePluginUiIntoSettings(nlohmann::json& settings) const;
    void RestoreProfileUiFromSettings();
    void RefreshEffectDisplay();
    void SyncSpatialLightingSceneForUi();

    void SaveEffectStack();
    void LoadEffectStack();
    std::string GetEffectStackPath();
    bool RebuildEffectStackFromJson(const nlohmann::json& effects_array);
    void ApplyLoadedStackSelection(int desired_index);

    void SaveEffectProfile(const std::string& filename);
    void LoadEffectProfile(const std::string& filename);
    std::string GetEffectProfilePath(const std::string& profile_name);
    void PopulateEffectProfileDropdown();
    void SaveCurrentEffectProfileName();
    void TryAutoLoadEffectProfile();

    void BindSettingsPanels();
    void InitSceneObjectEditTab();
    bool IsSceneObjectEditTabActive() const;
    SceneTransformPanel* sceneTransformPanelWidget() const;
    void                   MaybeHideSceneObjectEditOnDeselect();
    bool                   HasSceneObjectEditTarget() const;
    bool ControllerTransformHasVirtualController(int transform_index) const;
    int  FindVirtualControllerLibraryIndex(const VirtualController3D* vc) const;
    void InitLedViewport();
    bool IsAmbilightEffectClass(const std::string& class_name) const;
    void RefreshAmbilightReferencePointDropdowns();
    void UpdateAudioPanelVisibility();
    bool IsAudioEffectClass(const std::string& class_name) const;
    void PopulateEffectLibraryCategories();
    void PopulateEffectLibrary();
    void AddEffectInstanceToStack(const QString& class_name,
                                  const QString& ui_name,
                                  int zone_index = -1,
                                  BlendMode blend_mode = BlendMode::NO_BLEND,
                                  const nlohmann::json* preset_settings = nullptr,
                                  bool enabled = true,
                                  bool keep_minecraft_hub_panel_after_add = false);

    QListWidget* zonesList() const;
    QPushButton* editZoneButton() const;
    QPushButton* deleteZoneButton() const;
    QComboBox*   effectCategoryCombo() const;
    QLineEdit*   effectLibrarySearch() const;
    QListWidget* effectLibraryList() const;
    QPushButton* effectLibraryAddButton() const;
    QListWidget* effectStackList() const;
    QListWidget* stackPresetsList() const;
    QPushButton* startAllEffectsButton() const;
    QPushButton* stopAllEffectsButton() const;
    QGroupBox*   effectConfigGroup() const;
    QLabel*      effectStackRowLabel() const;
    QComboBox*   effectCombo() const;
    QLabel*      effectZoneLabel() const;
    QComboBox*   effectZoneCombo() const;
    QLabel*      originLabel() const;
    QComboBox*   effectOriginCombo() const;
    QLabel*      effectBoundsLabel() const;
    QComboBox*   effectBoundsCombo() const;
    QComboBox*   stackEffectTypeCombo() const;
    QComboBox*   stackEffectZoneCombo() const;

    QComboBox*   layoutProfilesCombo() const;
    QPushButton* saveLayoutButton() const;
    QCheckBox*   autoLoadLayoutCheckbox() const;
    QComboBox*   effectProfilesCombo() const;
    QCheckBox*   autoLoadEffectProfileCheckbox() const;

    QSpinBox*    gridXSpin() const;
    QSpinBox*    gridYSpin() const;
    QSpinBox*    gridZSpin() const;
    QCheckBox*   gridSnapCheckbox() const;
    QDoubleSpinBox* gridScaleSpin() const;
    QLabel*      selectionInfoLabel() const;
    QCheckBox*   roomGridOverlayCheckbox() const;
    QDoubleSpinBox* roomWidthSpin() const;
    QDoubleSpinBox* roomDepthSpin() const;
    QDoubleSpinBox* roomHeightSpin() const;
    QCheckBox*   useManualRoomSizeCheckbox() const;

    QDoubleSpinBox* posXSpin() const;
    QDoubleSpinBox* posYSpin() const;
    QDoubleSpinBox* posZSpin() const;
    QSlider*     posXSlider() const;
    QSlider*     posYSlider() const;
    QSlider*     posZSlider() const;
    QDoubleSpinBox* rotXSpin() const;
    QDoubleSpinBox* rotYSpin() const;
    QDoubleSpinBox* rotZSpin() const;
    QSlider*     rotXSlider() const;
    QSlider*     rotYSlider() const;
    QSlider*     rotZSlider() const;

    QWidget*     effectControlsWidget() const;
    QVBoxLayout* effectControlsLayout() const;

    QComboBox*   minecraftLibraryLayerCombo() const;
    QWidget*     minecraftHubPreviewHolder() const;

    QComboBox*      audioDeviceCombo() const;
    QSlider*        audioGainSlider() const;
    QSlider*        audioClaritySlider() const;
    QLabel*         audioClarityValueLabel() const;
    QSlider*        audioIsolationSlider() const;
    QLabel*         audioIsolationValueLabel() const;
    QComboBox*      audioMixPresetCombo() const;
    QProgressBar*   audioLevelBar() const;
    QProgressBar*   audioBassBar() const;
    QProgressBar*   audioMidBar() const;
    QProgressBar*   audioHighBar() const;
    QProgressBar*   audioKickStemBar() const;
    QProgressBar*   audioSnareStemBar() const;
    QProgressBar*   audioHihatStemBar() const;
    QProgressBar*   audioBassStemBar() const;
    QPushButton*    audioStartButton() const;
    QPushButton*    audioStopButton() const;
    QLabel*         audioGainValueLabel() const;
    QComboBox*      audioBandsCombo() const;
    QComboBox*      audioFftCombo() const;
    QLabel*         audioSpectrumLabel() const;
    QLabel*         audioEqCaption() const;
    QScrollArea*    audioEqScroll() const;

    QLabel*         objectCreatorStatusLabel() const;
    QListWidget*    customControllersList() const;
    QWidget*        customControllersEmptyLabel() const;
    QPushButton*    exportCustomControllerButton() const;
    QPushButton*    editCustomControllerButton() const;
    QPushButton*    deleteCustomControllerButton() const;
    QListWidget*    referencePointsList() const;
    QWidget*        refPointsEmptyLabel() const;
    QPushButton*    editReferencePointButton() const;
    QPushButton*    removeRefPointButton() const;
    QListWidget*    displayPlanesList() const;
    QWidget*        displayPlanesEmptyLabel() const;
    QPushButton*    editDisplayPlaneButton() const;
    QPushButton*    removeDisplayPlaneButton() const;

    void FillDisplayPlaneCaptureCombo(QComboBox* combo, const std::string& prefer_source_id);

    SpatialControllerCardList* availableControllerCards() const;
    SpatialControllerCardList* sceneControllerCards() const;

    Ui::OpenRGB3DSpatialTab*    ui = nullptr;
    ResourceManagerInterface*   resource_manager;

    LEDViewport3D*              viewport = nullptr;

    std::vector<std::unique_ptr<ControllerTransform>> controller_transforms;

    SpatialAvailableControllerBacking available_controllers_;
    SpatialSceneControllerBacking     scene_controllers_;
    bool                        minecraft_library_hub_active = false;
    SpatialEffect3D*            minecraft_hub_preview_effect = nullptr;

    QPushButton*                start_effect_button = nullptr;
    QPushButton*                stop_effect_button = nullptr;
    SpatialEffect3D*            current_effect_ui = nullptr;
    QTimer*                     auto_load_timer = nullptr;
    QTimer*                     effect_timer = nullptr;
    bool                        deferred_startup_done_ = false;
    bool                        first_load = true;
    bool                        effect_running = false;
    float                       effect_time = 0.0f;
    QElapsedTimer               effect_elapsed;
    bool                        stack_settings_updating = false;

    int                         custom_grid_x = 10;
    int                         custom_grid_y = 10;
    int                         custom_grid_z = 10;
    float                       grid_scale_mm = 10.0f;

    float                       manual_room_width = 1000.0f;
    float                       manual_room_depth = 1000.0f;
    float                       manual_room_height = 1000.0f;
    bool                        use_manual_room_size = false;

    float                       default_led_spacing_x_ = 10.0f;
    float                       default_led_spacing_y_ = 0.0f;
    float                       default_led_spacing_z_ = 0.0f;
    QHash<int, QVector3D>       available_rgb_spacing_draft_;

    std::vector<std::unique_ptr<VirtualController3D>> virtual_controllers;
    std::vector<std::string>                          virtual_controller_json_files;
    std::vector<std::unique_ptr<VirtualReferencePoint3D>> reference_points;
    std::unique_ptr<ZoneManager3D> zone_manager;

    int                         last_stack_selection_index = -1;
    QComboBox*                  stack_effect_blend_combo = nullptr;
    QWidget*                    stack_blend_container = nullptr;

    std::vector<std::unique_ptr<EffectInstance3D>> effect_stack;
    int next_effect_instance_id = 1;

    std::vector<std::unique_ptr<StackPreset3D>> stack_presets;

    void ComputeAutoRoomExtents(float& width_mm, float& depth_mm, float& height_mm) const;

    QWidget*        audio_eq_container = nullptr;
    QHBoxLayout*    audio_eq_row_layout = nullptr;
    std::vector<QSlider*> audio_eq_sliders;
    bool audio_eq_rebuilding = false;

    void rebuildAudioEqSliders(bool persist_settings = false);

private slots:
    void on_audio_device_changed(int index);
    void on_audio_gain_changed(int value);
    void on_audio_clarity_changed(int value);
    void on_audio_isolation_changed(int value);
    void on_audio_mix_preset_changed(int index);
    void sync_audio_eq_sliders_from_manager();
    void on_audio_start_clicked();
    void on_audio_stop_clicked();
    void on_audio_level_updated(float level);
    void on_audio_bands_changed(int index);
    void on_audio_fft_changed(int index);
    void on_audio_restore_defaults_clicked();
    void on_audio_eq_changed(int band_index);

    void on_display_plane_selected(int index);
    void on_display_planes_list_selection_changed(int row);
    void on_add_display_plane_clicked();
    void on_edit_display_plane_clicked();
    void on_remove_display_plane_clicked();
    void on_display_plane_position_signal(int index, float x, float y, float z);
    void on_display_plane_rotation_signal(int index, float x, float y, float z);

private:
    std::vector<RGBColor> room_grid_overlay_buffer;

    bool layout_dirty = false;

    int                       scene_object_edit_tab_index_ = -1;
    int                       settings_tab_before_edit_  = -1;
    int                       right_stack_before_edit_   = -1;

    int sceneObjectEditSceneRow() const;

    std::vector<std::unique_ptr<DisplayPlane3D>> display_planes;
    int             current_display_plane_index = -1;

    void UpdateDisplayPlanesList();
    void UpdateCurrentDisplayPlaneListItemLabel();
    void RefreshDisplayPlaneDetails();
    DisplayPlane3D* GetSelectedDisplayPlane();
    void SyncDisplayPlaneManager();
    void NotifyDisplayPlaneChanged();
    void SyncDisplayPlaneControls(DisplayPlane3D* plane);
    void SetDisplayPlaneVisibleInScene(DisplayPlane3D* plane, bool visible);
    bool EditReferencePointAtIndex(int ref_index);
    bool EditDisplayPlaneAtIndex(int plane_index);
    int  FindAvailableControllerRow(int type_code, int object_index) const;
    void SelectAvailableControllerEntry(int type_code, int object_index);
    bool AddCustomControllerToScene(int virtual_controller_index);

    static void OnOpenRgbDetectionEnded(void* context);
};

#endif

