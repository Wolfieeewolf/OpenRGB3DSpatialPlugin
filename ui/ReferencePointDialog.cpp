// SPDX-License-Identifier: GPL-2.0-only

#include "ReferencePointDialog.h"
#include "PluginUiUtils.h"
#include "RGBController.h"
#include "VirtualReferencePoint3D.h"
#include "ui_ReferencePointDialog.h"

#include <QColorDialog>

ReferencePointDialog::ReferencePointDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::ReferencePointDialog)
{
    ui->setupUi(this);

    PluginUiApplyMutedSecondaryLabel(ui->helpLabel->label());

    const std::vector<std::string> type_names = VirtualReferencePoint3D::GetTypeNames();
    for(const std::string& type_name : type_names)
    {
        ui->typeCombo->addItem(QString::fromStdString(type_name));
    }

    connect(ui->colorButton, &QPushButton::clicked, this, &ReferencePointDialog::onColorButtonClicked);
}

ReferencePointDialog::~ReferencePointDialog()
{
    delete ui;
}

void ReferencePointDialog::setCreateMode()
{
    setWindowTitle(tr("Create Reference Point"));
}

void ReferencePointDialog::setEditMode()
{
    setWindowTitle(tr("Edit Reference Point"));
}

void ReferencePointDialog::setDefaultColor(unsigned int rgb)
{
    color_rgb_ = rgb;
    PluginUiSetRgbSwatchButton(ui->colorButton,
                               (int)(rgb & 0xFF),
                               (int)((rgb >> 8) & 0xFF),
                               (int)((rgb >> 16) & 0xFF));
}

void ReferencePointDialog::loadFrom(const VirtualReferencePoint3D& point)
{
    ui->nameEdit->setText(QString::fromStdString(point.GetName()));
    const int type_index = static_cast<int>(point.GetType());
    if(type_index >= 0 && type_index < ui->typeCombo->count())
    {
        ui->typeCombo->setCurrentIndex(type_index);
    }

    const RGBColor c = point.GetDisplayColor();
    color_rgb_ = static_cast<unsigned int>(c);
    PluginUiSetRgbSwatchButton(ui->colorButton,
                               RGBGetRValue(c),
                               RGBGetGValue(c),
                               RGBGetBValue(c));
}

QString ReferencePointDialog::name() const
{
    return ui->nameEdit->text().trimmed();
}

ReferencePointType ReferencePointDialog::type() const
{
    const int index = ui->typeCombo->currentIndex();
    if(index < 0)
    {
        return REF_POINT_USER;
    }
    return static_cast<ReferencePointType>(index);
}

unsigned int ReferencePointDialog::displayColorRgb() const
{
    return color_rgb_;
}

void ReferencePointDialog::onColorButtonClicked()
{
    const QColor current((color_rgb_ & 0xFF),
                         (color_rgb_ >> 8) & 0xFF,
                         (color_rgb_ >> 16) & 0xFF);
    const QColor chosen = QColorDialog::getColor(current, this, tr("Reference Point Color"));
    if(!chosen.isValid())
    {
        return;
    }

    color_rgb_ = (static_cast<unsigned int>(chosen.blue()) << 16) | (static_cast<unsigned int>(chosen.green()) << 8)
                 | static_cast<unsigned int>(chosen.red());
    PluginUiSetRgbSwatchButton(ui->colorButton, chosen.red(), chosen.green(), chosen.blue());
}
