// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialStripFieldEngine.h"

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>

#include <algorithm>
#include <cmath>

namespace
{

const char* kStripVertexShader = R"(attribute vec2 a_position;
void main() {
    gl_Position = vec4(a_position, 0.0, 1.0);
}
)";

QString BuildStripFragmentShader(const QString& user_body)
{
    return QStringLiteral(
               "#version 110\n"
               "uniform float u_time;\n"
               "uniform float u_width;\n"
               "uniform float u_params[16];\n"
               "void stripMain(out vec4 out_color, in float s01);\n")
           + user_body
           + QStringLiteral(
               "\nvoid main() {\n"
               "    float w = max(u_width, 1.0);\n"
               "    float s01 = clamp((floor(gl_FragCoord.x) + 0.5) / w, 0.0, 1.0);\n"
               "    vec4 c = vec4(0.0);\n"
               "    stripMain(c, s01);\n"
               "    gl_FragColor = vec4(clamp(c.rgb, 0.0, 1.0), 1.0);\n"
               "}\n");
}

} // namespace

SpatialStripFieldEngine::SpatialStripFieldEngine() = default;

SpatialStripFieldEngine::~SpatialStripFieldEngine()
{
    std::lock_guard<std::mutex> lock(mutex_);
    shutdownGl();
}

void SpatialStripFieldEngine::shutdownGl()
{
    if(context_ && surface_ && context_->isValid())
    {
        context_->makeCurrent(surface_.get());
        fbo_.reset();
        program_.reset();
        context_->doneCurrent();
    }
    else
    {
        fbo_.reset();
        program_.reset();
    }
    context_.reset();
    surface_.reset();
    fbo_w_ = 0;
    gl_ok_ = false;
}

void SpatialStripFieldEngine::setFragmentBody(const QString& glsl_body)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(fragment_body_ == glsl_body)
    {
        return;
    }
    fragment_body_ = glsl_body;
    body_dirty_ = true;
}

void SpatialStripFieldEngine::setWidth(int w)
{
    const int clamped = std::clamp(w, kMinWidth, kMaxWidth);
    std::lock_guard<std::mutex> lock(mutex_);
    if(width_ == clamped)
    {
        return;
    }
    width_ = clamped;
    size_dirty_ = true;
}

void SpatialStripFieldEngine::setParams(const Params& params)
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

QString SpatialStripFieldEngine::lastError() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return last_error_;
}

bool SpatialStripFieldEngine::ensureReady()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(!body_dirty_ && !params_dirty_ && !size_dirty_ && strip_w_ > 0 && !strip_r_.empty())
    {
        return available_.load();
    }
    return renderStrip();
}

bool SpatialStripFieldEngine::initGl()
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

bool SpatialStripFieldEngine::compileProgram(const QString& body)
{
    if(!context_->makeCurrent(surface_.get()))
    {
        last_error_ = QStringLiteral("OpenGL makeCurrent failed.");
        return false;
    }

    program_ = std::make_unique<QOpenGLShaderProgram>();
    if(!program_->addShaderFromSourceCode(QOpenGLShader::Vertex, kStripVertexShader))
    {
        last_error_ = program_->log();
        program_.reset();
        context_->doneCurrent();
        return false;
    }
    const QString frag = BuildStripFragmentShader(body);
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

bool SpatialStripFieldEngine::ensureFbo(int w)
{
    if(fbo_ && fbo_w_ == w && fbo_->isValid())
    {
        return true;
    }
    if(!context_->makeCurrent(surface_.get()))
    {
        last_error_ = QStringLiteral("OpenGL makeCurrent failed.");
        return false;
    }
    fbo_ = std::make_unique<QOpenGLFramebufferObject>(w, 1);
    fbo_w_ = w;
    const bool ok = fbo_->isValid();
    context_->doneCurrent();
    if(!ok)
    {
        last_error_ = QStringLiteral("Strip FBO allocation failed.");
        fbo_.reset();
        fbo_w_ = 0;
    }
    return ok;
}

bool SpatialStripFieldEngine::renderStrip()
{
    if(fragment_body_.trimmed().isEmpty())
    {
        last_error_ = QStringLiteral("Empty strip field body.");
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

    const int w = width_;
    if(size_dirty_ || !fbo_ || fbo_w_ != w)
    {
        if(!ensureFbo(w))
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

    QOpenGLFunctions* gl = context_->functions();
    float param_bins[kMaxParams] = {};
    for(int i = 0; i < kMaxParams; ++i)
    {
        param_bins[i] = params_.values[i];
    }

    program_->bind();
    fbo_->bind();
    gl->glViewport(0, 0, w, 1);
    program_->setUniformValue("u_time", params_.time_sec);
    program_->setUniformValue("u_width", (float)w);
    program_->setUniformValueArray("u_params", param_bins, kMaxParams, 1);

    static const float quad[] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};
    program_->enableAttributeArray("a_position");
    program_->setAttributeArray("a_position", GL_FLOAT, quad, 2);
    gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    program_->disableAttributeArray("a_position");

    readbackStrip(w);

    fbo_->release();
    program_->release();
    context_->doneCurrent();

    params_dirty_ = false;
    last_error_.clear();
    available_.store(true);
    return true;
}

void SpatialStripFieldEngine::readbackStrip(int w)
{
    QOpenGLFunctions* gl = context_->functions();
    std::vector<unsigned char> rgba((size_t)w * 4u);
    gl->glPixelStorei(GL_PACK_ALIGNMENT, 1);
    gl->glReadPixels(0, 0, w, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    strip_r_.resize((size_t)w);
    for(int x = 0; x < w; ++x)
    {
        strip_r_[(size_t)x] = rgba[(size_t)x * 4u] / 255.0f;
    }
    strip_w_ = w;
}

float SpatialStripFieldEngine::sample01(float s01) const
{
    // Lock-free: ensureReady must not run concurrently (same render thread).
    if(strip_w_ < 2 || strip_r_.empty())
    {
        return 0.0f;
    }
    const int n = strip_w_;
    const float fx = std::clamp(s01, 0.0f, 1.0f) * (float)(n - 1);
    const int x0 = (int)std::floor(fx);
    const int x1 = std::min(x0 + 1, n - 1);
    const float t = fx - (float)x0;
    // Cubic-ish smootherstep blend for nicer kernels than nearest/linear alone.
    const float u = t * t * (3.0f - 2.0f * t);
    return strip_r_[(size_t)x0] * (1.0f - u) + strip_r_[(size_t)x1] * u;
}

float SpatialStripFieldEngine::sampleKernelSigned(float s01) const
{
    return sample01(s01) * 2.0f - 1.0f;
}
