// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialVolumeFieldEngine.h"

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{

const char* kVertexShader = R"(attribute vec2 a_position;
void main() {
    gl_Position = vec4(a_position, 0.0, 1.0);
}
)";

QString BuildFragmentShader(const QString& user_body)
{
    return QStringLiteral(
               "#version 110\n"
               "uniform float u_time;\n"
               "uniform float u_res;\n"
               "uniform vec2 u_atlas;\n"
               "uniform float u_params[16];\n"
               "uniform sampler2D u_media;\n"
               "void volumeMain(out vec4 out_color, in vec3 p01);\n")
           + user_body
           + QStringLiteral(
               "\nvoid main() {\n"
               "    float n = max(u_res, 1.0);\n"
               "    float fx = floor(gl_FragCoord.x);\n"
               "    float fy = floor(gl_FragCoord.y);\n"
               "    float slice = floor(fy / n);\n"
               "    float ly = fy - slice * n;\n"
               "    vec3 p01 = vec3((fx + 0.5) / n, (ly + 0.5) / n, (slice + 0.5) / n);\n"
               "    vec4 c = vec4(0.0);\n"
               "    volumeMain(c, clamp(p01, 0.0, 1.0));\n"
               "    gl_FragColor = vec4(clamp(c.rgb, 0.0, 1.0), 1.0);\n"
               "}\n");
}

void ConvertRgbaAtlasToRgb(const unsigned char* rgba, int n, std::vector<float>& out_rgb)
{
    const int atlas_w = n;
    const int atlas_h = n * n;
    const float inv255 = 1.0f / 255.0f;
    out_rgb.assign((size_t)n * (size_t)n * (size_t)n * 3u, 0.0f);
    for(int slice = 0; slice < n; ++slice)
    {
        for(int y = 0; y < n; ++y)
        {
            const int src_row = (atlas_h - 1 - (slice * n + y));
            const unsigned char* src_row_ptr = rgba + ((size_t)src_row * (size_t)atlas_w) * 4u;
            float* dst_row = out_rgb.data() + (((size_t)slice * (size_t)n + (size_t)y) * (size_t)n) * 3u;
            for(int x = 0; x < n; ++x)
            {
                const unsigned char* src = src_row_ptr + (size_t)x * 4u;
                float* dst = dst_row + (size_t)x * 3u;
                dst[0] = src[0] * inv255;
                dst[1] = src[1] * inv255;
                dst[2] = src[2] * inv255;
            }
        }
    }
}

} // namespace

SpatialVolumeFieldEngine::SpatialVolumeFieldEngine() = default;

SpatialVolumeFieldEngine::~SpatialVolumeFieldEngine()
{
    std::lock_guard<std::mutex> lock(mutex_);
    shutdownGl();
}

void SpatialVolumeFieldEngine::destroyPbos()
{
    if(pbos_[0] == 0 && pbos_[1] == 0)
    {
        pbo_has_pending_ = false;
        pbo_bytes_ = 0;
        pbo_pending_n_ = 0;
        return;
    }

    QOpenGLExtraFunctions* xf = context_ ? context_->extraFunctions() : nullptr;
    if(xf && context_->isValid())
    {
        const bool was_current = (QOpenGLContext::currentContext() == context_.get());
        bool made_current = was_current;
        if(!was_current && surface_)
        {
            made_current = context_->makeCurrent(surface_.get());
        }
        if(made_current)
        {
            xf->glDeleteBuffers(2, pbos_);
            if(!was_current)
            {
                context_->doneCurrent();
            }
        }
    }

    pbos_[0] = pbos_[1] = 0;
    pbo_has_pending_ = false;
    pbo_bytes_ = 0;
    pbo_pending_n_ = 0;
}

