// SPDX-License-Identifier: GPL-2.0-or-later

#include "ActionMediaGpu.h"
#include "SpatialOffscreenGlPool.h"
#include "FieldFullscreenQuad410.h"

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

#ifndef GL_RG32F
#define GL_RG32F 0x8230
#endif
#ifndef GL_RG
#define GL_RG 0x8227
#endif
#ifndef GL_R8
#define GL_R8 0x8229
#endif
#ifndef GL_RED
#define GL_RED 0x1903
#endif

namespace Action
{
namespace media_gpu
{
namespace
{

constexpr int kMaxPointEdge = 2048;
constexpr int kAlphaGate = 16; /* matches SampleNearestAlpha (frag uses 16/255) */

std::mutex g_mu;
std::unique_ptr<QOpenGLShaderProgram> g_program;
std::unique_ptr<QOpenGLFramebufferObject> g_point_fbo;
int g_point_w = 0;
int g_point_h = 0;
unsigned int g_media_tex = 0;
int g_media_w = 0;
int g_media_h = 0;
unsigned int g_column_tex = 0;
int g_column_w = 0;
unsigned int g_uv_tex = 0;
int g_uv_w = 0;
int g_uv_h = 0;
unsigned int g_flag_tex = 0;
std::vector<float> g_uv_upload;
std::vector<unsigned char> g_flag_upload;
std::vector<unsigned char> g_readback;
qint64 g_frame_key = 0;
bool g_ready = false;

const char* kVert = FieldFullscreenQuad410::VertexShaderSource();

const char* kFrag = R"GLSL(
#version 410 core
uniform sampler2D u_media;
uniform sampler2D u_column;
uniform sampler2D u_uvs;
uniform sampler2D u_flags;
uniform vec2 u_point_atlas;
uniform int u_point_count;
uniform int u_media_w;
uniform int u_media_h;
out vec4 frag_color;
void main()
{
    int ix = int(floor(gl_FragCoord.x));
    int iy = int(floor(gl_FragCoord.y));
    int i = iy * int(u_point_atlas.x) + ix;
    if(i < 0 || i >= u_point_count)
    {
        frag_color = vec4(0.0);
        return;
    }
    vec2 uv = clamp(texelFetch(u_uvs, ivec2(ix, iy), 0).rg, 0.0, 1.0);
    float collapse = texelFetch(u_flags, ivec2(ix, iy), 0).r;
    vec4 c;
    int mw = max(u_media_w, 1);
    int mh = max(u_media_h, 1);
    /* Match CPU UvToX / UvToY (round + v flipped). */
    int px = clamp(int(round(uv.x * float(mw - 1))), 0, mw - 1);
    if(collapse > 0.5)
    {
        c = texelFetch(u_column, ivec2(px, 0), 0);
    }
    else
    {
        int py = clamp(int(round((1.0 - uv.y) * float(mh - 1))), 0, mh - 1);
        c = texelFetch(u_media, ivec2(px, py), 0);
    }
    if(c.a < 16.0 / 255.0)
    {
        frag_color = vec4(0.0);
        return;
    }
    frag_color = vec4(c.rgb, 1.0);
}
)GLSL";

bool EnsureProgram()
{
    if(g_program)
    {
        return true;
    }
    auto prog = std::make_unique<QOpenGLShaderProgram>();
    if(!prog->addShaderFromSourceCode(QOpenGLShader::Vertex, kVert)
       || !prog->addShaderFromSourceCode(QOpenGLShader::Fragment, QString::fromLatin1(kFrag))
       || !prog->link())
    {
        return false;
    }
    g_program = std::move(prog);
    return true;
}

void DestroyTexturesUnlocked()
{
    QOpenGLContext* ctx = SpatialOffscreenGlPool::sharedContext();
    QOpenGLFunctions* gl = ctx ? ctx->functions() : nullptr;
    if(gl)
    {
        if(g_media_tex)
        {
            gl->glDeleteTextures(1, &g_media_tex);
        }
        if(g_column_tex)
        {
            gl->glDeleteTextures(1, &g_column_tex);
        }
        if(g_uv_tex)
        {
            gl->glDeleteTextures(1, &g_uv_tex);
        }
        if(g_flag_tex)
        {
            gl->glDeleteTextures(1, &g_flag_tex);
        }
    }
    g_media_tex = g_column_tex = g_uv_tex = g_flag_tex = 0;
    g_media_w = g_media_h = g_column_w = g_uv_w = g_uv_h = 0;
    g_point_fbo.reset();
    g_point_w = g_point_h = 0;
    g_frame_key = 0;
    g_ready = false;
}

qint64 FrameKey(const QImage& frame)
{
    return ((qint64)frame.cacheKey()) ^ (((qint64)frame.width()) << 32) ^ (qint64)frame.height();
}

bool UploadColumnStrip(const QImage& frame)
{
    QOpenGLContext* ctx = SpatialOffscreenGlPool::sharedContext();
    QOpenGLFunctions* gl = ctx ? ctx->functions() : nullptr;
    if(!gl || frame.isNull() || frame.width() < 1 || frame.height() < 1)
    {
        return false;
    }
    const int w = frame.width();
    const int h = frame.height();
    std::vector<unsigned char> rgba((size_t)w * 4u, 0);
    for(int x = 0; x < w; ++x)
    {
        int best_score = -1;
        QRgb best = 0;
        for(int y = 0; y < h; ++y)
        {
            const QRgb px = frame.pixel(x, y);
            const int a = qAlpha(px);
            if(a < kAlphaGate)
            {
                continue;
            }
            const int score = a * 3 + qRed(px) + qGreen(px) + qBlue(px);
            if(score > best_score)
            {
                best_score = score;
                best = px;
            }
        }
        unsigned char* d = rgba.data() + (size_t)x * 4u;
        if(best_score < 0)
        {
            d[0] = d[1] = d[2] = d[3] = 0;
        }
        else
        {
            d[0] = (unsigned char)qRed(best);
            d[1] = (unsigned char)qGreen(best);
            d[2] = (unsigned char)qBlue(best);
            d[3] = (unsigned char)qAlpha(best);
        }
    }
    if(g_column_tex == 0)
    {
        gl->glGenTextures(1, &g_column_tex);
    }
    gl->glBindTexture(GL_TEXTURE_2D, g_column_tex);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    gl->glBindTexture(GL_TEXTURE_2D, 0);
    g_column_w = w;
    return g_column_tex != 0;
}

} // namespace

void Reset()
{
    std::lock_guard<std::mutex> lock(g_mu);
    if(!SpatialOffscreenGlPool::hostContextReady())
    {
        g_media_tex = g_column_tex = g_uv_tex = g_flag_tex = 0;
        g_program.reset();
        g_point_fbo.reset();
        g_ready = false;
        g_frame_key = 0;
        return;
    }
    SpatialOffscreenGlPool::Session gl;
    if(gl)
    {
        DestroyTexturesUnlocked();
        g_program.reset();
    }
    else
    {
        g_media_tex = g_column_tex = g_uv_tex = g_flag_tex = 0;
        g_program.reset();
        g_point_fbo.reset();
        g_ready = false;
        g_frame_key = 0;
    }
}

bool IsAvailable()
{
    std::lock_guard<std::mutex> lock(g_mu);
    return g_ready && g_media_tex != 0;
}

bool UploadPreparedFrame(const QImage& frame)
{
    if(frame.isNull() || frame.width() < 1 || frame.height() < 1)
    {
        return false;
    }
    if(!SpatialOffscreenGlPool::hostContextReady())
    {
        return false;
    }
    QString err;
    if(!SpatialOffscreenGlPool::warmUp(&err))
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_mu);
    const qint64 key = FrameKey(frame);
    if(g_ready && g_frame_key == key && g_media_tex != 0)
    {
        return true;
    }

