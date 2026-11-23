// SPDX-License-Identifier: GPL-2.0-or-later
#include "SpatialOverlayHandoff.h"

#include "SpatialOffscreenGlPool.h"

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>

#ifndef GL_RGBA32F
#define GL_RGBA32F 0x8814
#endif

void SpatialOverlayHandoff::defer(unsigned int tex)
{
    if(tex == 0)
    {
        return;
    }
    for(unsigned int id : deferred_)
    {
        if(id == tex)
        {
            return;
        }
    }
    deferred_.push_back(tex);
}

void SpatialOverlayHandoff::releaseDeferred()
{
    if(deferred_.empty())
    {
        return;
    }
    QOpenGLContext* ctx = SpatialOffscreenGlPool::sharedContext();
    QOpenGLFunctions* gl = ctx ? ctx->functions() : nullptr;
    if(!gl)
    {
        /* Keep ids — deleting without a context would leak; retry next bake. */
        return;
    }
    for(unsigned int id : deferred_)
    {
        if(id != 0)
        {
            GLuint tid = id;
            gl->glDeleteTextures(1, &tid);
        }
    }
    deferred_.clear();
}

bool SpatialOverlayHandoff::ensureBlitFbos()
{
    if(blit_read_fbo_ != 0 && blit_draw_fbo_ != 0)
    {
        return true;
    }
    QOpenGLContext* ctx = SpatialOffscreenGlPool::sharedContext();
    QOpenGLExtraFunctions* xf = ctx ? ctx->extraFunctions() : nullptr;
    if(!xf)
    {
        return false;
    }
    GLuint fbos[2] = {0, 0};
    xf->glGenFramebuffers(2, fbos);
    if(fbos[0] == 0 || fbos[1] == 0)
    {
        xf->glDeleteFramebuffers(2, fbos);
        return false;
    }
    blit_read_fbo_ = fbos[0];
    blit_draw_fbo_ = fbos[1];
    return true;
}

bool SpatialOverlayHandoff::ensureOwnedTex(int w, int h, unsigned internal_format)
{
    if(owned_ && tex_ != 0 && w_ == w && h_ == h && internal_format_ == internal_format)
    {
        return true;
    }
    QOpenGLContext* ctx = SpatialOffscreenGlPool::sharedContext();
    QOpenGLFunctions* gl = ctx ? ctx->functions() : nullptr;
    if(!gl || w < 1 || h < 1)
    {
        return false;
    }
    GLuint dst = 0;
    gl->glGenTextures(1, &dst);
    if(dst == 0)
    {
        return false;
    }
    gl->glBindTexture(GL_TEXTURE_2D, dst);
    const GLint filter = (internal_format == GL_RGBA32F) ? GL_NEAREST : GL_LINEAR;
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->glTexImage2D(GL_TEXTURE_2D, 0, (GLint)internal_format, w, h, 0, GL_RGBA,
                     (internal_format == GL_RGBA32F) ? GL_FLOAT : GL_UNSIGNED_BYTE, nullptr);
    gl->glBindTexture(GL_TEXTURE_2D, 0);

    if(owned_ && tex_ != 0)
    {
        defer(tex_);
    }
    tex_ = dst;
    w_ = w;
    h_ = h;
    internal_format_ = internal_format;
    owned_ = true;
    return true;
}

bool SpatialOverlayHandoff::ensureScratchTex(int w, int h)
{
    if(scratch_tex_ != 0 && scratch_w_ == w && scratch_h_ == h)
    {
        return true;
    }
    QOpenGLContext* ctx = SpatialOffscreenGlPool::sharedContext();
    QOpenGLFunctions* gl = ctx ? ctx->functions() : nullptr;
    if(!gl || w < 1 || h < 1)
    {
        return false;
    }
    if(scratch_tex_ != 0)
    {
        GLuint old = scratch_tex_;
        gl->glDeleteTextures(1, &old);
        scratch_tex_ = 0;
    }
    GLuint id = 0;
    gl->glGenTextures(1, &id);
    if(id == 0)
    {
        return false;
    }
    gl->glBindTexture(GL_TEXTURE_2D, id);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    gl->glBindTexture(GL_TEXTURE_2D, 0);
    scratch_tex_ = id;
    scratch_w_ = w;
    scratch_h_ = h;
    return true;
}

