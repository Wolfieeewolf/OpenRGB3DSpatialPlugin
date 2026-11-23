// SPDX-License-Identifier: GPL-2.0-or-later

#include "SpatialShaderEngine.h"
#include "SpatialOffscreenGlPool.h"
#include "FieldFullscreenQuad410.h"
#include "GlslUniformArray.h"
#include "SpatialVfxLib410.h"
#include "QtCompat.h"

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#ifndef GL_RG32F
#define GL_RG32F 0x8230
#endif
#ifndef GL_RG
#define GL_RG 0x8227
#endif

namespace
{

const char* kVertexShader = FieldFullscreenQuad410::VertexShaderSource();

QString BuildFragmentShader(const QString& user_body)
{
    return QStringLiteral(
               "#version 410 core\n"
               "uniform float u_time;\n"
               "uniform vec2 u_resolution;\n"
               "uniform float u_params[8];\n"
               "out vec4 frag_color;\n"
               "void spatialMain(out vec4 out_color, in vec2 frag_coord);\n")
           + SpatialVfxLib410::GlslSource()
           + user_body
           + QStringLiteral(
               "\nvoid main() {\n"
               "    vec4 c = vec4(0.0);\n"
               "    spatialMain(c, gl_FragCoord.xy);\n"
               "    frag_color = vec4(clamp(c.rgb, 0.0, 1.0), 1.0);\n"
               "}\n");
}

/** True UV point eval — spatialMain at uv * u_resolution (same space as atlas). */
QString BuildPointFragmentShader(const QString& user_body)
{
    return QStringLiteral(
               "#version 410 core\n"
               "uniform float u_time;\n"
               "uniform vec2 u_resolution;\n"
               "uniform float u_params[8];\n"
               "uniform sampler2D u_uvs;\n"
               "uniform vec2 u_point_atlas;\n"
               "uniform int u_point_count;\n"
               "out vec4 frag_color;\n"
               "void spatialMain(out vec4 out_color, in vec2 frag_coord);\n")
           + SpatialVfxLib410::GlslSource()
           + user_body
           + QStringLiteral(
               "\nvoid main() {\n"
               "    int ix = int(floor(gl_FragCoord.x));\n"
               "    int iy = int(floor(gl_FragCoord.y));\n"
               "    int i = iy * int(u_point_atlas.x) + ix;\n"
               "    if(i < 0 || i >= u_point_count) {\n"
               "        frag_color = vec4(0.0);\n"
               "        return;\n"
               "    }\n"
               "    vec2 uv = clamp(texelFetch(u_uvs, ivec2(ix, iy), 0).rg, 0.0, 1.0);\n"
               "    vec2 frag_coord = uv * u_resolution;\n"
               "    vec4 c = vec4(0.0);\n"
               "    spatialMain(c, frag_coord);\n"
               "    frag_color = vec4(clamp(c.rgb, 0.0, 1.0), 1.0);\n"
               "}\n");
}

} // namespace

SpatialShaderEngine::SpatialShaderEngine(QObject* parent)
    : QObject(parent)
{
}

SpatialShaderEngine::~SpatialShaderEngine()
{
    stop();
    destroyPointPbos();
    SpatialOffscreenGlPool::Session gl;
    if(gl)
    {
        destroyPointResources();
        fbo_.reset();
        overlay_handoff_.destroy();
        program_.reset();
        point_program_.reset();
    }
    else
    {
        overlay_handoff_ = SpatialOverlayHandoff{};
    }
}

void SpatialShaderEngine::setRenderSize(int width, int height)
{
    std::lock_guard<std::mutex> lock(mutex_);
    render_width_ = std::clamp(width, kMinEdge, kMaxEdge);
    render_height_ = std::clamp(height, kMinEdge, kMaxEdge);
    size_dirty_ = true;
}

void SpatialShaderEngine::setFragmentBody(const QString& glsl_body)
{
    std::lock_guard<std::mutex> lock(mutex_);
    fragment_body_ = glsl_body;
    body_dirty_ = true;
    latest_frame_ = QImage();
}

