// SPDX-License-Identifier: GPL-2.0-only

#include "EffectControlsHostPanel.h"

#include <QVBoxLayout>

EffectControlsHostPanel::EffectControlsHostPanel(QWidget* parent)
    : QWidget(parent)
    , content_layout_(new QVBoxLayout(this))
{
    content_layout_->setContentsMargins(0, 0, 0, 0);
    content_layout_->setSpacing(6);
    setVisible(false);
}

EffectControlsHostPanel::~EffectControlsHostPanel() = default;

void EffectControlsHostPanel::bindTab(OpenRGB3DSpatialTab* tab)
{
    if(!tab)
    {
        return;
    }

}

QVBoxLayout* EffectControlsHostPanel::contentLayout() const
{
    return content_layout_;
}
