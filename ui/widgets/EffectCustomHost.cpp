// SPDX-License-Identifier: GPL-2.0-only

#include "EffectCustomHost.h"

#include <QVBoxLayout>

EffectCustomHost::EffectCustomHost(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* custom_host_layout = new QVBoxLayout(this);
    custom_host_layout->setContentsMargins(0, 0, 0, 0);
    custom_host_layout->setSpacing(4);
}
