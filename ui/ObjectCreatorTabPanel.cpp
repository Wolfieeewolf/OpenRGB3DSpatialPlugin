// SPDX-License-Identifier: GPL-2.0-only



#include "ObjectCreatorTabPanel.h"

#include "OpenRGB3DSpatialTab.h"

#include "PluginUiUtils.h"

#include "ui_ObjectCreatorTabPanel.h"



#include <QFont>

#include <QListWidget>

#include <QPushButton>

#include <QString>

#include <Qt>



ObjectCreatorTabPanel::ObjectCreatorTabPanel(QWidget* parent)

    : QWidget(parent)

    , ui(new Ui::ObjectCreatorTabPanel)

{

    ui->setupUi(this);

    ui->contentStack->setCurrentWidget(ui->emptyPage);

    applyVisualStyles();

}



ObjectCreatorTabPanel::~ObjectCreatorTabPanel()

{

    delete ui;

}



void ObjectCreatorTabPanel::applyVisualStyles()

{

    QFont bold = ui->objectTypeLabel->font();

    bold.setBold(true);

    ui->objectTypeLabel->setFont(bold);



    ui->customListLabel->setFont(bold);

    ui->refListLabel->setFont(bold);

    ui->displayListLabel->setFont(bold);



    ui->emptyHelpLabel->setText(QStringLiteral(

        "Choose Custom Controller, Reference Point, or Display Plane above to create objects and add them to the 3D view."));

    PluginUiApplyMutedSecondaryLabel(ui->emptyHelpLabel->label());

    QFont empty_font = ui->emptyHelpLabel->label()->font();

    empty_font.setItalic(true);

    ui->emptyHelpLabel->label()->setFont(empty_font);



    PluginUiApplyMutedSecondaryLabel(ui->customSubtitleLabel->label());

    PluginUiApplyItalicSecondaryLabel(ui->customControllersEmptyLabel->label());

    ui->customControllersEmptyLabel->label()->setAlignment(Qt::AlignHCenter);



    PluginUiApplyMutedSecondaryLabel(ui->refSubtitleLabel->label());

    PluginUiApplyItalicSecondaryLabel(ui->refPointsEmptyLabel->label());

    ui->refPointsEmptyLabel->label()->setAlignment(Qt::AlignHCenter);



    PluginUiApplyMutedSecondaryLabel(ui->displaySubtitleLabel->label());

    PluginUiApplyItalicSecondaryLabel(ui->displayPlanesEmptyLabel->label());

    ui->displayPlanesEmptyLabel->label()->setAlignment(Qt::AlignHCenter);

    PluginUiApplyMutedSecondaryLabel(ui->refHelpLabel->label());

}



void ObjectCreatorTabPanel::bindTab(OpenRGB3DSpatialTab* tab)

{

    if(!tab)

    {

        return;

    }



    host_tab_ = tab;



    ui->importCustomControllerButton->setToolTip(

        tr("Copy portable preset .json files into the controllers folder (see OpenRGB3DSpatialPresets on GitHub). "

           "Use controller_name = OpenRGB device name and controller_location \"1:1\"."));

    ui->exportCustomControllerButton->setToolTip(

        tr("Export selected layout as a portable preset JSON (OpenRGB device names, location \"1:1\", "

           "brand/model) for sharing or the presets repo"));

    ui->editCustomControllerButton->setToolTip("Edit selected custom controller");

    ui->deleteCustomControllerButton->setToolTip(

        "Remove selected custom controller from library (remove from 3D scene first if in use)");

    ui->editReferencePointButton->setToolTip("Edit selected reference point");

    ui->removeRefPointButton->setToolTip("Delete selected reference point");

    ui->editDisplayPlaneButton->setToolTip("Edit selected display plane");

    ui->removeDisplayPlaneButton->setToolTip("Delete selected display plane");



    connect(ui->customControllersList, &QListWidget::currentRowChanged, tab,

            &OpenRGB3DSpatialTab::customControllerSelectionChanged);

    connect(ui->createCustomControllerButton, &QPushButton::clicked, tab,

            &OpenRGB3DSpatialTab::createCustomControllerClicked);

    connect(ui->importCustomControllerButton, &QPushButton::clicked, tab,

            &OpenRGB3DSpatialTab::importCustomControllerClicked);

    connect(ui->exportCustomControllerButton, &QPushButton::clicked, tab,

            &OpenRGB3DSpatialTab::exportCustomControllerClicked);

    connect(ui->editCustomControllerButton, &QPushButton::clicked, tab,

            &OpenRGB3DSpatialTab::editCustomControllerClicked);

    connect(ui->deleteCustomControllerButton, &QPushButton::clicked, tab,

            &OpenRGB3DSpatialTab::deleteCustomControllerClicked);



    connect(ui->referencePointsList, &QListWidget::currentRowChanged, this,

            &ObjectCreatorTabPanel::onReferencePointListRowChanged);

    connect(ui->referencePointsList, &QListWidget::currentRowChanged, tab,

            &OpenRGB3DSpatialTab::referencePointsListSelectionChanged);

    connect(ui->createReferencePointButton, &QPushButton::clicked, tab,

            &OpenRGB3DSpatialTab::addRefPointClicked);

    connect(ui->editReferencePointButton, &QPushButton::clicked, tab,

            &OpenRGB3DSpatialTab::editReferencePointClicked);

    connect(ui->removeRefPointButton, &QPushButton::clicked, tab,

            &OpenRGB3DSpatialTab::removeRefPointClicked);



    connect(ui->displayPlanesList, &QListWidget::currentRowChanged, tab,

            &OpenRGB3DSpatialTab::displayPlaneSelected);

    connect(ui->displayPlanesList, &QListWidget::currentRowChanged, tab,

            &OpenRGB3DSpatialTab::displayPlanesListSelectionChanged);

    connect(ui->createDisplayPlaneButton, &QPushButton::clicked, tab,

            &OpenRGB3DSpatialTab::addDisplayPlaneClicked);

    connect(ui->editDisplayPlaneButton, &QPushButton::clicked, tab,

            &OpenRGB3DSpatialTab::editDisplayPlaneClicked);

    connect(ui->removeDisplayPlaneButton, &QPushButton::clicked, tab,

            &OpenRGB3DSpatialTab::removeDisplayPlaneClicked);



    connect(ui->objectTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,

            &ObjectCreatorTabPanel::onObjectTypeChanged);



    ui->objectTypeCombo->setCurrentIndex(0);

}



