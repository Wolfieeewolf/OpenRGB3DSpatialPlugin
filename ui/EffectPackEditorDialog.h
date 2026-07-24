// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPacks/EffectPack.h"
#include "EffectPacks/EffectPackPlayer.h"
#include "EffectPackTimelineWidget.h"
#include "filesystem.h"
#include <QDialog>
#include <QElapsedTimer>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTimer;
class QWidget;
class EffectPackGradientBar;
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
    void onDurationChanged(int value);
    void onPlayheadChanged(int ms);
    void onBlockSelected(int track_index, int block_index);
    void onEffectAddRequested(int row_index, int ms, int block_type);
    void onAddEffectFromPalette();
    void onRemoveBlock();
    void onBlockDeleteRequested(int track_index, int block_index);
    void onBlockFieldChanged();
    void onTypeChanged();
    void onPickColor();
    void onPickColorTo();
    void onGradientPreset();
    void onGradientStopsChanged();
    void onSave();
    void onPreview();
    void onTick();

private:
    void buildUi();
    void loadIntoUi(const EffectPack::Pack& pack);
    void applyMetaToPack();
    bool promptSelectControllers(std::vector<std::string>* devices, bool require_selection);
    EffectPackTimelineWidget::Node buildControllerNode(ControllerTransform* transform, int index) const;
    bool deviceSelectedForPack(const std::string& key) const;
    int ensureTrackForTarget(const EffectPack::Target& target, const QString& label);
    void applyBlockToForm();
    void applyFormToSelectedBlock();
    void updatePropVisibility();
    void syncGradientBar();
    void updateSelectionActions();
    void setColorButton(QPushButton* button, RGBColor color);
    RGBColor colorFromButton(QPushButton* button) const;
    QString sanitizeId(const QString& name) const;
    void setPlayingUi(bool playing);
    void addBlockAt(int row_index, int ms, EffectPack::BlockType type);
    int currentTimelineRow() const;
    EffectPack::Block* selectedBlock();

    OpenRGB3DSpatialTab* tab_ = nullptr;
    filesystem::path packs_dir_;
    filesystem::path pack_path_;
    EffectPack::Pack pack_;
    bool suppress_ui_ = false;

    QLineEdit* name_edit_ = nullptr;
    QSpinBox* duration_spin_ = nullptr;
    QComboBox* loop_combo_ = nullptr;
    QPushButton* controllers_button_ = nullptr;
    EffectPackTimelineWidget* timeline_ = nullptr;
    QLabel* status_label_ = nullptr;

    QComboBox* effect_palette_ = nullptr;
    QComboBox* type_combo_ = nullptr;
    QSpinBox* start_spin_ = nullptr;
    QSpinBox* end_spin_ = nullptr;
    QSpinBox* period_spin_ = nullptr;
    QSpinBox* intensity_spin_ = nullptr;
    QSpinBox* min_intensity_spin_ = nullptr;
    QDoubleSpinBox* speed_spin_ = nullptr;
    QSpinBox* pulse_length_spin_ = nullptr;
    QComboBox* direction_combo_ = nullptr;
    QPushButton* color_button_ = nullptr;
    QPushButton* color_to_button_ = nullptr;
    QPushButton* remove_block_button_ = nullptr;
    QComboBox* gradient_preset_ = nullptr;
    EffectPackGradientBar* gradient_bar_ = nullptr;
    QWidget* direction_section_ = nullptr;
    QWidget* speed_section_ = nullptr;
    QWidget* pulse_section_ = nullptr;
    QWidget* color_to_row_ = nullptr;
    QWidget* period_row_ = nullptr;
    QWidget* min_intensity_row_ = nullptr;

    QPushButton* preview_button_ = nullptr;
    QPushButton* stop_button_ = nullptr;
    QPushButton* save_button_ = nullptr;

    QTimer* timer_ = nullptr;
    EffectPack::Player player_;
    QElapsedTimer wall_;
    int last_elapsed_ms_ = 0;
    int selected_track_ = -1;
    int selected_block_ = -1;
};
