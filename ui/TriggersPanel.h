// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "Triggers/TriggerRuntime.h"
#include "Triggers/TriggerSourceRegistry.h"
#include <QGroupBox>
#include <vector>

class QListWidgetItem;
class QTimer;

namespace Ui {
class TriggersPanel;
}

class OpenRGB3DSpatialTab;

class TriggersPanel : public QGroupBox
{
    Q_OBJECT

public:
    explicit TriggersPanel(QWidget* parent = nullptr);
    ~TriggersPanel() override;

    void bindTab(OpenRGB3DSpatialTab* tab);
    void stopAll();

private slots:
    void onAdd();
    void onEdit();
    void onDelete();
    void onSelectionChanged();
    void onFire();
    void onStop();
    void onHoldToggled(bool checked);
    void onTick();
    void onItemChanged(class QListWidgetItem* item);

private:
    void reloadDocument();
    void saveDocument();
    void populateList();
    void setStatus(const QString& text);
    bool editBindingDialog(Trigger::Binding* binding);
    filesystem::path bindingsPath() const;
    filesystem::path triggerEffectsDir() const;
    filesystem::path triggersDir() const;
    void syncRuntime();
    bool catalogEnabled(const std::string& id) const;
    bool isCatalogItem(const QListWidgetItem* item) const;

    Ui::TriggersPanel* ui = nullptr;
    OpenRGB3DSpatialTab* tab_ = nullptr;
    QTimer* timer_ = nullptr;
    Trigger::TriggerSourceRegistry registry_;
    Trigger::TriggerRuntime runtime_;
    Trigger::Document user_;
    std::vector<Trigger::Binding> catalog_;
    bool bound_ = false;
};
