// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPacks/EffectPack.h"
#include "EffectPacks/EffectPackPlayer.h"
#include "OpenRGBPluginInterface.h"
#include "filesystem.h"
#include <QDialog>
#include <QElapsedTimer>

class QColor;
class QTimer;

namespace Ui {
class EffectPackEditorDialog;
}

class EffectPackEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EffectPackEditorDialog(OpenRGBPluginAPIInterface* rm, QWidget* parent = nullptr);
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
    void onAddSolid();
    void onAddFade();
    void onAddPulse();
    void onRemoveBlock();
    void onBlockSelected();
    void onBlockFieldChanged();
    void onPickColor();
    void onPickColorTo();
    void onSave();
    void onPreview();
    void onTick();

private:
    void loadIntoUi(const EffectPack::Pack& pack);
    EffectPack::Pack packFromUi() const;
    void refreshBlockList(int select_row = -1);
    void applyBlockToForm();
    void applyFormToSelectedBlock();
    void setColorButton(class QPushButton* button, RGBColor color);
    RGBColor colorFromButton(class QPushButton* button) const;
    QString sanitizeId(const QString& name) const;
    void setPlayingUi(bool playing);

    Ui::EffectPackEditorDialog* ui = nullptr;
    OpenRGBPluginAPIInterface* resource_manager = nullptr;
    filesystem::path packs_dir_;
    filesystem::path pack_path_;
    EffectPack::Pack pack_;
    bool suppress_block_ui_ = false;

    QTimer* timer_ = nullptr;
    EffectPack::Player player_;
    QElapsedTimer wall_;
    int last_elapsed_ms_ = 0;
};
