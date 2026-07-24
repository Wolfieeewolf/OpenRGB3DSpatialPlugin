// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPacks/EffectPack.h"
#include "EffectPacks/EffectPackPlayer.h"
#include "filesystem.h"
#include <QDialog>
#include <QElapsedTimer>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTimer;
class EffectPackTimelineWidget;
class OpenRGB3DSpatialTab;

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

private slots:
    void onRebuildTimelineModel();
    void onDurationChanged(int value);
    void onPlayheadChanged(int ms);
    void onBlockSelected(int track_index, int block_index);
    void onEmptyCellClicked(int row_index, int ms);
    void onAddSolid();
    void onAddFade();
    void onAddPulse();
    void onRemoveBlock();
    void onBlockFieldChanged();
    void onPickColor();
    void onPickColorTo();
    void onSave();
    void onPreview();
    void onTick();

private:
    void buildUi();
    void loadIntoUi(const EffectPack::Pack& pack);
    void applyMetaToPack();
    int ensureTrackForTarget(const EffectPack::Target& target, const QString& label);
    void applyBlockToForm();
    void applyFormToSelectedBlock();
    void setColorButton(QPushButton* button, RGBColor color);
    RGBColor colorFromButton(QPushButton* button) const;
    QString sanitizeId(const QString& name) const;
    void setPlayingUi(bool playing);
    void addBlockAt(int row_index, int ms, EffectPack::BlockType type);
    int currentTimelineRow() const;

    OpenRGB3DSpatialTab* tab_ = nullptr;
    filesystem::path packs_dir_;
    filesystem::path pack_path_;
    EffectPack::Pack pack_;
    bool suppress_ui_ = false;

    QLineEdit* name_edit_ = nullptr;
    QSpinBox* duration_spin_ = nullptr;
    QComboBox* loop_combo_ = nullptr;
    EffectPackTimelineWidget* timeline_ = nullptr;
    QLabel* status_label_ = nullptr;

    QSpinBox* start_spin_ = nullptr;
    QSpinBox* end_spin_ = nullptr;
    QSpinBox* period_spin_ = nullptr;
    QSpinBox* intensity_spin_ = nullptr;
    QPushButton* color_button_ = nullptr;
    QPushButton* color_to_button_ = nullptr;
    QLabel* color_to_label_ = nullptr;
    QLabel* period_label_ = nullptr;

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
