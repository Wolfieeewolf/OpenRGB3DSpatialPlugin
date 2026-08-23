// SPDX-License-Identifier: GPL-2.0-only

#ifndef QTCOMPAT_H
#define QTCOMPAT_H

#include <QtGlobal>
#include <QImage>

namespace OpenRGB3DUi
{
/** ~20 FPS screen preview tick; lower CPU/GPU use than 30 FPS when mirroring displays. */
inline constexpr int kScreenPreviewTimerIntervalMs = 50;

inline QImage FlipImageVertical(const QImage& image)
{
    return image.mirrored(false, true);
}
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    #define MOUSE_EVENT_X(event) ((event)->position().x())
    #define MOUSE_EVENT_Y(event) ((event)->position().y())
    #define MOUSE_EVENT_POS(event) ((event)->position().toPoint())
#else
#error OpenRGB 3D Spatial Plugin requires Qt 6.8 or newer
#endif

#endif
