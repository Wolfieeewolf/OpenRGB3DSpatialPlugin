// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialOffscreenGlPool.h"

#include "FieldFullscreenQuad410.h"
#include "PluginLog.h"

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QSurface>
#include <QSurfaceFormat>

#include <atomic>
#include <memory>
#include <mutex>

namespace
{

std::recursive_mutex g_pool_mutex;
std::unique_ptr<QOffscreenSurface> g_surface;
std::unique_ptr<QOpenGLContext> g_context;
bool g_warmed = false;
bool g_logged_gl = false;
QString g_warm_error;
std::atomic<bool> g_host_ready{false};

SpatialOffscreenGlPool::HostMakeCurrentFn g_host_make_current = nullptr;
void* g_host_user = nullptr;
QOpenGLContext* g_host_context = nullptr;

QSurfaceFormat OffscreenFormat()
{
    QSurfaceFormat fmt;
    fmt.setVersion(4, 1);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setSwapBehavior(QSurfaceFormat::SingleBuffer);
    fmt.setSamples(0);
    fmt.setDepthBufferSize(0);
    fmt.setStencilBufferSize(0);
    return fmt;
}

void invokeHostMakeCurrent()
{
    if(g_host_make_current && g_host_user)
    {
        g_host_make_current(g_host_user);
    }
}

void restoreAfterOffscreen(QOpenGLContext* previous_context,
                           QSurface* previous_surface,
                           bool previous_was_host)
{
    if(previous_was_host || !previous_context)
    {
        invokeHostMakeCurrent();
        return;
    }
    if(previous_context->isValid() && previous_surface)
    {
        previous_context->makeCurrent(previous_surface);
        return;
    }
    invokeHostMakeCurrent();
}

void logOffscreenGlOnce()
{
    if(g_logged_gl || !g_context || !g_context->isValid())
    {
        return;
    }
    const QSurfaceFormat fmt = g_context->format();
    const char* profile =
        (fmt.profile() == QSurfaceFormat::CoreProfile) ? "Core" :
        (fmt.profile() == QSurfaceFormat::CompatibilityProfile) ? "Compatibility" : "NoProfile";
    QOpenGLFunctions* fn = g_context->functions();
    const char* ver = fn ? reinterpret_cast<const char*>(fn->glGetString(GL_VERSION)) : nullptr;
    const char* renderer = fn ? reinterpret_cast<const char*>(fn->glGetString(GL_RENDERER)) : nullptr;
    const bool shared = (g_host_context
                         && g_context->shareGroup()
                         && g_host_context->shareGroup()
                         && g_context->shareGroup() == g_host_context->shareGroup());
    LOG_INFO("[3DSpatial] Offscreen field GL requested %d.%d %s; GL_VERSION=\"%s\" GL_RENDERER=\"%s\" share=%s",
             fmt.majorVersion(),
             fmt.minorVersion(),
             profile,
             ver ? ver : "(null)",
             renderer ? renderer : "(null)",
             shared ? "yes" : "no");
    g_logged_gl = true;
}

bool warmUpUnlocked(QString* error)
{
    if(g_warmed && g_context && g_context->isValid() && g_surface && g_surface->isValid())
    {
        if(error)
        {
            error->clear();
        }
        return true;
    }

    if(!g_host_context || !g_host_context->isValid())
    {
        g_warm_error = QStringLiteral("Host GL share context not registered.");
        g_warmed = false;
        if(error)
        {
            *error = g_warm_error;
        }
        return false;
    }

    if(!g_surface)
    {
        g_surface = std::make_unique<QOffscreenSurface>();
        g_surface->setFormat(OffscreenFormat());
        g_surface->create();
        if(!g_surface->isValid())
        {
            g_warm_error = QStringLiteral("Offscreen surface unavailable.");
            g_warmed = false;
            if(error)
            {
                *error = g_warm_error;
            }
            return false;
        }
    }

    if(!g_context)
    {
        g_context = std::make_unique<QOpenGLContext>();
        g_context->setFormat(OffscreenFormat());
        g_context->setShareContext(g_host_context);
        if(!g_context->create())
        {
            g_warm_error = QStringLiteral("OpenGL 4.1 Core offscreen context creation failed.");
            g_context.reset();
            g_warmed = false;
            if(error)
            {
                *error = g_warm_error;
            }
            return false;
        }
    }

    QOpenGLContext* previous_context = QOpenGLContext::currentContext();
    QSurface* previous_surface = previous_context ? previous_context->surface() : nullptr;
    const bool previous_was_host =
        (g_host_context != nullptr && previous_context == g_host_context);

    if(!g_context->makeCurrent(g_surface.get()))
    {
        g_warm_error = QStringLiteral("OpenGL makeCurrent failed.");
        g_warmed = false;
        if(error)
        {
            *error = g_warm_error;
        }
        return false;
    }
    logOffscreenGlOnce();
    g_context->doneCurrent();
    restoreAfterOffscreen(previous_context, previous_surface, previous_was_host);

    g_warm_error.clear();
    g_warmed = true;
    if(error)
    {
        error->clear();
    }
    return true;
}

} // namespace

