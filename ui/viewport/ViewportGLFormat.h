// SPDX-License-Identifier: GPL-2.0-only

#ifndef VIEWPORTGLFORMAT_H
#define VIEWPORTGLFORMAT_H

#include <QSurfaceFormat>

class QOpenGLWidget;

/** Compatibility-profile OpenGL surface for the room viewport (QOpenGLWidget). */
namespace ViewportGLFormat
{
constexpr float kDefaultFovyDegrees = 45.0f;
constexpr float kDefaultNearPlane = 0.1f;
constexpr float kDefaultFarPlane = 100000.0f;

QSurfaceFormat BuildSurfaceFormat();

void ApplyToWidget(::QOpenGLWidget* widget);
}

#endif
