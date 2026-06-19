// SPDX-License-Identifier: GPL-2.0-only

#include "EffectStackBlendRow.h"
#include "EffectInstance3D.h"
#include "ui_EffectStackBlendRow.h"

#include <QComboBox>

EffectStackBlendRow::EffectStackBlendRow(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::EffectStackBlendRow)
{
    ui->setupUi(this);
    populateBlendItems();
}

EffectStackBlendRow::~EffectStackBlendRow()
{
    delete ui;
}

QComboBox* EffectStackBlendRow::blendCombo() const
{
    return ui->blendCombo;
}

void EffectStackBlendRow::populateBlendItems()
{
    QComboBox* combo = ui->blendCombo;
    combo->setToolTip(QStringLiteral("How this effect combines with other layers."));
    combo->addItem(QStringLiteral("No Blend"), (int)BlendMode::NO_BLEND);
    combo->setItemData(0,
        QStringLiteral("Uses only this layer's color at this step (ignores the composite below it). "
                       "Earlier layers have no effect after a No Blend layer; order matters."),
        Qt::ToolTipRole);
    combo->addItem(QStringLiteral("Replace"), (int)BlendMode::REPLACE);
    combo->setItemData(1,
        QStringLiteral("Completely replaces colors from previous effects (last effect wins)"),
        Qt::ToolTipRole);
    combo->addItem(QStringLiteral("Add"), (int)BlendMode::ADD);
    combo->setItemData(2, QStringLiteral("Adds colors together (brightens)"), Qt::ToolTipRole);
    combo->addItem(QStringLiteral("Multiply"), (int)BlendMode::MULTIPLY);
    combo->setItemData(3, QStringLiteral("Multiplies colors (darkens)"), Qt::ToolTipRole);
    combo->addItem(QStringLiteral("Screen"), (int)BlendMode::SCREEN);
    combo->setItemData(4,
        QStringLiteral("Screen blend (brightens without overexposure)"),
        Qt::ToolTipRole);
    combo->addItem(QStringLiteral("Max"), (int)BlendMode::MAX);
    combo->setItemData(5,
        QStringLiteral("Takes the brightest channel from previous effects"),
        Qt::ToolTipRole);
    combo->addItem(QStringLiteral("Min"), (int)BlendMode::MIN);
    combo->setItemData(6,
        QStringLiteral("Takes the darkest channel from previous effects"),
        Qt::ToolTipRole);
}
