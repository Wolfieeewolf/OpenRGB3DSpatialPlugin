// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QString>

class QOffscreenSurface;
class QOpenGLContext;

/**
 * One shared offscreen GL context for all volume/strip field assists.
 * Creating a context per effect destabilizes Windows drivers when combined
 * with the main viewport context.
 */
class SpatialOffscreenGlPool
{
public:
    /** Call from LEDViewport3D::initializeGL once the host GL stack is live. */
    static void notifyHostContextReady();

    static bool hostContextReady();

    static QOpenGLContext* sharedContext();
    static QOffscreenSurface* sharedSurface();

    /** Create the shared context if needed (does not make it current). */
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
    };

private:
    SpatialOffscreenGlPool() = delete;
};