void SpatialOffscreenGlPool::notifyHostContextReady()
{
    g_host_ready.store(true);
}

void SpatialOffscreenGlPool::setHostMakeCurrent(HostMakeCurrentFn fn, void* user, QOpenGLContext* host_context)
{
    std::lock_guard<std::recursive_mutex> lock(g_pool_mutex);
    /* First live room viewport wins. Secondary LEDViewport3D widgets (e.g. controller
       preview dialogs) must not steal or tear down the shared field pool. */
    if(g_host_user && g_host_user != user)
    {
        return;
    }
    g_host_make_current = fn;
    g_host_user = user;
    g_host_context = host_context;
    if(g_context)
    {
        if(g_context->isValid() && g_surface && g_surface->isValid()
           && g_context->makeCurrent(g_surface.get()))
        {
            FieldFullscreenQuad410::Destroy(g_context.get());
            g_context->doneCurrent();
        }
        else
        {
            FieldFullscreenQuad410::Destroy(nullptr);
        }
        g_context.reset();
        g_surface.reset();
        g_warmed = false;
        g_logged_gl = false;
    }
    invokeHostMakeCurrent();
}

void SpatialOffscreenGlPool::clearHostMakeCurrent(void* user)
{
    std::lock_guard<std::recursive_mutex> lock(g_pool_mutex);
    if(g_host_user == user)
    {
        g_host_make_current = nullptr;
        g_host_user = nullptr;
        g_host_context = nullptr;
        g_host_ready.store(false);
        if(g_context && g_context->isValid() && g_surface && g_surface->isValid()
           && g_context->makeCurrent(g_surface.get()))
        {
            FieldFullscreenQuad410::Destroy(g_context.get());
            g_context->doneCurrent();
        }
        else
        {
            FieldFullscreenQuad410::Destroy(nullptr);
        }
        g_context.reset();
        g_surface.reset();
        g_warmed = false;
        g_logged_gl = false;
    }
}

bool SpatialOffscreenGlPool::hostContextReady()
{
    return g_host_ready.load();
}

QOpenGLContext* SpatialOffscreenGlPool::sharedContext()
{
    return g_context.get();
}

QOffscreenSurface* SpatialOffscreenGlPool::sharedSurface()
{
    return g_surface.get();
}

bool SpatialOffscreenGlPool::warmUp(QString* error)
{
    std::lock_guard<std::recursive_mutex> lock(g_pool_mutex);
    return warmUpUnlocked(error);
}

SpatialOffscreenGlPool::Session::Session()
{
    g_pool_mutex.lock();
    previous_context_ = QOpenGLContext::currentContext();
    previous_surface_ = previous_context_ ? previous_context_->surface() : nullptr;
    previous_was_host_ =
        (g_host_context != nullptr && previous_context_ == g_host_context);

    QString err;
    if(!warmUpUnlocked(&err))
    {
        ok_ = false;
        return;
    }
    ok_ = g_context && g_context->makeCurrent(g_surface.get());
}

SpatialOffscreenGlPool::Session::~Session()
{
    if(ok_ && g_context)
    {
        g_context->doneCurrent();
    }
    restoreAfterOffscreen(previous_context_, previous_surface_, previous_was_host_);
    g_pool_mutex.unlock();
}
