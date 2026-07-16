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
class EffectGlobalSettingsPanel;

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
    void startEffectClicked();
    void stopEffectClicked();

    void controllerSelected(int index);
    void viewportControllerSelected(int transform_index);
    void viewportDisplayPlaneSelected(int plane_index);
    void controllerPositionChanged(int index, float x, float y, float z);
    void controllerRotationChanged(int index, float x, float y, float z);

    void availableCardAdd(SpatialControllerEntryKey key,
                             int                            granularity,
                             int                            item_index,
                             float                          spacing_x_mm,
                             float                          spacing_y_mm,
                             float                          spacing_z_mm);
    void sceneCardRemove(int scene_list_row);
    void removeControllerClicked();
    void removeControllerFromViewport(int index);
    void clearAllClicked();
    void sceneControllerCardsSelectionChanged(int scene_list_row);
    void sceneCardEdit(int scene_list_row);

    void ShowSceneObjectEditPanel(int scene_list_row, bool sync_scene_selection = true);
    void HideSceneObjectEditPanel();
    void SyncSceneObjectSpacingPanel();
    void EditCustomControllerForCurrentSceneSelection();
    void EditReferencePointForCurrentSceneSelection();
    void EditDisplayPlaneForCurrentSceneSelection();
    void FocusObjectCreatorTab();

    void quickSaveLayoutClicked();
    void saveLayoutClicked();
    void loadLayoutClicked();
    void deleteLayoutClicked();
    void layoutProfileChanged(int index);
    void saveEffectProfileClicked();
    void loadEffectProfileClicked();
    void deleteEffectProfileClicked();
    void effectProfileChanged(int index);
    void openConfigFolderClicked();
    void createCustomControllerClicked();
    void importCustomControllerClicked();
    void exportCustomControllerClicked();
    void editCustomControllerClicked();
    void deleteCustomControllerClicked();
    void customControllerSelectionChanged(int row);
    void addRefPointClicked();
    void editReferencePointClicked();
    void removeRefPointClicked();
    void referencePointsListSelectionChanged(int row);
    void refPointSelected(int index, bool from_scene_controller_list = false);
    void refPointPositionChanged(int index, float x, float y, float z);

    void createZoneClicked();
    void editZoneClicked();
    void deleteZoneClicked();
    void zoneSelected(int index);

    void effectTimerTimeout();
    void RenderEffectStack();
    void gridDimensionsChanged();
    void gridSnapToggled(bool enabled);
    void roomGuideLabelsToggled(bool enabled);
    void frameSelectionInView();
    void resetViewportCamera();
    void effectChanged(int index);
    void effectOriginChanged(int index);
    void effectBoundsChanged(int index);
    void effectZoneChanged(int index);
    void UpdateSelectionInfo();
    void UpdateEffectOriginCombo();

    void effectLibraryCategoryChanged(int index);
    void effectLibraryGameChanged(int index);
    void effectLibrarySearchChanged(const QString& text);
    void effectLibraryAddClicked();
    void effectLibraryItemDoubleClicked(QListWidgetItem* item);
    void effectLibrarySelectionChanged(int row);

    void startAllEffectsClicked();
    void stopAllEffectsClicked();
    void UpdateStartStopAllButtons();
    void removeEffectFromStackClicked();
    void effectStackItemDoubleClicked(QListWidgetItem* item);
    void effectStackSelectionChanged(int index);
    void stackEffectTypeChanged(int index);
    void stackEffectZoneChanged(int index);
    void stackEffectBlendChanged(int index);

    void saveStackPresetClicked();
    void loadStackPresetClicked();
    void deleteStackPresetClicked();

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
    bool SaveCustomController(unsigned int index);
    int  IndexOfVirtualController(const VirtualController3D* controller) const;
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
    void SetupStackRoomOutputPanel();
    void SyncStackRoomOutputPanel();
    QString sceneLabelForTransformIndex(int transform_index) const;

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
    bool PlaybackUsesScreenMirror() const;
    void SyncScreenCaptureSession();
    void UpdateAudioPanelVisibility();
    bool IsAudioEffectClass(const std::string& class_name) const;
    void PopulateEffectLibraryCategories();
    void PopulateEffectLibraryGames();
    void UpdateEffectLibraryGameFilterVisibility();
    void PopulateEffectLibrary();
    void AddEffectInstanceToStack(const QString& class_name,
                                  const QString& ui_name,
                                  int zone_index = -1,
                                  BlendMode blend_mode = BlendMode::NO_BLEND,
                                  const nlohmann::json* preset_settings = nullptr,
                                  bool enabled = true);

    QListWidget* zonesList() const;
    QPushButton* editZoneButton() const;
    QPushButton* deleteZoneButton() const;
    QComboBox*   effectCategoryCombo() const;
    QComboBox*   effectLibraryGameCombo() const;
    QLineEdit*   effectLibrarySearch() const;
    QListWidget* effectLibraryList() const;
    QPushButton* effectLibraryAddButton() const;
    QListWidget* effectStackList() const;
    QListWidget* stackPresetsList() const;
    QPushButton* startAllEffectsButton() const;
    QPushButton* stopAllEffectsButton() const;
    QGroupBox*   effectConfigGroup() const;
    EffectGlobalSettingsPanel* effectGlobalSettingsPanel() const;
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

    QPushButton*                start_effect_button = nullptr;
    QPushButton*                stop_effect_button = nullptr;
    SpatialEffect3D*            current_effect_ui = nullptr;
    class EffectRoomOutputPanel* stack_room_output_panel_ = nullptr;
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
    void audioDeviceChanged(int index);
    void audioGainChanged(int value);
    void audioClarityChanged(int value);
    void audioIsolationChanged(int value);
    void audioMixPresetChanged(int index);
    void sync_audio_eq_sliders_from_manager();
    void audioStartClicked();
    void audioStopClicked();
    void audioLevelUpdated(float level);
    void audioBandsChanged(int index);
    void audioFftChanged(int index);
    void audioRestoreDefaultsClicked();
    void audioEqChanged(int band_index);

    void displayPlaneSelected(int index);
    void displayPlanesListSelectionChanged(int row);
    void addDisplayPlaneClicked();
    void editDisplayPlaneClicked();
    void removeDisplayPlaneClicked();
    void displayPlanePositionSignal(int index, float x, float y, float z);
    void displayPlaneRotationSignal(int index, float x, float y, float z);

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

