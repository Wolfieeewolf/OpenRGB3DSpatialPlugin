// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialOffscreenGlPool.h"

#include <QOffscreenSurface>
#include <QOpenGLContext>
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
QString g_warm_error;
std::atomic<bool> g_host_ready{false};

QSurfaceFormat OffscreenFormat()
{
    QSurfaceFormat fmt;
    fmt.setVersion(2, 1);
    fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setSwapBehavior(QSurfaceFormat::SingleBuffer);
    return fmt;
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
        g_context->setFormat(g_surface->format());
        if(!g_context->create())
        {
            g_warm_error = QStringLiteral("OpenGL context creation failed.");
            g_context.reset();
            g_warmed = false;
            if(error)
            {
                *error = g_warm_error;
            }
            return false;
        }
    }

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
    g_context->doneCurrent();

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
    g_pool_mutex.unlock();
}
