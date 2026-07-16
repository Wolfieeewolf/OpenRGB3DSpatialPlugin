// SPDX-License-Identifier: GPL-2.0-only

#include "ViewportGLFormat.h"

#include <QOpenGLWidget>

namespace ViewportGLFormat
{

QSurfaceFormat BuildSurfaceFormat()
{
    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    fmt.setSamples(4);
    fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    fmt.setSwapInterval(1);
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
    fmt.setVersion(2, 1);
    return fmt;
}

void ApplyToWidget(::QOpenGLWidget* widget)
{
    if(!widget)
    {
        return;
    }
    widget->setFormat(BuildSurfaceFormat());
}

} // namespace ViewportGLFormat