void ObjectCreatorTabPanel::onObjectTypeChanged(int index)

{

    if(!host_tab_)

    {

        return;

    }



    if(index <= 0)

    {

        ui->contentStack->setCurrentWidget(ui->emptyPage);

    }

    else if(index == 1)

    {

        ui->contentStack->setCurrentWidget(ui->customPage);

    }

    else if(index == 2)

    {

        ui->contentStack->setCurrentWidget(ui->referencePage);

    }

    else if(index == 3)

    {

        ui->contentStack->setCurrentWidget(ui->displayPage);

        host_tab_->UpdateDisplayPlanesList();

        host_tab_->RefreshDisplayPlaneDetails();

    }

}



void ObjectCreatorTabPanel::showCustomControllerSection()

{

    if(ui->objectTypeCombo->currentIndex() != 1)

    {

        ui->objectTypeCombo->setCurrentIndex(1);

    }

    else

    {

        onObjectTypeChanged(1);

    }

}



void ObjectCreatorTabPanel::showReferencePointSection()

{

    if(ui->objectTypeCombo->currentIndex() != 2)

    {

        ui->objectTypeCombo->setCurrentIndex(2);

    }

    else

    {

        onObjectTypeChanged(2);

    }

}



void ObjectCreatorTabPanel::showDisplayPlaneSection()

{

    if(ui->objectTypeCombo->currentIndex() != 3)

    {

        ui->objectTypeCombo->setCurrentIndex(3);

    }

    else

    {

        onObjectTypeChanged(3);

    }

}



void ObjectCreatorTabPanel::onReferencePointListRowChanged(int list_row)

{

    if(!host_tab_)

    {

        return;

    }

    host_tab_->refPointSelected(host_tab_->ReferencePointIndexFromListRow(list_row));

}



QLabel* ObjectCreatorTabPanel::statusLabel() const { return ui->statusLabel; }

QListWidget* ObjectCreatorTabPanel::customControllersList() const { return ui->customControllersList; }

QWidget* ObjectCreatorTabPanel::customControllersEmptyLabel() const { return ui->customControllersEmptyLabel; }

QPushButton* ObjectCreatorTabPanel::exportCustomControllerButton() const { return ui->exportCustomControllerButton; }

QPushButton* ObjectCreatorTabPanel::editCustomControllerButton() const { return ui->editCustomControllerButton; }

QPushButton* ObjectCreatorTabPanel::deleteCustomControllerButton() const { return ui->deleteCustomControllerButton; }

QListWidget* ObjectCreatorTabPanel::referencePointsList() const { return ui->referencePointsList; }

QWidget* ObjectCreatorTabPanel::refPointsEmptyLabel() const { return ui->refPointsEmptyLabel; }

QPushButton* ObjectCreatorTabPanel::createReferencePointButton() const { return ui->createReferencePointButton; }

QPushButton* ObjectCreatorTabPanel::editReferencePointButton() const { return ui->editReferencePointButton; }

QPushButton* ObjectCreatorTabPanel::removeRefPointButton() const { return ui->removeRefPointButton; }

QListWidget* ObjectCreatorTabPanel::displayPlanesList() const { return ui->displayPlanesList; }

QWidget* ObjectCreatorTabPanel::displayPlanesEmptyLabel() const { return ui->displayPlanesEmptyLabel; }

QPushButton* ObjectCreatorTabPanel::createDisplayPlaneButton() const { return ui->createDisplayPlaneButton; }

QPushButton* ObjectCreatorTabPanel::editDisplayPlaneButton() const { return ui->editDisplayPlaneButton; }

QPushButton* ObjectCreatorTabPanel::removeDisplayPlaneButton() const { return ui->removeDisplayPlaneButton; }