void SpatialShaderEngine::setUniforms(const SpatialShaderUniforms& uniforms)
{
    std::lock_guard<std::mutex> lock(mutex_);
    uniform_values_.time_sec = uniforms.time_sec;
    uniform_values_.param_count = std::clamp(uniforms.param_count, 0, 8);
    for(int i = 0; i < 8; ++i)
    {
        uniform_values_.params[i] = (i < uniforms.param_count) ? uniforms.params[i] : 0.0f;
    }
    params_dirty_ = true;
}

void SpatialShaderEngine::start()
{
    running_.store(true);
}

void SpatialShaderEngine::stop()
{
    running_.store(false);
}

bool SpatialShaderEngine::compilePrograms(const QString& body)
{
    auto next = std::make_unique<QOpenGLShaderProgram>();
    if(!next->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader)
       || !next->addShaderFromSourceCode(QOpenGLShader::Fragment, BuildFragmentShader(body).toUtf8())
       || !next->link())
    {
        last_error_ = next->log();
        return false;
    }

    auto point = std::make_unique<QOpenGLShaderProgram>();
    if(!point->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader)
       || !point->addShaderFromSourceCode(QOpenGLShader::Fragment, BuildPointFragmentShader(body).toUtf8())
       || !point->link())
    {
        last_error_ = point->log();
        return false;
    }

    program_ = std::move(next);
    point_program_ = std::move(point);
    last_error_.clear();
    return true;
}

bool SpatialShaderEngine::ensurePrograms()
{
    if(fragment_body_.trimmed().isEmpty())
    {
        last_error_ = QStringLiteral("Empty shader body.");
        return false;
    }
    if(!body_dirty_ && program_ && point_program_)
    {
        return true;
    }
    if(!SpatialOffscreenGlPool::hostContextReady())
    {
        last_error_ = QStringLiteral("Host GL not ready.");
        return false;
    }
    if(!SpatialOffscreenGlPool::warmUp(&last_error_))
    {
        return false;
    }

    SpatialOffscreenGlPool::Session gl;
    if(!gl)
    {
        last_error_ = QStringLiteral("OpenGL makeCurrent failed.");
        return false;
    }

    if(!compilePrograms(fragment_body_))
    {
        emit compileMessage(last_error_);
        return false;
    }
    emit compileMessage(QString());
    body_dirty_ = false;
    return true;
}

void SpatialShaderEngine::destroyPointPbos()
{
    if(point_pbos_[0] == 0 && point_pbos_[1] == 0)
    {
        point_pbo_has_pending_ = false;
        point_pbo_bytes_ = 0;
        point_pbo_pending_w_ = 0;
        point_pbo_pending_h_ = 0;
        point_pbo_pending_count_ = 0;
        return;
    }

    QOpenGLContext* ctx = SpatialOffscreenGlPool::sharedContext();
    QOpenGLExtraFunctions* xf = ctx ? ctx->extraFunctions() : nullptr;
    if(xf && ctx && ctx->isValid())
    {
        SpatialOffscreenGlPool::Session gl;
        if(gl)
        {
            xf->glDeleteBuffers(2, point_pbos_);
        }
    }

    point_pbos_[0] = point_pbos_[1] = 0;
    point_pbo_has_pending_ = false;
    point_pbo_bytes_ = 0;
    point_pbo_write_idx_ = 0;
    point_pbo_pending_w_ = 0;
    point_pbo_pending_h_ = 0;
    point_pbo_pending_count_ = 0;
}

bool SpatialShaderEngine::ensurePointPbos(int bytes)
{
    QOpenGLContext* ctx = SpatialOffscreenGlPool::sharedContext();
    QOpenGLExtraFunctions* xf = ctx ? ctx->extraFunctions() : nullptr;
    if(!xf || bytes <= 0)
    {
        return false;
    }
    if(point_pbos_[0] != 0 && point_pbo_bytes_ == bytes)
    {
        return true;
    }
    destroyPointPbos();
    xf->glGenBuffers(2, point_pbos_);
    if(point_pbos_[0] == 0 || point_pbos_[1] == 0)
    {
        point_pbos_[0] = point_pbos_[1] = 0;
        return false;
    }
    for(int i = 0; i < 2; ++i)
    {
        xf->glBindBuffer(GL_PIXEL_PACK_BUFFER, point_pbos_[i]);
        xf->glBufferData(GL_PIXEL_PACK_BUFFER, bytes, nullptr, GL_STREAM_READ);
    }
    xf->glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    point_pbo_bytes_ = bytes;
    point_pbo_write_idx_ = 0;
    point_pbo_has_pending_ = false;
    point_pbo_pending_w_ = 0;
    point_pbo_pending_h_ = 0;
    point_pbo_pending_count_ = 0;
    if(readback_staging_.size() < (size_t)bytes)
    {
        readback_staging_.resize((size_t)bytes);
    }
    return true;
}

