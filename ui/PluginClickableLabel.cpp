// SPDX-License-Identifier: GPL-2.0-only

#include "PluginClickableLabel.h"
#include <QMouseEvent>

PluginClickableLabel::PluginClickableLabel(QWidget* parent)
    : QLabel(parent)
{
}

void PluginClickableLabel::mousePressEvent(QMouseEvent* event)
{
    if(event && event->button() == Qt::LeftButton)
    {
        emit clicked();
    }
    QLabel::mousePressEvent(event);
}
