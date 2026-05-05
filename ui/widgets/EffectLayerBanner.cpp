// SPDX-License-Identifier: GPL-2.0-only

#include "EffectLayerBanner.h"

#include "PluginUiUtils.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

EffectLayerBanner::EffectLayerBanner(bool include_start_stop, QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(0, 0, 0, 0);

    QLabel* layer_heading = new QLabel(QStringLiteral("Layer controls"));
    QFont hf = layer_heading->font();
    hf.setBold(true);
    layer_heading->setFont(hf);
    main_layout->addWidget(layer_heading);

    if(include_start_stop)
    {
        QHBoxLayout* button_layout = new QHBoxLayout();
        PluginUiAddEffectTransportButtons(button_layout, &start_effect_button_, &stop_effect_button_);
        main_layout->addLayout(button_layout);
    }
}