void SpatialShaderEngine::destroyPointResources()
{
    QOpenGLContext* ctx = SpatialOffscreenGlPool::sharedContext();
    QOpenGLFunctions* gl = ctx ? ctx->functions() : nullptr;
    if(gl && uv_tex_id_ != 0)
    {
        gl->glDeleteTextures(1, &uv_tex_id_);
    }
    uv_tex_id_ = 0;
    uv_tex_w_ = uv_tex_h_ = 0;
    point_fbo_.reset();
    point_fbo_w_ = point_fbo_h_ = 0;
}

bool SpatialShaderEngine::ensurePointFbo(int w, int h)
{
    if(point_fbo_ && point_fbo_w_ == w && point_fbo_h_ == h)
    {
        return true;
    }
    point_fbo_ = std::make_unique<QOpenGLFramebufferObject>(w, h);
    if(!point_fbo_->isValid())
    {
        point_fbo_.reset();
        point_fbo_w_ = point_fbo_h_ = 0;
        last_error_ = QStringLiteral("Shader Field point FBO allocation failed.");
        return false;
    }
    point_fbo_w_ = w;
    point_fbo_h_ = h;
    return true;
}

bool SpatialShaderEngine::uploadUvTexture(const float* uv01, int count, int w, int h)
{
    QOpenGLContext* ctx = SpatialOffscreenGlPool::sharedContext();
    QOpenGLFunctions* gl = ctx ? ctx->functions() : nullptr;
    if(!gl || !uv01 || count < 1 || w < 1 || h < 1)
    {
        return false;
    }
    const int cells = w * h;
    uv_upload_.assign((size_t)cells * 2u, 0.0f);
    const int n = std::min(count, cells);
    for(int i = 0; i < n; ++i)
    {
        uv_upload_[(size_t)i * 2u] = std::clamp(uv01[(size_t)i * 2u], 0.0f, 1.0f);
        uv_upload_[(size_t)i * 2u + 1u] = std::clamp(uv01[(size_t)i * 2u + 1u], 0.0f, 1.0f);
    }
    if(uv_tex_id_ == 0)
    {
        gl->glGenTextures(1, &uv_tex_id_);
        if(uv_tex_id_ == 0)
        {
            last_error_ = QStringLiteral("Shader Field UV texture allocation failed.");
            return false;
        }
    }
    gl->glBindTexture(GL_TEXTURE_2D, uv_tex_id_);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if(uv_tex_w_ != w || uv_tex_h_ != h)
    {
        gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, w, h, 0, GL_RG, GL_FLOAT, uv_upload_.data());
        uv_tex_w_ = w;
        uv_tex_h_ = h;
    }
    else
    {
        gl->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RG, GL_FLOAT, uv_upload_.data());
    }
    gl->glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void SpatialShaderEngine::unpackPointReadback(const unsigned char* rgba,
                                             int w,
                                             int count,
                                             float* out_rgb) const
{
    const float inv255 = 1.0f / 255.0f;
    for(int i = 0; i < count; ++i)
    {
        const int ix = i % w;
        const int iy = i / w;
        const unsigned char* src = rgba + ((size_t)iy * (size_t)w + (size_t)ix) * 4u;
        out_rgb[(size_t)i * 3u + 0u] = src[0] * inv255;
        out_rgb[(size_t)i * 3u + 1u] = src[1] * inv255;
        out_rgb[(size_t)i * 3u + 2u] = src[2] * inv255;
    }
}

