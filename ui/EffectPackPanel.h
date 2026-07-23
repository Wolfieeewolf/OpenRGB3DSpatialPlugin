// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EffectPacks/EffectPackPlayer.h"
#include "filesystem.h"
#include <QElapsedTimer>
#include <QGroupBox>

class QTimer;
class EffectPackEditorDialog;

namespace Ui {
class EffectPackPanel;
}

class OpenRGB3DSpatialTab;

class EffectPackPanel : public QGroupBox
{
    Q_OBJECT

public:
    explicit EffectPackPanel(QWidget* parent = nullptr);
    ~EffectPackPanel() override;

    void bindTab(OpenRGB3DSpatialTab* tab);
    void stopPreview();

private slots:
    void onRefresh();
    void onPreview();
    void onStop();
    void onNew();
    void onEdit();
    void onSeedExample();
    void onTick();
    void onEditorSaved(const QString& path);
    void onEditorPreviewStarted();

private:
    void setPlayingUi(bool playing);
    void populateList();
    void selectPathInList(const QString& path);
    filesystem::path packsDir() const;
    EffectPackEditorDialog* ensureEditor();
    QString pathFromItem(class QListWidgetItem* item) const;

    Ui::EffectPackPanel* ui = nullptr;
    OpenRGB3DSpatialTab* tab_ = nullptr;
    QTimer* timer_ = nullptr;
    EffectPackEditorDialog* editor_ = nullptr;
    EffectPack::Player player_;
    QElapsedTimer wall_;
    int last_elapsed_ms_ = 0;
};
