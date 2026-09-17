// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "EventBindings/BindingRuntime.h"
#include "EventBindings/EventSourceRegistry.h"
#include <QGroupBox>

class QTimer;

namespace Ui {
class EventBindingsPanel;
}

class OpenRGB3DSpatialTab;

class EventBindingsPanel : public QGroupBox
{
    Q_OBJECT

public:
    explicit EventBindingsPanel(QWidget* parent = nullptr);
    ~EventBindingsPanel() override;

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
    bool editBindingDialog(EffectBinding::Binding* binding);
    filesystem::path bindingsPath() const;
    filesystem::path packsDir() const;

    Ui::EventBindingsPanel* ui = nullptr;
    OpenRGB3DSpatialTab* tab_ = nullptr;
    QTimer* timer_ = nullptr;
    EffectBinding::EventSourceRegistry registry_;
    EffectBinding::BindingRuntime runtime_;
    bool bound_ = false;
};
