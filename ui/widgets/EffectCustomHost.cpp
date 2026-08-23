// SPDX-License-Identifier: GPL-2.0-only

#include "EffectCustomHost.h"

#include <QVBoxLayout>

EffectCustomHost::EffectCustomHost(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
}
