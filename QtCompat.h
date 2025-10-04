/*---------------------------------------------------------*\
| QtCompat.h                                                |
|                                                           |
|   Qt 5/6 compatibility macros                            |
|                                                           |
|   Date: 2025-10-05                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef QTCOMPAT_H
#define QTCOMPAT_H

#include <QtGlobal>

/*---------------------------------------------------------*\
| QMouseEvent position compatibility                       |
| Qt 5: x(), y()                                           |
| Qt 6: position().x(), position().y()                     |
\*---------------------------------------------------------*/
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    #define MOUSE_EVENT_X(event) ((event)->position().x())
    #define MOUSE_EVENT_Y(event) ((event)->position().y())
#else
    #define MOUSE_EVENT_X(event) ((event)->x())
    #define MOUSE_EVENT_Y(event) ((event)->y())
#endif

#endif // QTCOMPAT_H
