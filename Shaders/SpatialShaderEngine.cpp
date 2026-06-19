// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialShaderEngine.h"

#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>

#include "QtCompat.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

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
               "uniform float u_audio[128];\n"
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
}

void SpatialShaderEngine::setTargetFps(int fps)
{
    target_fps = std::clamp(fps, 8, 60);
}

void SpatialShaderEngine::setRenderSize(int width, int height)
{
    render_width = std::clamp(width, 32, 512);
    render_height = std::clamp(height, 32, 512);
    size_dirty.store(true);
}

void SpatialShaderEngine::setFragmentBody(const QString& glsl_body)
{
    std::lock_guard<std::mutex> lock(state_mutex);
    fragment_body = glsl_body;
    source_dirty.store(true);
}

void SpatialShaderEngine::setUniforms(const SpatialShaderUniforms& uniforms)
{
    std::lock_guard<std::mutex> lock(state_mutex);
    uniform_values.time_sec = uniforms.time_sec;
    if(uniforms.audio_ptr && uniforms.audio_count > 0)
    {
        const int n = std::min(uniforms.audio_count, 128);
        for(int i = 0; i < n; ++i)
        {
            uniform_values.audio_bins[i] = uniforms.audio_ptr[i];
        }
        for(int i = n; i < 128; ++i)
        {
            uniform_values.audio_bins[i] = 0.0f;
        }
        uniform_values.audio_ptr = uniform_values.audio_bins;
        uniform_values.audio_count = n;
    }
    else
    {
        uniform_values.audio_ptr = nullptr;
        uniform_values.audio_count = 0;
    }
}

void SpatialShaderEngine::start()
{
    if(running.load())
    {
        return;
    }
    running.store(true);
    source_dirty.store(true);
    size_dirty.store(true);
    render_thread = std::thread(&SpatialShaderEngine::renderThreadMain, this);
}

void SpatialShaderEngine::stop()
{
    if(!running.load())
    {
        return;
    }
    running.store(false);
    if(render_thread.joinable())
    {
        render_thread.join();
    }
}

void SpatialShaderEngine::renderThreadMain()
{
    QOffscreenSurface surface;
    surface.create();

    QOpenGLContext context;
    context.setFormat(surface.format());
    if(!context.create())
    {
        emit compileMessage(QStringLiteral("OpenGL context creation failed."));
        running.store(false);
        return;
    }
    context.makeCurrent(&surface);

    QOpenGLFunctions* gl = context.functions();
    QOpenGLShaderProgram program;
    QOpenGLFramebufferObject* fbo = nullptr;
    int fbo_w = 0;
    int fbo_h = 0;

    auto ensure_fbo = [&](int w, int h) {
        if(fbo && fbo_w == w && fbo_h == h)
        {
            return;
        }
        delete fbo;
        fbo = new QOpenGLFramebufferObject(w, h);
        fbo_w = w;
        fbo_h = h;
        size_dirty.store(false);
    };

    const auto frame_sleep = [this]() {
        const int use_fps = std::max(8, target_fps);
        std::this_thread::sleep_for(std::chrono::microseconds(1000000 / use_fps));
    };

    while(running.load())
    {
        int w = render_width;
        int h = render_height;
        QString body;
        SpatialShaderUniforms locals;
        int audio_count = 0;
        float audio_bins[128] = {};
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            w = render_width;
            h = render_height;
            body = fragment_body;
            locals.time_sec = uniform_values.time_sec;
            audio_count = std::min(uniform_values.audio_count, 128);
            if(audio_count > 0)
            {
                for(int i = 0; i < audio_count; ++i)
                {
                    audio_bins[i] = uniform_values.audio_bins[i];
                }
            }
        }

        if(source_dirty.load() || !program.isLinked())
        {
            program.removeAllShaders();
            if(!program.addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader))
            {
                emit compileMessage(program.log());
                source_dirty.store(false);
                frame_sleep();
                continue;
            }
            const QString frag_src = BuildFragmentShader(body);
            if(!program.addShaderFromSourceCode(QOpenGLShader::Fragment, frag_src.toUtf8()))
            {
                emit compileMessage(program.log());
                source_dirty.store(false);
                frame_sleep();
                continue;
            }
            if(!program.link())
            {
                emit compileMessage(program.log());
            }
            else
            {
                emit compileMessage(QString());
            }
            source_dirty.store(false);
        }

        if(!program.isLinked())
        {
            frame_sleep();
            continue;
        }

        if(size_dirty.load())
        {
            ensure_fbo(w, h);
        }
        if(!fbo)
        {
            ensure_fbo(w, h);
        }

        program.bind();
        fbo->bind();
        gl->glViewport(0, 0, w, h);
        program.setUniformValue("u_time", locals.time_sec);
        program.setUniformValue("u_resolution", QVector2D((float)w, (float)h));
        if(audio_count > 0)
        {
            program.setUniformValueArray("u_audio", audio_bins, audio_count, 1);
        }

        static const float quad[] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};
        program.enableAttributeArray("a_position");
        program.setAttributeArray("a_position", GL_FLOAT, quad, 2);
        gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        program.disableAttributeArray("a_position");

        fbo->release();
        program.release();

        emit frameReady(OpenRGB3DUi::FlipImageVertical(fbo->toImage()));
        frame_sleep();
    }

    delete fbo;
    context.doneCurrent();
}
