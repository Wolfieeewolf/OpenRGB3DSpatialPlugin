// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPATIALTOOLTIPPROXY_H
#define SPATIALTOOLTIPPROXY_H

#include <QProxyStyle>

class SpatialTooltipProxy : public QProxyStyle
{
public:
    using QProxyStyle::QProxyStyle;

    int styleHint(StyleHint hint,
                  const QStyleOption* option = nullptr,
                  const QWidget* widget = nullptr,
                  QStyleHintReturn* returnData = nullptr) const override
    {
        if(hint == QStyle::SH_ToolTip_WakeUpDelay)
        {
            return 0;
        }
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};

#endif
