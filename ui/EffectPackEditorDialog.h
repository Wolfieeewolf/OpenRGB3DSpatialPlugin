// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPacks/EffectPack.h"
#include "EffectPacks/EffectPackPlayer.h"
#include "EffectPackTimelineWidget.h"
#include "filesystem.h"
#include <QDialog>
#include <QElapsedTimer>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;
class QTimer;
class QToolButton;
class QWidget;
class EffectPackCurveBar;
class EffectPackGradientBar;
class EffectPackToolBar;
class OpenRGB3DSpatialTab;
struct ControllerTransform;

class EffectPackEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EffectPackEditorDialog(OpenRGB3DSpatialTab* tab, QWidget* parent = nullptr);
    ~EffectPackEditorDialog() override;

    void NewPack(const filesystem::path& packs_dir);
    void EditPack(const filesystem::path& path);

signals:
    void packSaved(const QString& path);
    void previewStarted();
    void previewStopped();

public slots:
    void stopPreview();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onRebuildTimelineModel();
    void onPickControllers();
    void onMapControllers();
    void onDurationChanged(int value);
    void onPlayheadChanged(int ms);
    void onBlockSelected(int track_index, int block_index);
    void onEffectAddRequested(int row_index, int ms, const QString& effect_id);
    void onToolbarEffectClicked(const QString& effect_id);
    void onToolbarColorClicked(unsigned int rgb);
    void onToolbarGradientClicked(const QString& preset_id);
    void onGradientPresetApplied(int track_index, int block_index, const QString& preset_id);
    void onCurvePresetApplied(int track_index, int block_index, const QString& preset_id);
    void onToolbarCurveClicked(const QString& preset_id);
    void onColorDropped(int row_index, int ms, unsigned int rgb);
    void onGradientDropped(int row_index, int ms, const QString& preset_id);
    void onRemoveBlock();
    void onBlockDeleteRequested(int track_index, int block_index);
    void onSceneZoneControllersReordered(const QString& scene_zone_name, const QVector<int>& controller_indices);
    void onBlockFieldChanged();
    void onTypeChanged();
    void onPickColor();
    void onPickColorTo();
    void onGradientPreset();
    void onGradientStopsChanged();
    void onAddColorStop();
    void onRemoveColorStop();
    void onCurvePointsChanged();
    void onCurvePresetChanged();
    void onSaveUserCurve();
    void onPeriodCurvePointsChanged();
    void onPeriodCurvePresetChanged();
    void onBrowseMediaPath();
    void onFlipDirection();
    void onSaveUserGradient();
    void onDeleteUserGradient();
    void onOverwriteGradientPreset(const QString& preset_id);
    void onDeleteGradientPreset(const QString& preset_id);
    void onResetGradientPreset(const QString& preset_id);
    void onSave();
    void onPreview();
    void onPreviewPack(int start_ms);
    void onPreviewRow(int row_index, int start_ms);
    void onPreviewTrack(int track_index, int start_ms);
    void onTick();

