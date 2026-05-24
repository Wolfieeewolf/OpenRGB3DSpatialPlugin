// SPDX-License-Identifier: GPL-2.0-only

#ifndef CUSTOMCONTROLLERGRIDCELL_H
#define CUSTOMCONTROLLERGRIDCELL_H

#include <QColor>
#include <QString>

struct CustomControllerGridCellVisual
{
    QColor      fill;
    QColor      text;
    QString     label;
    QString     tooltip;
    bool        is_hole  = false;
    bool        is_empty = true;
};

#endif
