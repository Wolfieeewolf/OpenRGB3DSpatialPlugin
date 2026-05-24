// SPDX-License-Identifier: GPL-2.0-only

#ifndef VIEWPORTGLFORMAT_H
#define VIEWPORTGLFORMAT_H

#include <QSurfaceFormat>

class QOpenGLWidget;

/** Prep for modern GL: centralizes widget surface format (compat today, core later). */
namespace ViewportGLFormat
{
/** Defaults used by LEDViewport3D (matches legacy gluPerspective 45°, depth, vsync). */
constexpr float kDefaultFovyDegrees = 45.0f;
constexpr float kDefaultNearPlane = 0.1f;
constexpr float kDefaultFarPlane = 100000.0f;

enum class Backend
{
    LegacyFixedFunction,
    ModernCore33,
};

/**
 * Build format for LEDViewport3D. Compatibility profile hosts GLSL 1.20 shader paths today.
 * ModernCore33 is for a future switch once all draws use core-only entry points.
 */
QSurfaceFormat BuildSurfaceFormat(Backend backend = Backend::LegacyFixedFunction);

void ApplyToWidget(::QOpenGLWidget* widget, Backend backend = Backend::LegacyFixedFunction);
}

#endif