void SpatialVolumeFieldEngine::shutdownGl()
{
    destroyPbos();
    if(context_ && surface_ && context_->isValid())
    {
        context_->makeCurrent(surface_.get());
        destroyMediaTexture();
        fbo_.reset();
        program_.reset();
        context_->doneCurrent();
    }
    else
    {
        media_tex_id_ = 0;
        media_tex_w_ = 0;
        media_tex_h_ = 0;
        fbo_.reset();
        program_.reset();
    }
    context_.reset();
    surface_.reset();
    fbo_n_ = 0;
    gl_ok_ = false;
}

void SpatialVolumeFieldEngine::setFragmentBody(const QString& glsl_body)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(fragment_body_ == glsl_body)
    {
        return;
    }
    fragment_body_ = glsl_body;
    body_dirty_ = true;
}

void SpatialVolumeFieldEngine::setResolution(int n)
{
    const int clamped = std::clamp(n, kMinResolution, kMaxResolution);
    std::lock_guard<std::mutex> lock(mutex_);
    if(resolution_ == clamped)
    {
        return;
    }
    resolution_ = clamped;
    size_dirty_ = true;
}

void SpatialVolumeFieldEngine::setParams(const Params& params)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const int count = std::clamp(params.count, 0, kMaxParams);
    bool changed = (params_.time_sec != params.time_sec) || (params_.count != count);
    if(!changed)
    {
        for(int i = 0; i < count; ++i)
        {
            if(params_.values[i] != params.values[i])
            {
                changed = true;
                break;
            }
        }
    }
    if(!changed)
    {
        return;
    }
    params_.time_sec = params.time_sec;
    params_.count = count;
    for(int i = 0; i < kMaxParams; ++i)
    {
        params_.values[i] = (i < params_.count) ? params.values[i] : 0.0f;
    }
    params_dirty_ = true;
}

void SpatialVolumeFieldEngine::setMediaTexture(const QImage& image, bool wrap)
{
    std::lock_guard<std::mutex> lock(mutex_);
    QImage conv;
    if(!image.isNull() && image.width() > 0 && image.height() > 0)
    {
        conv = image.convertToFormat(QImage::Format_RGBA8888);
        if(conv.width() > kMaxMediaEdge || conv.height() > kMaxMediaEdge)
        {
            conv = conv.scaled(kMaxMediaEdge, kMaxMediaEdge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            conv = conv.convertToFormat(QImage::Format_RGBA8888);
        }
    }
    const qint64 key = conv.isNull() ? 0 : conv.cacheKey();
    if(key == media_cache_key_ && wrap == media_wrap_ && media_tex_id_ != 0 && !media_dirty_)
    {
        return;
    }
    media_image_ = std::move(conv);
    media_wrap_ = wrap;
    media_cache_key_ = key;
    media_dirty_ = true;
}

void SpatialVolumeFieldEngine::clearMediaTexture()
{
    setMediaTexture(QImage(), false);
}

bool SpatialVolumeFieldEngine::mediaDirty() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return media_dirty_;
}

QString SpatialVolumeFieldEngine::lastError() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

bool SpatialVolumeFieldEngine::ensureReady()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(!body_dirty_ && !params_dirty_ && !size_dirty_ && !media_dirty_ && atlas_res_ > 0 && !atlas_rgb_.empty())
    {
        return available_.load();
    }
    return renderAtlas();
}

bool SpatialVolumeFieldEngine::initGl()
{
    if(gl_ok_ && context_ && surface_ && context_->isValid())
    {
        return true;
    }
    shutdownGl();

    surface_ = std::make_unique<QOffscreenSurface>();
    surface_->create();
    if(!surface_->isValid())
    {
        last_error_ = QStringLiteral("Offscreen surface unavailable.");
        return false;
    }

    context_ = std::make_unique<QOpenGLContext>();
    context_->setFormat(surface_->format());
    if(!context_->create())
    {
        last_error_ = QStringLiteral("OpenGL context creation failed.");
        shutdownGl();
        return false;
    }
    if(!context_->makeCurrent(surface_.get()))
    {
        last_error_ = QStringLiteral("OpenGL makeCurrent failed.");
        shutdownGl();
        return false;
    }
    context_->doneCurrent();
    gl_ok_ = true;
    return true;
}

