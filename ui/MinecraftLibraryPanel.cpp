// SPDX-License-Identifier: GPL-2.0-only

#include "MinecraftLibraryPanel.h"
#include "OpenRGB3DSpatialTab.h"
#include "Effects3D/Games/Minecraft/MinecraftEffectLibrary.h"
#include "PluginUiUtils.h"
#include "ui_MinecraftLibraryPanel.h"

#include <QComboBox>
#include <QPushButton>

MinecraftLibraryPanel::MinecraftLibraryPanel(QWidget* parent)
    : QGroupBox(parent)
    , ui(new Ui::MinecraftLibraryPanel)
{
    ui->setupUi(this);
    PluginUiApplyMutedSecondaryLabel(ui->introLabel->label());
}

MinecraftLibraryPanel::~MinecraftLibraryPanel()
{
    delete ui;
}

void MinecraftLibraryPanel::bindTab(OpenRGB3DSpatialTab* tab)
{
    if(!tab)
    {
        return;
    }

    ui->layerCombo->clear();
    for(const MinecraftEffectLibrary::Variant& var : MinecraftEffectLibrary::Variants())
    {
        ui->layerCombo->addItem(QString::fromUtf8(var.label), QString::fromUtf8(var.class_name));
    }
    {
        const int health_idx = ui->layerCombo->findData(QStringLiteral("MinecraftHealth"));
        const int pick       = health_idx >= 0 ? health_idx : (ui->layerCombo->count() > 1 ? 1 : 0);
        ui->layerCombo->setCurrentIndex(pick);
    }

    connect(ui->layerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), tab,
            &OpenRGB3DSpatialTab::on_minecraft_library_layer_combo_changed);
    connect(ui->addLayerButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::on_minecraft_library_add_clicked);

    setVisible(false);
}

QComboBox* MinecraftLibraryPanel::layerCombo() const { return ui->layerCombo; }
QWidget* MinecraftLibraryPanel::hubPreviewHolder() const { return ui->hubPreviewHolder; }
