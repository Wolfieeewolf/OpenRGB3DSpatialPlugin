// SPDX-License-Identifier: GPL-2.0-only

#include "ZonesPanel.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginUiUtils.h"
#include "ui_ZonesPanel.h"

#include <QListWidget>
#include <QPushButton>

ZonesPanel::ZonesPanel(QWidget* parent)
    : QGroupBox(parent)
    , ui(new Ui::ZonesPanel)
{
    ui->setupUi(this);
}

ZonesPanel::~ZonesPanel()
{
    delete ui;
}

void ZonesPanel::bindTab(OpenRGB3DSpatialTab* tab)
{
    if(!tab)
    {
        return;
    }

    PluginUiApplyMutedSecondaryLabel(ui->helpLabel->label());

    connect(ui->zonesList, &QListWidget::currentRowChanged, tab, &OpenRGB3DSpatialTab::on_zone_selected);
    connect(ui->createZoneButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::on_create_zone_clicked);
    connect(ui->editZoneButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::on_edit_zone_clicked);
    connect(ui->deleteZoneButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::on_delete_zone_clicked);
}

QListWidget* ZonesPanel::zonesList() const { return ui->zonesList; }
QPushButton* ZonesPanel::createZoneButton() const { return ui->createZoneButton; }
QPushButton* ZonesPanel::editZoneButton() const { return ui->editZoneButton; }
QPushButton* ZonesPanel::deleteZoneButton() const { return ui->deleteZoneButton; }
