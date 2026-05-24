// SPDX-License-Identifier: GPL-2.0-only

#include "ViewportGLFormat.h"

#include <QOpenGLWidget>

namespace ViewportGLFormat
{

QSurfaceFormat BuildSurfaceFormat(Backend backend)
{
    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    fmt.setSamples(4);
    fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    fmt.setSwapInterval(1);

    switch(backend)
    {
        case Backend::ModernCore33:
            fmt.setRenderableType(QSurfaceFormat::OpenGL);
            fmt.setProfile(QSurfaceFormat::CoreProfile);
            fmt.setVersion(3, 3);
            break;
        case Backend::LegacyFixedFunction:
        default:
            fmt.setRenderableType(QSurfaceFormat::OpenGL);
            fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
            fmt.setVersion(2, 1);
            break;
    }

    return fmt;
}

void ApplyToWidget(::QOpenGLWidget* widget, Backend backend)
{
    if(!widget)
    {
        return;
    }
    widget->setFormat(BuildSurfaceFormat(backend));
}

} // namespace ViewportGLFormat
