// SPDX-License-Identifier: GPL-2.0-only

#include "ControllerListPanel.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginUiUtils.h"
#include "SpatialControllerCardList.h"
#include "SpatialControllerCardWidget.h"
#include "ui_ControllerListPanel.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>

ControllerListPanel::ControllerListPanel(QWidget* parent)
    : QGroupBox(parent)
    , ui(new Ui::ControllerListPanel)
{
    ui->setupUi(this);
}

ControllerListPanel::~ControllerListPanel()
{
    delete ui;
}

bool ControllerListPanel::showUndetectedControllers() const
{
    return ui && ui->showUndetectedCheckBox && ui->showUndetectedCheckBox->isChecked();
}

void ControllerListPanel::setShowUndetectedControllers(bool checked)
{
    if(!ui || !ui->showUndetectedCheckBox)
    {
        return;
    }

    const QSignalBlocker block(ui->showUndetectedCheckBox);
    ui->showUndetectedCheckBox->setChecked(checked);
}

void ControllerListPanel::bindTab(OpenRGB3DSpatialTab* tab, Mode mode)
{
    if(!tab || card_list_)
    {
        return;
    }

    // Match Visual Map / OpenRGB: titled group boxes + expanding lists (not flat untitled panels).
    setFlat(false);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    if(mode == Mode::Available)
    {
        setTitle(QStringLiteral("Available Controllers"));
        ui->helpLabel->setText(QStringLiteral(
            "Each device has a + button to add it to the 3D scene. Set Device / Zone / LED on the card before adding. "
            "Only unassigned devices and remaining zones/LEDs are listed."));
        ui->cardListHolder->setMinimumHeight(160);
        ui->bottomRowHost->setVisible(true);
        if(ui->showUndetectedCheckBox)
        {
            ui->showUndetectedCheckBox->setVisible(true);
        }
    }
    else
    {
        setTitle(QStringLiteral("Controllers in 3D Scene"));
        ui->helpLabel->setText(QStringLiteral(
            "Press − to remove from the scene. Edit opens position, rotation, and spacing for the selected object."));
        ui->cardListHolder->setMinimumHeight(160);
        ui->bottomRowHost->setVisible(false);
        if(ui->showUndetectedCheckBox)
        {
            ui->showUndetectedCheckBox->setVisible(false);
        }
    }

    PluginUiApplyMutedSecondaryLabel(ui->helpLabel->label());

    QWidget* holder = ui->cardListHolder;
    QBoxLayout* holder_lay = qobject_cast<QBoxLayout*>(holder->layout());
    if(!holder_lay)
    {
        return;
    }

    const auto card_mode = (mode == Mode::Available) ? SpatialControllerCardWidget::Mode::Available
                                                     : SpatialControllerCardWidget::Mode::InScene;
    card_list_ = new SpatialControllerCardList(card_mode, holder);
    card_list_->setMinimumHeight(ui->cardListHolder->minimumHeight());
    card_list_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    holder_lay->addWidget(card_list_, 1);

    if(mode == Mode::Available)
    {
        connect(ui->clearAllFromSceneButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::clearAllClicked);
        if(ui->showUndetectedCheckBox)
        {
            connect(ui->showUndetectedCheckBox, &QCheckBox::toggled, tab, [tab](bool) {
                tab->UpdateAvailableControllersList();
                tab->SavePluginUiSettings();
            });
        }
    }
    else
    {
        connect(card_list_, &SpatialControllerCardList::sceneSelectionChanged, tab,
                &OpenRGB3DSpatialTab::sceneControllerCardsSelectionChanged);
    }
}