bool SpatialOverlayHandoff::publishShared(unsigned int src_tex, int w, int h)
{
    if(src_tex == 0 || w < 1 || h < 1)
    {
        return false;
    }
    if(owned_ && tex_ != 0 && tex_ != src_tex)
    {
        defer(tex_);
        owned_ = false;
        internal_format_ = 0;
    }
    tex_ = src_tex;
    w_ = w;
    h_ = h;
    owned_ = false;
    internal_format_ = 0;
    return true;
}

bool SpatialOverlayHandoff::publishCopy(unsigned int src_tex, int w, int h, bool flip_y)
{
    if(src_tex == 0 || w < 1 || h < 1)
    {
        return false;
    }
    QOpenGLContext* ctx = SpatialOffscreenGlPool::sharedContext();
    QOpenGLExtraFunctions* xf = ctx ? ctx->extraFunctions() : nullptr;
    QOpenGLFunctions* gl = ctx ? ctx->functions() : nullptr;
    if(!xf || !gl || !ensureBlitFbos() || !ensureOwnedTex(w, h, GL_RGBA8))
    {
        return false;
    }

    GLint prev_read = 0;
    GLint prev_draw = 0;
    gl->glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read);
    gl->glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw);

    xf->glBindFramebuffer(GL_READ_FRAMEBUFFER, blit_read_fbo_);
    xf->glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, src_tex, 0);
    xf->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, blit_draw_fbo_);
    xf->glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_, 0);
    const GLenum read_ok = xf->glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
    const GLenum draw_ok = xf->glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    bool ok = (read_ok == GL_FRAMEBUFFER_COMPLETE && draw_ok == GL_FRAMEBUFFER_COMPLETE);
    if(ok)
    {
        if(flip_y)
        {
            xf->glBlitFramebuffer(0, 0, w, h, 0, h, w, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
        else
        {
            xf->glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
    }
    xf->glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prev_read);
    xf->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prev_draw);
    return ok;
}

bool SpatialOverlayHandoff::publishFromRgba32f(const float* rgba, int w, int h)
{
    if(!rgba || w < 1 || h < 1 || !ensureOwnedTex(w, h, GL_RGBA32F))
    {
        return false;
    }
    QOpenGLContext* ctx = SpatialOffscreenGlPool::sharedContext();
    QOpenGLFunctions* gl = ctx ? ctx->functions() : nullptr;
    if(!gl)
    {
        return false;
    }
    gl->glBindTexture(GL_TEXTURE_2D, tex_);
    gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    gl->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_FLOAT, rgba);
    gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl->glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

bool SpatialOverlayHandoff::beginCompositeRgba8(int w, int h)
{
    if(!ensureOwnedTex(w, h, GL_RGBA8) || !ensureBlitFbos())
    {
        return false;
    }
    QOpenGLContext* ctx = SpatialOffscreenGlPool::sharedContext();
    QOpenGLExtraFunctions* xf = ctx ? ctx->extraFunctions() : nullptr;
    QOpenGLFunctions* gl = ctx ? ctx->functions() : nullptr;
    if(!xf || !gl)
    {
        return false;
    }

    GLint prev_draw = 0;
    gl->glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw);
    GLfloat prev_clear[4] = {};
    gl->glGetFloatv(GL_COLOR_CLEAR_VALUE, prev_clear);

    xf->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, blit_draw_fbo_);
    xf->glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_, 0);
    if(xf->glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        xf->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prev_draw);
        return false;
    }
    gl->glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    gl->glClear(GL_COLOR_BUFFER_BIT);
    gl->glClearColor(prev_clear[0], prev_clear[1], prev_clear[2], prev_clear[3]);
    xf->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prev_draw);
    return true;
}

