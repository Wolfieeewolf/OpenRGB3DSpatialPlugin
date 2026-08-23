// SPDX-License-Identifier: GPL-2.0-only

#include "DisplayPlaneDialog.h"
#include "DisplayPlaneManager.h"
#include "OpenRGB3DSpatialTab.h"
#include "PluginUiUtils.h"
#include "ui_DisplayPlaneDialog.h"

#include <QDesktopServices>
#include <QUrl>

DisplayPlaneDialog::DisplayPlaneDialog(OpenRGB3DSpatialTab* host_tab, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::DisplayPlaneDialog)
    , host_tab_(host_tab)
{
    ui->setupUi(this);

    PluginUiApplyMutedSecondaryLabel(ui->panelSizeHintLabel->label());

    connect(ui->refreshCaptureButton, &QPushButton::clicked, this, &DisplayPlaneDialog::onRefreshCaptureClicked);
    connect(ui->openSpecsButton, &QPushButton::clicked, this, &DisplayPlaneDialog::onOpenSpecsClicked);
}

DisplayPlaneDialog::~DisplayPlaneDialog()
{
    delete ui;
}

void DisplayPlaneDialog::setCreateMode()
{
    setWindowTitle(tr("Create Display Plane"));
}

void DisplayPlaneDialog::setEditMode()
{
    setWindowTitle(tr("Edit Display Plane"));
}

void DisplayPlaneDialog::setCreateDefaults(const QString& suggested_name, float width_mm, float height_mm)
{
    ui->nameEdit->setText(suggested_name);
    ui->widthSpin->setValue(width_mm);
    ui->heightSpin->setValue(height_mm);
    populateCaptureCombo("");
}

void DisplayPlaneDialog::loadFrom(const DisplayPlane3D& plane)
{
    ui->nameEdit->setText(QString::fromStdString(plane.GetName()));
    ui->widthSpin->setValue(plane.GetWidthMM());
    ui->heightSpin->setValue(plane.GetHeightMM());
    populateCaptureCombo(plane.GetCaptureSourceId());
}

QString DisplayPlaneDialog::name() const
{
    return ui->nameEdit->text().trimmed();
}

float DisplayPlaneDialog::widthMm() const
{
    return static_cast<float>(ui->widthSpin->value());
}

float DisplayPlaneDialog::heightMm() const
{
    return static_cast<float>(ui->heightSpin->value());
}

std::string DisplayPlaneDialog::captureSourceId() const
{
    const int index = ui->captureCombo->currentIndex();
    if(index < 0)
    {
        return {};
    }
    return ui->captureCombo->itemData(index).toString().toStdString();
}

void DisplayPlaneDialog::populateCaptureCombo(const std::string& prefer_source_id)
{
    if(!host_tab_)
    {
        return;
    }
    host_tab_->FillDisplayPlaneCaptureCombo(ui->captureCombo, prefer_source_id);
}

void DisplayPlaneDialog::onRefreshCaptureClicked()
{
    std::string current;
    const int index = ui->captureCombo->currentIndex();
    if(index >= 0)
    {
        current = ui->captureCombo->itemData(index).toString().toStdString();
    }
    populateCaptureCombo(current);
}

void DisplayPlaneDialog::onOpenSpecsClicked()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://www.displayspecifications.com/")));
}
