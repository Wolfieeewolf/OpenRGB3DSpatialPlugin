// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "Actions/ActionPlayer.h"
#include "filesystem.h"
#include <QElapsedTimer>
#include <QGroupBox>

class QTimer;
class ActionEditorDialog;

namespace Ui {
class ActionsPanel;
}

class OpenRGB3DSpatialTab;

class ActionsPanel : public QGroupBox
{
    Q_OBJECT

public:
    explicit ActionsPanel(QWidget* parent = nullptr);
    ~ActionsPanel() override;

    void bindTab(OpenRGB3DSpatialTab* tab);
    void stopPreview();

private slots:
    void onRefresh();
    void onPreview();
    void onStop();
    void onNew();
    void onEdit();
    void onImport();
    void onExport();
    void onTick();
    void onEditorSaved(const QString& path);
    void onEditorPreviewStarted();

private:
    void setPlayingUi(bool playing);
    void populateList();
    void selectPathInList(const QString& path);
    filesystem::path triggerEffectsDir() const;
    ActionEditorDialog* ensureEditor();
    QString pathFromItem(class QListWidgetItem* item) const;

    Ui::ActionsPanel* ui = nullptr;
    OpenRGB3DSpatialTab* tab_ = nullptr;
    QTimer* timer_ = nullptr;
    ActionEditorDialog* editor_ = nullptr;
    Action::Player player_;
    QElapsedTimer wall_;
    int last_elapsed_ms_ = 0;
    bool bound_ = false;
};