    SpatialOffscreenGlPool::Session gl;
    if(!gl)
    {
        return false;
    }
    if(!EnsureProgram())
    {
        return false;
    }

    QOpenGLFunctions* glf = SpatialOffscreenGlPool::sharedContext()->functions();
    if(!glf)
    {
        return false;
    }

    QImage upload = (frame.format() == QImage::Format_RGBA8888)
                        ? frame
                        : frame.convertToFormat(QImage::Format_RGBA8888);

    if(g_media_tex == 0)
    {
        glf->glGenTextures(1, &g_media_tex);
    }
    glf->glBindTexture(GL_TEXTURE_2D, g_media_tex);
    glf->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glf->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glf->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glf->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glf->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glf->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, upload.width(), upload.height(), 0,
                      GL_RGBA, GL_UNSIGNED_BYTE, upload.constBits());
    glf->glBindTexture(GL_TEXTURE_2D, 0);
    g_media_w = upload.width();
    g_media_h = upload.height();

    if(!UploadColumnStrip(frame))
    {
        return false;
    }

    g_frame_key = key;
    g_ready = (g_media_tex != 0);
    return g_ready;
}

bool SampleAtUvs(const float* uv01,
                 const unsigned char* collapse_flags,
                 int count,
                 float* out_rgb,
                 unsigned char* out_lit)
{
    if(!uv01 || !out_rgb || !out_lit || count <= 0 || count > kMaxPointEdge * kMaxPointEdge)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_mu);
    if(!g_ready || !g_program || g_media_tex == 0)
    {
        return false;
    }

    SpatialOffscreenGlPool::Session gl;
    if(!gl)
    {
        return false;
    }
    QOpenGLContext* ctx = SpatialOffscreenGlPool::sharedContext();
    QOpenGLFunctions* glf = ctx ? ctx->functions() : nullptr;
    if(!glf)
    {
        return false;
    }

    const int w = std::min(count, kMaxPointEdge);
    const int h = (count + w - 1) / w;
    if(!g_point_fbo || g_point_w != w || g_point_h != h || !g_point_fbo->isValid())
    {
        g_point_fbo = std::make_unique<QOpenGLFramebufferObject>(w, h);
        g_point_w = w;
        g_point_h = h;
        if(!g_point_fbo->isValid())
        {
            g_point_fbo.reset();
            return false;
        }
    }

    const int cells = w * h;
    g_uv_upload.assign((size_t)cells * 2u, 0.0f);
    g_flag_upload.assign((size_t)cells, 0);
    for(int i = 0; i < count; ++i)
    {
        g_uv_upload[(size_t)i * 2u] = std::clamp(uv01[(size_t)i * 2u], 0.0f, 1.0f);
        g_uv_upload[(size_t)i * 2u + 1u] = std::clamp(uv01[(size_t)i * 2u + 1u], 0.0f, 1.0f);
        g_flag_upload[(size_t)i] = (collapse_flags && collapse_flags[i]) ? 255 : 0;
    }

    if(g_uv_tex == 0)
    {
        glf->glGenTextures(1, &g_uv_tex);
    }
    glf->glBindTexture(GL_TEXTURE_2D, g_uv_tex);
    glf->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glf->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glf->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glf->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glf->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glf->glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, w, h, 0, GL_RG, GL_FLOAT, g_uv_upload.data());

    if(g_flag_tex == 0)
    {
        glf->glGenTextures(1, &g_flag_tex);
    }
    glf->glBindTexture(GL_TEXTURE_2D, g_flag_tex);
    glf->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glf->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glf->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glf->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glf->glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, g_flag_upload.data());
    glf->glBindTexture(GL_TEXTURE_2D, 0);

    g_point_fbo->bind();
    glf->glViewport(0, 0, w, h);
    g_program->bind();
    glf->glActiveTexture(GL_TEXTURE0);
    glf->glBindTexture(GL_TEXTURE_2D, g_media_tex);
    g_program->setUniformValue("u_media", 0);
    glf->glActiveTexture(GL_TEXTURE1);
    glf->glBindTexture(GL_TEXTURE_2D, g_column_tex);
    g_program->setUniformValue("u_column", 1);
    glf->glActiveTexture(GL_TEXTURE2);
    glf->glBindTexture(GL_TEXTURE_2D, g_uv_tex);
    g_program->setUniformValue("u_uvs", 2);
    glf->glActiveTexture(GL_TEXTURE3);
    glf->glBindTexture(GL_TEXTURE_2D, g_flag_tex);
    g_program->setUniformValue("u_flags", 3);
    g_program->setUniformValue("u_point_atlas", QVector2D((float)w, (float)h));
    g_program->setUniformValue("u_point_count", count);
    g_program->setUniformValue("u_media_w", g_media_w);
    g_program->setUniformValue("u_media_h", g_media_h);

    if(!FieldFullscreenQuad410::Draw(ctx))
    {
        g_program->release();
        g_point_fbo->release();
        return false;
    }

    const size_t bytes = (size_t)w * (size_t)h * 4u;
    g_readback.resize(bytes);
    glf->glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glf->glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, g_readback.data());
    g_program->release();
    g_point_fbo->release();
    glf->glActiveTexture(GL_TEXTURE3);
    glf->glBindTexture(GL_TEXTURE_2D, 0);
    glf->glActiveTexture(GL_TEXTURE2);
    glf->glBindTexture(GL_TEXTURE_2D, 0);
    glf->glActiveTexture(GL_TEXTURE1);
    glf->glBindTexture(GL_TEXTURE_2D, 0);
    glf->glActiveTexture(GL_TEXTURE0);
    glf->glBindTexture(GL_TEXTURE_2D, 0);

    for(int i = 0; i < count; ++i)
    {
        const int ix = i % w;
        const int iy = i / w;
        const unsigned char* src = g_readback.data() + ((size_t)iy * (size_t)w + (size_t)ix) * 4u;
        const bool lit = src[3] > 0;
        out_lit[i] = lit ? 1 : 0;
        out_rgb[(size_t)i * 3u + 0u] = lit ? src[0] / 255.0f : 0.0f;
        out_rgb[(size_t)i * 3u + 1u] = lit ? src[1] / 255.0f : 0.0f;
        out_rgb[(size_t)i * 3u + 2u] = lit ? src[2] / 255.0f : 0.0f;
    }
    return true;
}

} // namespace media_gpu
} // namespace Action
