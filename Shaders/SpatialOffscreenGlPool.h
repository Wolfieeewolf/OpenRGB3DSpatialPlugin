// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QString>

class QOffscreenSurface;
class QOpenGLContext;
class QSurface;

/**
 * One shared offscreen GL context for all volume/strip/shader-field assists.
 * Creating a context per effect destabilizes Windows drivers when combined
 * with the main viewport context.
 *
 * Format matches the room viewport: OpenGL 4.1 Core (no MSAA). The pool
 * context shares with the host QOpenGLWidget context. After each Session,
 * restore the host via QOpenGLWidget::makeCurrent (never raw makeCurrent on
 * the widget context).
 */
class SpatialOffscreenGlPool
{
public:
    using HostMakeCurrentFn = void (*)(void* user);

    /** Call from LEDViewport3D::initializeGL once the host GL stack is live. */
    static void notifyHostContextReady();

    /** Register QOpenGLWidget::makeCurrent + host context for shareContext. */
    static void setHostMakeCurrent(HostMakeCurrentFn fn, void* user, QOpenGLContext* host_context);
    static void clearHostMakeCurrent(void* user);

    static bool hostContextReady();

    static QOpenGLContext* sharedContext();
    static QOffscreenSurface* sharedSurface();

    /** Create the shared 4.1 Core context if needed (does not leave it current). */
    static bool warmUp(QString* error = nullptr);

    /** RAII: serializes makeCurrent/doneCurrent for all field-engine GL work. */
    class Session
    {
    public:
        Session();
        ~Session();

        explicit operator bool() const { return ok_; }

    private:
        bool ok_ = false;
        QOpenGLContext* previous_context_ = nullptr;
        QSurface* previous_surface_ = nullptr;
        bool previous_was_host_ = false;
    };

private:
    SpatialOffscreenGlPool() = delete;
};
