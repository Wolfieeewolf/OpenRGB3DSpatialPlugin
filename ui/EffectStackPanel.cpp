// SPDX-License-Identifier: GPL-2.0-only

#include "EffectStackPanel.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginUiUtils.h"
#include "ui_EffectStackPanel.h"

#include <QFont>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>

EffectStackPanel::EffectStackPanel(QWidget* parent)
    : QGroupBox(parent)
    , ui(new Ui::EffectStackPanel)
{
    ui->setupUi(this);
}

EffectStackPanel::~EffectStackPanel()
{
    delete ui;
}

void EffectStackPanel::bindTab(OpenRGB3DSpatialTab* tab)
{
    if(!tab)
    {
        return;
    }

    QFont bold = ui->layersLabel->font();
    bold.setBold(true);
    ui->layersLabel->setFont(bold);
    ui->presetsLabel->setFont(bold);

    PluginUiApplyMutedSecondaryLabel(ui->layersHintLabel);

    connect(ui->stackList, &QListWidget::currentRowChanged, tab,
            &OpenRGB3DSpatialTab::on_effect_stack_selection_changed);
    connect(ui->stackList, &QListWidget::itemClicked, tab,
            [tab](QListWidgetItem*)
            {
                if(!tab->effectStackList())
                {
                    return;
                }
                const int row = tab->effectStackList()->currentRow();
                if(row >= 0 && row == tab->last_stack_selection_index)
                {
                    tab->on_effect_stack_selection_changed(row);
                }
            });
    connect(ui->stackList, &QListWidget::itemDoubleClicked, tab,
            &OpenRGB3DSpatialTab::on_effect_stack_item_double_clicked);

    connect(ui->startAllButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::on_start_all_effects_clicked);
    connect(ui->stopAllButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::on_stop_all_effects_clicked);
    connect(ui->removeLayerButton, &QPushButton::clicked, tab,
            &OpenRGB3DSpatialTab::on_remove_effect_from_stack_clicked);

    connect(ui->savePresetButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::on_save_stack_preset_clicked);
    connect(ui->loadPresetButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::on_load_stack_preset_clicked);
    connect(ui->deletePresetButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::on_delete_stack_preset_clicked);

    tab->UpdateStackEffectZoneCombo();
}

QListWidget* EffectStackPanel::stackList() const { return ui->stackList; }
QListWidget* EffectStackPanel::presetsList() const { return ui->presetsList; }
QPushButton* EffectStackPanel::startAllButton() const { return ui->startAllButton; }
QPushButton* EffectStackPanel::stopAllButton() const { return ui->stopAllButton; }