bool SpatialShaderEngine::renderAtlas(bool readback_cpu)
{
    if(!ensurePrograms())
    {
        return false;
    }

    SpatialOffscreenGlPool::Session gl;
    if(!gl)
    {
        last_error_ = QStringLiteral("OpenGL makeCurrent failed.");
        return false;
    }

    QOpenGLContext* ctx = SpatialOffscreenGlPool::sharedContext();
    QOpenGLFunctions* glf = ctx ? ctx->functions() : nullptr;
    if(!glf || !program_)
    {
        last_error_ = QStringLiteral("OpenGL functions unavailable.");
        return false;
    }

    /* Free ids retired last bake — viewport has already adopted the newer publish. */
    overlay_handoff_.releaseDeferred();

    const int w = render_width_;
    const int h = render_height_;
    if(size_dirty_ || !fbo_ || fbo_w_ != w || fbo_h_ != h)
    {
        fbo_ = std::make_unique<QOpenGLFramebufferObject>(w, h);
        if(!fbo_->isValid())
        {
            last_error_ = QStringLiteral("Shader Field FBO allocation failed.");
            fbo_.reset();
            fbo_w_ = fbo_h_ = 0;
            emit compileMessage(last_error_);
            return false;
        }
        fbo_w_ = w;
        fbo_h_ = h;
        size_dirty_ = false;
    }

    float params[8] = {};
    for(int i = 0; i < 8; ++i)
    {
        params[i] = uniform_values_.params[i];
    }

    program_->bind();
    fbo_->bind();
    glf->glViewport(0, 0, w, h);
    program_->setUniformValue("u_time", uniform_values_.time_sec);
    program_->setUniformValue("u_resolution", QVector2D((float)w, (float)h));
    SetGlslFloatUniformArray(*program_, glf, "u_params", params, 8);

    if(!FieldFullscreenQuad410::Draw(ctx))
    {
        last_error_ = QStringLiteral("Fullscreen quad draw failed.");
        fbo_->release();
        program_->release();
        return false;
    }

    if(readback_cpu)
    {
        if(readback_staging_.size() < (size_t)w * (size_t)h * 4u)
        {
            readback_staging_.resize((size_t)w * (size_t)h * 4u);
        }
        glf->glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glf->glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, readback_staging_.data());
        QImage raw(readback_staging_.data(), w, h, w * 4, QImage::Format_RGBA8888);
        latest_frame_ = OpenRGB3DUi::FlipImageVertical(raw.copy());
    }

    /* Upright durable copy — overlay sampleFrame expects top-left rows (same as CPU export). */
    const bool handoff_ok = overlay_handoff_.publishCopy(fbo_->texture(), w, h, true);

    fbo_->release();
    program_->release();
    params_dirty_ = false;
    if(!handoff_ok)
    {
        last_error_ = QStringLiteral("Overlay handoff publish failed.");
        return false;
    }
    return true;
}

bool SpatialShaderEngine::ensureReady()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(!running_.load())
    {
        running_.store(true);
    }
    /* Compile only — LED path uses sampleAtUvs; atlas bake is overlay-only. */
    if(!body_dirty_ && program_ && point_program_)
    {
        return true;
    }
    return ensurePrograms();
}