bool SpatialVolumeFieldEngine::compileProgram(const QString& body)
{
    if(!context_->makeCurrent(surface_.get()))
    {
        last_error_ = QStringLiteral("OpenGL makeCurrent failed.");
        return false;
    }

    program_ = std::make_unique<QOpenGLShaderProgram>();
    if(!program_->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader))
    {
        last_error_ = program_->log();
        program_.reset();
        context_->doneCurrent();
        return false;
    }
    const QString frag = BuildFragmentShader(body);
    if(!program_->addShaderFromSourceCode(QOpenGLShader::Fragment, frag.toUtf8()))
    {
        last_error_ = program_->log();
        program_.reset();
        context_->doneCurrent();
        return false;
    }
    if(!program_->link())
    {
        last_error_ = program_->log();
        program_.reset();
        context_->doneCurrent();
        return false;
    }
    context_->doneCurrent();
    return true;
}

bool SpatialVolumeFieldEngine::ensureFbo(int n)
{
    if(fbo_ && fbo_n_ == n && fbo_->isValid())
    {
        return true;
    }
    if(!context_->makeCurrent(surface_.get()))
    {
        last_error_ = QStringLiteral("OpenGL makeCurrent failed.");
        return false;
    }
    destroyPbos();
    fbo_ = std::make_unique<QOpenGLFramebufferObject>(n, n * n);
    fbo_n_ = n;
    const bool ok = fbo_->isValid();
    context_->doneCurrent();
    if(!ok)
    {
        last_error_ = QStringLiteral("Volume atlas FBO allocation failed.");
        fbo_.reset();
        fbo_n_ = 0;
    }
    return ok;
}

bool SpatialVolumeFieldEngine::ensurePbos(int n)
{
    QOpenGLExtraFunctions* xf = context_->extraFunctions();
    if(!xf)
    {
        return false;
    }
    const int bytes = n * n * n * 4;
    if(pbos_[0] != 0 && pbo_bytes_ == bytes)
    {
        return true;
    }
    destroyPbos();
    xf->glGenBuffers(2, pbos_);
    if(pbos_[0] == 0 || pbos_[1] == 0)
    {
        pbos_[0] = pbos_[1] = 0;
        return false;
    }
    for(int i = 0; i < 2; ++i)
    {
        xf->glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos_[i]);
        xf->glBufferData(GL_PIXEL_PACK_BUFFER, bytes, nullptr, GL_STREAM_READ);
    }
    xf->glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    pbo_bytes_ = bytes;
    pbo_write_idx_ = 0;
    pbo_has_pending_ = false;
    pbo_pending_n_ = 0;
    readback_staging_.resize((size_t)bytes);
    return true;
}

void SpatialVolumeFieldEngine::destroyMediaTexture()
{
    if(media_tex_id_ == 0)
    {
        return;
    }
    if(context_ && context_->isValid())
    {
        QOpenGLFunctions* gl = context_->functions();
        if(gl)
        {
            gl->glDeleteTextures(1, &media_tex_id_);
        }
    }
    media_tex_id_ = 0;
    media_tex_w_ = 0;
    media_tex_h_ = 0;
}

bool SpatialVolumeFieldEngine::uploadMediaTexture()
{
    QOpenGLFunctions* gl = context_->functions();
    if(!gl)
    {
        return false;
    }

    QImage upload = media_image_;
    if(upload.isNull() || upload.width() < 1 || upload.height() < 1)
    {
        upload = QImage(1, 1, QImage::Format_RGBA8888);
        upload.fill(qRgba(0, 0, 0, 255));
    }
    else if(upload.format() != QImage::Format_RGBA8888)
    {
        upload = upload.convertToFormat(QImage::Format_RGBA8888);
    }

    if(media_tex_id_ == 0)
    {
        gl->glGenTextures(1, &media_tex_id_);
        if(media_tex_id_ == 0)
        {
            last_error_ = QStringLiteral("Media texture allocation failed.");
            return false;
        }
    }

    gl->glBindTexture(GL_TEXTURE_2D, media_tex_id_);
    const GLint wrap = media_wrap_ ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, upload.width(), upload.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     upload.constBits());
    gl->glBindTexture(GL_TEXTURE_2D, 0);
    media_tex_w_ = upload.width();
    media_tex_h_ = upload.height();
    media_dirty_ = false;
    return true;
}

