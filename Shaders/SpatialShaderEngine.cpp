// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialShaderEngine.h"
#include "SpatialOffscreenGlPool.h"
#include "GlslUniformArray.h"
#include "QtCompat.h"

#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QVector2D>

#include <algorithm>
#include <vector>

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
               "uniform vec2 u_resolution;\n"
               "uniform float u_params[8];\n"
               "void spatialMain(out vec4 out_color, in vec2 frag_coord);\n")
           + user_body
           + QStringLiteral(
               "\nvoid main() {\n"
               "    vec4 c = vec4(0.0);\n"
               "    spatialMain(c, gl_FragCoord.xy);\n"
               "    gl_FragColor = vec4(clamp(c.rgb, 0.0, 1.0), 1.0);\n"
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
    SpatialOffscreenGlPool::Session gl;
    if(gl)
    {
        fbo_.reset();
        program_.reset();
    }
}

void SpatialShaderEngine::setRenderSize(int width, int height)
{
    std::lock_guard<std::mutex> lock(mutex_);
    render_width_ = std::clamp(width, 32, 512);
    render_height_ = std::clamp(height, 32, 512);
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

bool SpatialShaderEngine::compileProgram(const QString& body)
{
    auto next = std::make_unique<QOpenGLShaderProgram>();
    if(!next->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader))
    {
        last_error_ = next->log();
        return false;
    }
    const QString frag = BuildFragmentShader(body);
    if(!next->addShaderFromSourceCode(QOpenGLShader::Fragment, frag.toUtf8()))
    {
        last_error_ = next->log();
        return false;
    }
    if(!next->link())
    {
        last_error_ = next->log();
        return false;
    }
    program_ = std::move(next);
    last_error_.clear();
    return true;
}

bool SpatialShaderEngine::renderFrame()
{
    if(fragment_body_.trimmed().isEmpty())
    {
        last_error_ = QStringLiteral("Empty shader body.");
        return false;
    }
    if(!SpatialOffscreenGlPool::hostContextReady())
    {
        last_error_ = QStringLiteral("Host GL not ready.");
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
    if(!glf)
    {
        last_error_ = QStringLiteral("OpenGL functions unavailable.");
        return false;
    }

    if(body_dirty_ || !program_)
    {
        if(!compileProgram(fragment_body_))
        {
            emit compileMessage(last_error_);
            return false;
        }
        emit compileMessage(QString());
        body_dirty_ = false;
    }

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

    static const float quad[] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};
    program_->enableAttributeArray("a_position");
    program_->setAttributeArray("a_position", GL_FLOAT, quad, 2);
    glf->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    program_->disableAttributeArray("a_position");

    std::vector<unsigned char> rgba((size_t)w * (size_t)h * 4u);
    glf->glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glf->glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

    fbo_->release();
    program_->release();

    QImage raw(rgba.data(), w, h, w * 4, QImage::Format_RGBA8888);
    latest_frame_ = OpenRGB3DUi::FlipImageVertical(raw.copy());
    params_dirty_ = false;
    return !latest_frame_.isNull();
}

bool SpatialShaderEngine::ensureReady()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if(!running_.load())
    {
        running_.store(true);
    }
    if(!body_dirty_ && !size_dirty_ && !params_dirty_ && !latest_frame_.isNull())
    {
        return true;
    }
    return renderFrame();
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
