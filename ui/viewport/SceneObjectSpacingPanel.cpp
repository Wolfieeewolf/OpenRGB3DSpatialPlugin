// SPDX-License-Identifier: GPL-2.0-or-later

#include "SceneObjectSpacingPanel.h"
#include "EffectSliderPromote.h"
#include "OpenRGB3DSpatialTab.h"
#include "ui_SceneObjectSpacingPanel.h"

#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <algorithm>

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
        spin->setSuffix(QStringLiteral(" mm"));
    };
    style_spin(ui->spacingXSpin);
    style_spin(ui->spacingYSpin);
    style_spin(ui->spacingZSpin);
    StackDoubleSpinOverThinSlider(ui->spacingXSpin);
    StackDoubleSpinOverThinSlider(ui->spacingYSpin);
    StackDoubleSpinOverThinSlider(ui->spacingZSpin);
    AlignGridCaptionColumn(ui->spacingLayout, 0, 48);

    ui->spacingGroup->setToolTip(
        tr("Distance between LEDs along each axis for the selected scene controller."));

    /* Light output of the device: Real light and Emitter + relay use it. */
    light_spin_ = new QDoubleSpinBox(this);
    light_spin_->setRange(0.0, 100000.0);
    light_spin_->setDecimals(0);
    light_spin_->setSingleStep(10.0);
    light_spin_->setSuffix(QStringLiteral(" lm"));
    light_spin_->setSpecialValueText(tr("Auto"));
    light_spin_->setToolTip(tr("Light output of the whole device at full white, in lumens (from the spec "
                               "sheet). Auto = a typical figure for this kind of device."));
    QLabel* light_label = new QLabel(tr("Light:"), this);
    light_label->setToolTip(light_spin_->toolTip());
    const int row = ui->spacingLayout->rowCount();
    ui->spacingLayout->addWidget(light_label, row, 0);
    ui->spacingLayout->addWidget(light_spin_, row, 1, 1, std::max(1, ui->spacingLayout->columnCount() - 1));
}

void SceneObjectSpacingPanel::setLightOutputLm(float lm)
{
    if(light_spin_)
    {
        QSignalBlocker blocker(light_spin_);
        light_spin_->setValue((double)std::max(0.0f, lm));
    }
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
    if(light_spin_)
    {
        connect(light_spin_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
            if(tab_ && transform_index_ >= 0)
            {
                tab_->SetTransformLightOutputLm(transform_index_, (float)v);
            }
        });
    }
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
