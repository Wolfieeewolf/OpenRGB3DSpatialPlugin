// SPDX-License-Identifier: GPL-2.0-only

#include "ControllerListPanel.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginUiUtils.h"
#include "SpatialControllerCardList.h"
#include "SpatialControllerCardWidget.h"
#include "ui_ControllerListPanel.h"

#include <QBoxLayout>
#include <QPushButton>
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

void ControllerListPanel::bindTab(OpenRGB3DSpatialTab* tab, Mode mode)
{
    if(!tab || card_list_)
    {
        return;
    }

    if(mode == Mode::Available)
    {
        setTitle(QString());
        setFlat(true);
        ui->helpLabel->setText(QStringLiteral(
            "Each device has a + button to add it to the 3D scene. Set Device / Zone / LED and X/Y/Z spacing on the card before adding."));
        ui->cardListHolder->setMinimumHeight(200);
        ui->bottomRowHost->setVisible(true);
    }
    else
    {
        setTitle(QStringLiteral("Controllers in 3D Scene"));
        setFlat(false);
        ui->helpLabel->setText(QStringLiteral(
            "Press − to remove from the scene. Adjust X/Y/Z spacing on a card to update LEDs in the 3D view."));
        ui->cardListHolder->setMinimumHeight(280);
        ui->bottomRowHost->setVisible(false);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
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
    if(mode == Mode::InScene)
    {
        card_list_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    }
    holder_lay->addWidget(card_list_, 1);

    if(mode == Mode::Available)
    {
        connect(ui->clearAllFromSceneButton, &QPushButton::clicked, tab, &OpenRGB3DSpatialTab::clearAllClicked);
    }
    else
    {
        connect(card_list_, &SpatialControllerCardList::sceneSelectionChanged, tab,
                &OpenRGB3DSpatialTab::sceneControllerCardsSelectionChanged);
    }
}