private:
    void buildUi();
    void loadIntoUi(const EffectPack::Pack& pack);
    void applyMetaToPack();
    bool promptSelectControllers(std::vector<std::string>* devices, bool require_selection);
    bool promptMapControllers();
    std::vector<std::string> packDeviceNames() const;
    bool deviceNameInScene(const std::string& name) const;
    EffectPackTimelineWidget::Node buildControllerNode(ControllerTransform* transform, int index) const;
    bool deviceSelectedForPack(const std::string& key) const;
    int ensureTrackForTarget(const EffectPack::Target& target, const QString& label);
    void applyBlockToForm();
    void applyFormToSelectedBlock();
    void updatePropVisibility();
    void syncGradientBar();
    void syncCurveBar();
    void syncPeriodCurveBar();
    void refillCurvePresets();
    void refillPeriodCurvePresets();
    void updateSelectionActions();
    void setColorButton(QPushButton* button, RGBColor color);
    RGBColor colorFromButton(QPushButton* button) const;
    QString sanitizeId(const QString& name) const;
    void setPlayingUi(bool playing);
    void addBlockAt(int row_index, int ms, const QString& effect_id = QString());
    int currentTimelineRow() const;
    EffectPack::Block* selectedBlock();
    void applyGradientPresetToBlock(EffectPack::Block* block, const QString& preset_id);
    void refillGradientPresets();
    void syncTimeSliderRanges();
    EffectPack::Pack soloPackForRow(int row_index) const;
    EffectPack::Pack soloPackForTrack(int track_index) const;
    void startPreview(const EffectPack::Pack& play_pack, int start_ms, const QString& status, bool solo);

    OpenRGB3DSpatialTab* tab_ = nullptr;
    filesystem::path packs_dir_;
    filesystem::path pack_path_;
    EffectPack::Pack pack_;
    bool suppress_ui_ = false;

    QLineEdit* name_edit_ = nullptr;
    QSpinBox* duration_spin_ = nullptr;
    QComboBox* loop_combo_ = nullptr;
    QPushButton* controllers_button_ = nullptr;
    QPushButton* map_controllers_button_ = nullptr;
    EffectPackToolBar* effect_toolbar_ = nullptr;
    EffectPackTimelineWidget* timeline_ = nullptr;
    QLabel* status_label_ = nullptr;

    QComboBox* type_combo_ = nullptr;
    QSpinBox* start_spin_ = nullptr;
    QSpinBox* end_spin_ = nullptr;
    QSlider* start_slider_ = nullptr;
    QSlider* end_slider_ = nullptr;
    QSpinBox* period_spin_ = nullptr;
    QSpinBox* intensity_spin_ = nullptr;
    QSpinBox* min_intensity_spin_ = nullptr;
    QSpinBox* max_intensity_spin_ = nullptr;
    QDoubleSpinBox* speed_spin_ = nullptr;
    QSpinBox* pulse_length_spin_ = nullptr;
    QComboBox* direction_combo_ = nullptr;
    QComboBox* axis_space_combo_ = nullptr;
    QComboBox* axis_mode_combo_ = nullptr;
    QDoubleSpinBox* axis_yaw_spin_ = nullptr;
    QDoubleSpinBox* axis_pitch_spin_ = nullptr;
    QComboBox* curve_combo_ = nullptr;
    QPushButton* save_curve_button_ = nullptr;
    EffectPackCurveBar* curve_bar_ = nullptr;
    QComboBox* period_curve_combo_ = nullptr;
    EffectPackCurveBar* period_curve_bar_ = nullptr;
    QCheckBox* reverse_check_ = nullptr;
    QCheckBox* flip_h_check_ = nullptr;
    QCheckBox* flip_v_check_ = nullptr;
    QComboBox* rotate_combo_ = nullptr;
    QPushButton* flip_direction_button_ = nullptr;
    QPushButton* color_button_ = nullptr;
    QPushButton* color_to_button_ = nullptr;
    QPushButton* remove_block_button_ = nullptr;
    QComboBox* gradient_preset_ = nullptr;
    QPushButton* save_gradient_button_ = nullptr;
    QPushButton* delete_gradient_button_ = nullptr;
    QPushButton* add_color_button_ = nullptr;
    QPushButton* remove_color_button_ = nullptr;
    QLabel* props_hint_ = nullptr;
    EffectPackGradientBar* gradient_bar_ = nullptr;
    QWidget* direction_section_ = nullptr;
    QWidget* speed_section_ = nullptr;
    QWidget* pulse_section_ = nullptr;
    QWidget* color_to_row_ = nullptr;
    QWidget* period_row_ = nullptr;
    QWidget* speed_row_ = nullptr;
    QWidget* min_intensity_row_ = nullptr;
    QWidget* max_intensity_row_ = nullptr;
    QWidget* media_section_ = nullptr;
    QLineEdit* media_path_edit_ = nullptr;
    QPushButton* media_browse_button_ = nullptr;
    QLineEdit* media_text_edit_ = nullptr;
    QCheckBox* media_scroll_check_ = nullptr;

    QPushButton* preview_button_ = nullptr;
    QToolButton* preview_menu_button_ = nullptr;
    QPushButton* stop_button_ = nullptr;
    QPushButton* save_button_ = nullptr;

    QTimer* timer_ = nullptr;
    EffectPack::Player player_;
    EffectPack::Pack preview_pack_;
    bool preview_solo_ = false;
    QElapsedTimer wall_;
    int last_elapsed_ms_ = 0;
    int selected_track_ = -1;
    int selected_block_ = -1;
};
