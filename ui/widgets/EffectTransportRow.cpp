// SPDX-License-Identifier: GPL-2.0-only

#include "EffectTransportRow.h"
#include "ui_EffectTransportRow.h"

#include <QPushButton>

EffectTransportRow::EffectTransportRow(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::EffectTransportRow)
{
    ui->setupUi(this);
}

EffectTransportRow::~EffectTransportRow()
{
    delete ui;
}

QPushButton* EffectTransportRow::startEffectButton() const
{
    return ui->startEffectButton;
}

QPushButton* EffectTransportRow::stopEffectButton() const
{
    return ui->stopEffectButton;
}