bool SpatialVolumeFieldEngine::renderAtlas()
{
    if(fragment_body_.trimmed().isEmpty())
    {
        last_error_ = QStringLiteral("Empty volume field body.");
        available_.store(false);
        return false;
    }

    if(!initGl())
    {
        available_.store(false);
        return false;
    }

    if(body_dirty_ || !program_)
    {
        if(!compileProgram(fragment_body_))
        {
            available_.store(false);
            return false;
        }
        body_dirty_ = false;
    }

    const int n = resolution_;
    if(size_dirty_ || !fbo_ || fbo_n_ != n)
    {
        if(!ensureFbo(n))
        {
            available_.store(false);
            return false;
        }
        size_dirty_ = false;
    }

    if(!context_->makeCurrent(surface_.get()))
    {
        last_error_ = QStringLiteral("OpenGL makeCurrent failed.");
        available_.store(false);
        return false;
    }

    if(media_dirty_ || media_tex_id_ == 0)
    {
        if(!uploadMediaTexture())
        {
            context_->doneCurrent();
            available_.store(false);
            return false;
        }
    }

    QOpenGLFunctions* gl = context_->functions();
    float param_bins[kMaxParams] = {};
    for(int i = 0; i < kMaxParams; ++i)
    {
        param_bins[i] = params_.values[i];
    }

    program_->bind();
    fbo_->bind();
    gl->glViewport(0, 0, n, n * n);
    program_->setUniformValue("u_time", params_.time_sec);
    program_->setUniformValue("u_res", (float)n);
    program_->setUniformValue("u_atlas", QVector2D((float)n, (float)(n * n)));
    program_->setUniformValueArray("u_params", param_bins, kMaxParams, 1);

    gl->glActiveTexture(GL_TEXTURE0);
    gl->glBindTexture(GL_TEXTURE_2D, media_tex_id_);
    program_->setUniformValue("u_media", 0);

    static const float quad[] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};
    program_->enableAttributeArray("a_position");
    program_->setAttributeArray("a_position", GL_FLOAT, quad, 2);
    gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    program_->disableAttributeArray("a_position");

    gl->glBindTexture(GL_TEXTURE_2D, 0);

    readbackAtlas(n);

    fbo_->release();
    program_->release();
    context_->doneCurrent();

    params_dirty_ = false;
    last_error_.clear();
    available_.store(atlas_res_ > 0 && !atlas_rgb_.empty());
    return available_.load();
}

