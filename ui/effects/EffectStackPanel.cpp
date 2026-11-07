// SPDX-License-Identifier: GPL-2.0-only

#include "EffectStackPanel.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginUiUtils.h"
#include "ui_EffectStackPanel.h"

#include <QFont>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSizePolicy>

EffectStackPanel::EffectStackPanel(QWidget* parent)
    : QGroupBox(parent)
    , ui(new Ui::EffectStackPanel)
{
    ui->setupUi(this);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    /* Let the layer list grow with the Run column — no low maxHeight that leaves dead space. */
    PluginUiConfigureListInOuterScroll(ui->stackList, 160, 0);
    PluginUiConfigureListInOuterScroll(ui->presetsList, 160, 0);
    if(ui->stackList)
    {
        ui->stackList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        ui->stackList->setMaximumHeight(QWIDGETSIZE_MAX);
    }
    if(ui->presetsList)
    {
        ui->presetsList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        ui->presetsList->setMaximumHeight(QWIDGETSIZE_MAX);
    }
    if(ui->activeStackLayout)
    {
        ui->activeStackLayout->setStretch(ui->activeStackLayout->indexOf(ui->stackList), 1);
    }
    if(ui->presetsLayout)
    {
        ui->presetsLayout->setStretch(ui->presetsLayout->indexOf(ui->presetsList), 1);
    }
    if(ui->activeBottomSpacer)
    {
        ui->activeBottomSpacer->changeSize(0, 0, QSizePolicy::Ignored, QSizePolicy::Minimum);
    }
    if(ui->presetsBottomSpacer)
    {
        ui->presetsBottomSpacer->changeSize(0, 0, QSizePolicy::Ignored, QSizePolicy::Minimum);
    }
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
            &OpenRGB3DSpatialTab::effectStackSelectionChanged);
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
                    tab->effectStackSelectionChanged(row);
                }
            });
    connect(ui->stackList, &QListWidget::itemDoubleClicked, tab,
            &OpenRGB3DSpatialTab::effectStackItemDoubleClicked);

    connect(ui->startAllButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::startAllEffectsClicked);
    connect(ui->stopAllButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::stopAllEffectsClicked);
    connect(ui->removeLayerButton, &QPushButton::clicked, tab,
            &OpenRGB3DSpatialTab::removeEffectFromStackClicked);

    connect(ui->savePresetButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::saveStackPresetClicked);
    connect(ui->loadPresetButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::loadStackPresetClicked);
    connect(ui->deletePresetButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::deleteStackPresetClicked);

    PluginUiWireListDestructiveDelete(
        ui->stackList,
        ui->removeLayerButton,
        QStringLiteral("Remove layer"),
        QStringLiteral("Remove selected effect layer"),
        QStringLiteral("Remove the selected layer from the stack."));
    PluginUiWireListDestructiveDelete(
        ui->presetsList,
        ui->deletePresetButton,
        QStringLiteral("Delete preset"),
        QStringLiteral("Delete selected stack preset"),
        QStringLiteral("Delete the selected stack preset file."));

    tab->UpdateStackEffectZoneCombo();
}

QListWidget* EffectStackPanel::stackList() const { return ui->stackList; }
QListWidget* EffectStackPanel::presetsList() const { return ui->presetsList; }
QPushButton* EffectStackPanel::startAllButton() const { return ui->startAllButton; }
QPushButton* EffectStackPanel::stopAllButton() const { return ui->stopAllButton; }