bool SpatialShaderEngine::sampleAtUvs(const float* uv01, int count, float* out_rgb)
{
    if(!uv01 || !out_rgb || count <= 0)
    {
        return false;
    }
    if(count > kMaxPointEdge * kMaxPointEdge)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_error_ = QStringLiteral("Point count exceeds GPU limit.");
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if(!running_.load())
    {
        running_.store(true);
    }
    if(!ensurePrograms())
    {
        return false;
    }

    SpatialOffscreenGlPool::Session gl;
    if(!gl)
    {
        last_error_ = QStringLiteral("OpenGL makeCurrent failed.");
        return false;
    }

    QOpenGLContext* ctx = SpatialOffscreenGlPool::sharedContext();
    QOpenGLFunctions* glf = ctx ? ctx->functions() : nullptr;
    if(!glf || !point_program_)
    {
        last_error_ = QStringLiteral("OpenGL functions unavailable.");
        return false;
    }

    const int w = std::min(count, kMaxPointEdge);
    const int h = (count + w - 1) / w;
    if(!ensurePointFbo(w, h) || !uploadUvTexture(uv01, count, w, h))
    {
        return false;
    }

    float params[8] = {};
    for(int i = 0; i < 8; ++i)
    {
        params[i] = uniform_values_.params[i];
    }

    point_program_->bind();
    point_fbo_->bind();
    glf->glViewport(0, 0, w, h);
    point_program_->setUniformValue("u_time", uniform_values_.time_sec);
    point_program_->setUniformValue("u_resolution",
                                    QVector2D((float)render_width_, (float)render_height_));
    SetGlslFloatUniformArray(*point_program_, glf, "u_params", params, 8);
    point_program_->setUniformValue("u_point_atlas", QVector2D((float)w, (float)h));
    point_program_->setUniformValue("u_point_count", count);

    glf->glActiveTexture(GL_TEXTURE0);
    glf->glBindTexture(GL_TEXTURE_2D, uv_tex_id_);
    point_program_->setUniformValue("u_uvs", 0);

    if(!FieldFullscreenQuad410::Draw(ctx))
    {
        last_error_ = QStringLiteral("Point-eval quad draw failed.");
        glf->glBindTexture(GL_TEXTURE_2D, 0);
        point_fbo_->release();
        point_program_->release();
        return false;
    }

    const size_t bytes = (size_t)w * (size_t)h * 4u;
    if(readback_staging_.size() < bytes)
    {
        readback_staging_.resize(bytes);
    }
    glf->glPixelStorei(GL_PACK_ALIGNMENT, 1);

    QOpenGLExtraFunctions* xf = ctx->extraFunctions();
    bool filled = false;
    if(xf && ensurePointPbos((int)bytes))
    {
        xf->glBindBuffer(GL_PIXEL_PACK_BUFFER, point_pbos_[point_pbo_write_idx_]);
        glf->glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        if(point_pbo_has_pending_
           && point_pbo_pending_w_ == w
           && point_pbo_pending_h_ == h
           && point_pbo_pending_count_ == count)
        {
            const int read_idx = 1 - point_pbo_write_idx_;
            xf->glBindBuffer(GL_PIXEL_PACK_BUFFER, point_pbos_[read_idx]);
            void* ptr = xf->glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, (GLsizeiptr)bytes, GL_MAP_READ_BIT);
            if(ptr)
            {
                unpackPointReadback(static_cast<const unsigned char*>(ptr), w, count, out_rgb);
                xf->glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
                filled = true;
            }
        }

        xf->glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        point_pbo_write_idx_ = 1 - point_pbo_write_idx_;
        point_pbo_has_pending_ = true;
        point_pbo_pending_w_ = w;
        point_pbo_pending_h_ = h;
        point_pbo_pending_count_ = count;
    }

    if(!filled)
    {
        glf->glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, readback_staging_.data());
        unpackPointReadback(readback_staging_.data(), w, count, out_rgb);
    }

    glf->glBindTexture(GL_TEXTURE_2D, 0);
    point_fbo_->release();
    point_program_->release();
    params_dirty_ = false;
    last_error_.clear();
    return true;
}

bool SpatialShaderEngine::ensureAtlasTextureReady()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(!running_.load())
    {
        running_.store(true);
    }
    return renderAtlas(false);
}

unsigned int SpatialShaderEngine::atlasTextureId() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return overlay_handoff_.textureId();
}

int SpatialShaderEngine::atlasTextureWidth() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return overlay_handoff_.empty() ? 0 : overlay_handoff_.width();
}

int SpatialShaderEngine::atlasTextureHeight() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return overlay_handoff_.empty() ? 0 : overlay_handoff_.height();
}

bool SpatialShaderEngine::exportAtlasRgba8(std::vector<unsigned char>& out_rgba, int& out_w, int& out_h)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(!running_.load())
    {
        running_.store(true);
    }
    if(!renderAtlas(true) || latest_frame_.isNull())
    {
        return false;
    }
    out_w = latest_frame_.width();
    out_h = latest_frame_.height();
    QImage rgba = latest_frame_.convertToFormat(QImage::Format_RGBA8888);
    out_rgba.resize((size_t)out_w * (size_t)out_h * 4u);
    for(int y = 0; y < out_h; ++y)
    {
        std::memcpy(out_rgba.data() + (size_t)y * (size_t)out_w * 4u,
                    rgba.constScanLine(y),
                    (size_t)out_w * 4u);
    }
    return true;
}

QImage SpatialShaderEngine::latestFrame() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return latest_frame_;
}

QString SpatialShaderEngine::lastError() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

int SpatialShaderEngine::renderWidth() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return render_width_;
}

int SpatialShaderEngine::renderHeight() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return render_height_;
}
