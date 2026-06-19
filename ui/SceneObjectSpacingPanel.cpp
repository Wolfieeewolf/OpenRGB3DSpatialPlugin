// SPDX-License-Identifier: GPL-2.0-only

#include "SceneObjectSpacingPanel.h"
#include "OpenRGB3DSpatialTab.h"
#include "ui_SceneObjectSpacingPanel.h"

#include <QDoubleSpinBox>
#include <QSignalBlocker>

SceneObjectSpacingPanel::SceneObjectSpacingPanel(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::SceneObjectSpacingPanel)
{
    ui->setupUi(this);

    const auto style_spin = [](QDoubleSpinBox* spin) {
        if(!spin)
        {
            return;
        }
        spin->setRange(0.0, 1000.0);
        spin->setDecimals(1);
        spin->setSingleStep(0.1);
    };
    style_spin(ui->spacingXSpin);
    style_spin(ui->spacingYSpin);
    style_spin(ui->spacingZSpin);

    ui->spacingGroup->setToolTip(
        tr("Distance between LEDs along each axis for the selected scene controller."));
}

SceneObjectSpacingPanel::~SceneObjectSpacingPanel()
{
    delete ui;
}

void SceneObjectSpacingPanel::bindTab(OpenRGB3DSpatialTab* tab)
{
    tab_ = tab;

    auto connect_spin = [this](QDoubleSpinBox* spin) {
        if(!spin)
        {
            return;
        }
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this](double) { onSpacingChanged(); });
    };
    connect_spin(ui->spacingXSpin);
    connect_spin(ui->spacingYSpin);
    connect_spin(ui->spacingZSpin);
}

void SceneObjectSpacingPanel::setTransformIndex(int transform_index)
{
    transform_index_ = transform_index;
}

void SceneObjectSpacingPanel::setSpacingMm(float x_mm, float y_mm, float z_mm)
{
    const auto set_spin = [](QDoubleSpinBox* spin, float value) {
        if(!spin)
        {
            return;
        }
        QSignalBlocker blocker(spin);
        spin->setValue((double)value);
    };
    set_spin(ui->spacingXSpin, x_mm);
    set_spin(ui->spacingYSpin, y_mm);
    set_spin(ui->spacingZSpin, z_mm);
}

void SceneObjectSpacingPanel::setSpacingEnabled(bool enabled)
{
    ui->spacingGroup->setEnabled(enabled);
    setVisible(enabled);
}

void SceneObjectSpacingPanel::onSpacingChanged()
{
    if(!tab_ || transform_index_ < 0)
    {
        return;
    }

    tab_->ApplyLedSpacingToTransform(transform_index_,
                                     (float)ui->spacingXSpin->value(),
                                     (float)ui->spacingYSpin->value(),
                                     (float)ui->spacingZSpin->value());
}
