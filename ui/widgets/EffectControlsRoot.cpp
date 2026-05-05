// SPDX-License-Identifier: GPL-2.0-only

#include "EffectControlsRoot.h"

#include <QVBoxLayout>

EffectControlsRoot::EffectControlsRoot(QWidget* parent)
    : QWidget(parent)
{
    main_layout_ = new QVBoxLayout(this);
}