void SpatialVolumeFieldEngine::readbackAtlas(int n)
{
    QOpenGLFunctions* gl = context_->functions();
    QOpenGLExtraFunctions* xf = context_->extraFunctions();
    const int atlas_w = n;
    const int atlas_h = n * n;
    const size_t bytes = (size_t)atlas_w * (size_t)atlas_h * 4u;

    auto publish = [&](const unsigned char* rgba) {
        ConvertRgbaAtlasToRgb(rgba, n, atlas_rgb_);
        atlas_res_ = n;
    };

    gl->glPixelStorei(GL_PACK_ALIGNMENT, 1);

    if(xf && ensurePbos(n))
    {
        xf->glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos_[pbo_write_idx_]);
        gl->glReadPixels(0, 0, atlas_w, atlas_h, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        if(pbo_has_pending_ && pbo_pending_n_ == n)
        {
            const int read_idx = 1 - pbo_write_idx_;
            xf->glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos_[read_idx]);
            void* ptr = xf->glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, (GLsizeiptr)bytes, GL_MAP_READ_BIT);
            if(ptr)
            {
                if(readback_staging_.size() < bytes)
                {
                    readback_staging_.resize(bytes);
                }
                std::memcpy(readback_staging_.data(), ptr, bytes);
                xf->glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
                publish(readback_staging_.data());
            }
        }
        else
        {
            // First frame (or size change): sync so sampling has a valid atlas immediately.
            if(readback_staging_.size() < bytes)
            {
                readback_staging_.resize(bytes);
            }
            xf->glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
            gl->glReadPixels(0, 0, atlas_w, atlas_h, GL_RGBA, GL_UNSIGNED_BYTE, readback_staging_.data());
            publish(readback_staging_.data());
        }

        xf->glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        pbo_write_idx_ = 1 - pbo_write_idx_;
        pbo_has_pending_ = true;
        pbo_pending_n_ = n;
        return;
    }

    if(readback_staging_.size() < bytes)
    {
        readback_staging_.resize(bytes);
    }
    gl->glReadPixels(0, 0, atlas_w, atlas_h, GL_RGBA, GL_UNSIGNED_BYTE, readback_staging_.data());
    publish(readback_staging_.data());
}

QVector3D SpatialVolumeFieldEngine::sample01(float x, float y, float z) const
{
    // Lock-free: ensureReady must not run concurrently (same render thread).
    if(atlas_res_ < 2 || atlas_rgb_.empty())
    {
        return QVector3D(0.0f, 0.0f, 0.0f);
    }

    const int n = atlas_res_;
    const float fx = std::clamp(x, 0.0f, 1.0f) * (float)(n - 1);
    const float fy = std::clamp(y, 0.0f, 1.0f) * (float)(n - 1);
    const float fz = std::clamp(z, 0.0f, 1.0f) * (float)(n - 1);

    const int x0 = (int)std::floor(fx);
    const int y0 = (int)std::floor(fy);
    const int z0 = (int)std::floor(fz);
    const int x1 = std::min(x0 + 1, n - 1);
    const int y1 = std::min(y0 + 1, n - 1);
    const int z1 = std::min(z0 + 1, n - 1);
    const float tx = fx - (float)x0;
    const float ty = fy - (float)y0;
    const float tz = fz - (float)z0;

    auto at = [&](int xi, int yi, int zi) -> QVector3D {
        const size_t idx = (((size_t)zi * (size_t)n + (size_t)yi) * (size_t)n + (size_t)xi) * 3u;
        return QVector3D(atlas_rgb_[idx], atlas_rgb_[idx + 1], atlas_rgb_[idx + 2]);
    };

    const QVector3D c000 = at(x0, y0, z0);
    const QVector3D c100 = at(x1, y0, z0);
    const QVector3D c010 = at(x0, y1, z0);
    const QVector3D c110 = at(x1, y1, z0);
    const QVector3D c001 = at(x0, y0, z1);
    const QVector3D c101 = at(x1, y0, z1);
    const QVector3D c011 = at(x0, y1, z1);
    const QVector3D c111 = at(x1, y1, z1);

    const QVector3D c00 = c000 * (1.0f - tx) + c100 * tx;
    const QVector3D c10 = c010 * (1.0f - tx) + c110 * tx;
    const QVector3D c01 = c001 * (1.0f - tx) + c101 * tx;
    const QVector3D c11 = c011 * (1.0f - tx) + c111 * tx;
    const QVector3D c0 = c00 * (1.0f - ty) + c10 * ty;
    const QVector3D c1 = c01 * (1.0f - ty) + c11 * ty;
    return c0 * (1.0f - tz) + c1 * tz;
}

float SpatialVolumeFieldEngine::sampleScalar01(float x, float y, float z) const
{
    return sample01(x, y, z).x();
}