bool SpatialOverlayHandoff::compositePlaneRgba8(const unsigned char* rgba, int sw, int sh,
                                               int dx, int dy, int dw, int dh)
{
    if(!rgba || !owned_ || tex_ == 0 || sw < 1 || sh < 1 || dw < 1 || dh < 1)
    {
        return false;
    }
    if(dx < 0 || dy < 0 || dx + dw > w_ || dy + dh > h_)
    {
        return false;
    }
    QOpenGLContext* ctx = SpatialOffscreenGlPool::sharedContext();
    QOpenGLFunctions* gl = ctx ? ctx->functions() : nullptr;
    if(!gl)
    {
        return false;
    }

    /* Same size: direct upload into the stack (no scratch/FBO). */
    if(sw == dw && sh == dh)
    {
        gl->glBindTexture(GL_TEXTURE_2D, tex_);
        gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        gl->glTexSubImage2D(GL_TEXTURE_2D, 0, dx, dy, dw, dh, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        gl->glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    }

    QOpenGLExtraFunctions* xf = ctx->extraFunctions();
    if(!xf || !ensureBlitFbos() || !ensureScratchTex(sw, sh))
    {
        return false;
    }

    gl->glBindTexture(GL_TEXTURE_2D, scratch_tex_);
    gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, sw, sh, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    gl->glBindTexture(GL_TEXTURE_2D, 0);

    GLint prev_read = 0;
    GLint prev_draw = 0;
    gl->glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read);
    gl->glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw);

    xf->glBindFramebuffer(GL_READ_FRAMEBUFFER, blit_read_fbo_);
    xf->glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, scratch_tex_, 0);
    xf->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, blit_draw_fbo_);
    xf->glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_, 0);
    const GLenum read_ok = xf->glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
    const GLenum draw_ok = xf->glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    bool ok = (read_ok == GL_FRAMEBUFFER_COMPLETE && draw_ok == GL_FRAMEBUFFER_COMPLETE);
    if(ok)
    {
        /* Top-first upload → texture row 0 = image top; matches former atlas_rgba + texelFetch. */
        xf->glBlitFramebuffer(0, 0, sw, sh,
                              dx, dy, dx + dw, dy + dh,
                              GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    xf->glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prev_read);
    xf->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prev_draw);
    return ok;
}

void SpatialOverlayHandoff::retireBeforeDestroy(unsigned int tex)
{
    if(tex == 0)
    {
        return;
    }
    if(tex_ == tex)
    {
        defer(tex_);
        tex_ = 0;
        w_ = 0;
        h_ = 0;
        owned_ = false;
        internal_format_ = 0;
        return;
    }
    defer(tex);
}

void SpatialOverlayHandoff::destroy()
{
    releaseDeferred();
    QOpenGLContext* ctx = SpatialOffscreenGlPool::sharedContext();
    QOpenGLFunctions* gl = ctx ? ctx->functions() : nullptr;
    QOpenGLExtraFunctions* xf = ctx ? ctx->extraFunctions() : nullptr;
    if(gl && owned_ && tex_ != 0)
    {
        GLuint tid = tex_;
        gl->glDeleteTextures(1, &tid);
    }
    tex_ = 0;
    w_ = 0;
    h_ = 0;
    owned_ = false;
    internal_format_ = 0;
    if(gl && scratch_tex_ != 0)
    {
        GLuint sid = scratch_tex_;
        gl->glDeleteTextures(1, &sid);
    }
    scratch_tex_ = 0;
    scratch_w_ = 0;
    scratch_h_ = 0;
    if(xf && (blit_read_fbo_ != 0 || blit_draw_fbo_ != 0))
    {
        GLuint fbos[2] = {blit_read_fbo_, blit_draw_fbo_};
        xf->glDeleteFramebuffers(2, fbos);
    }
    blit_read_fbo_ = 0;
    blit_draw_fbo_ = 0;
}
