// SPDX-License-Identifier: GPL-2.0-only

#ifndef QTCOMPAT_H
#define QTCOMPAT_H

#include <QtGlobal>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    #define MOUSE_EVENT_X(event) ((event)->position().x())
    #define MOUSE_EVENT_Y(event) ((event)->position().y())
    #define MOUSE_EVENT_POS(event) ((event)->position().toPoint())
#else
    #define MOUSE_EVENT_X(event) ((event)->x())
    #define MOUSE_EVENT_Y(event) ((event)->y())
    #define MOUSE_EVENT_POS(event) ((event)->pos())
#endif

#endif // QTCOMPAT_H
