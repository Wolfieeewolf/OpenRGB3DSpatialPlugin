// SPDX-License-Identifier: GPL-2.0-only

#include "ViewportPrograms.h"

#include <QMatrix4x4>
#include <QOpenGLContext>
#include <QOpenGLShader>

#include <cstring>

namespace
{
const char* VertexShaderGlsl120()
{
    return
        "#version 120\n"
        "attribute vec3 a_pos;\n"
        "attribute vec3 a_color;\n"
        "uniform mat4 u_mvp;\n"
        "uniform float u_point_size;\n"
        "varying vec3 v_color;\n"
        "void main() {\n"
        "    gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
        "    gl_PointSize = u_point_size;\n"
        "    v_color = a_color;\n"
        "}\n";
}

const char* FragmentShaderGlsl120()
{
    return
        "#version 120\n"
        "varying vec3 v_color;\n"
        "uniform float u_alpha;\n"
        "void main() {\n"
        "    gl_FragColor = vec4(v_color, u_alpha);\n"
        "}\n";
}

const char* VertexShaderGlsl330()
{
    return
        "#version 330 core\n"
        "layout(location = 0) in vec3 a_pos;\n"
        "layout(location = 1) in vec3 a_color;\n"
        "uniform mat4 u_mvp;\n"
        "uniform float u_point_size;\n"
        "out vec3 v_color;\n"
        "void main() {\n"
        "    gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
        "    gl_PointSize = u_point_size;\n"
        "    v_color = a_color;\n"
        "}\n";
}

const char* FragmentShaderGlsl330()
{
    return
        "#version 330 core\n"
        "in vec3 v_color;\n"
        "out vec4 fragColor;\n"
        "uniform float u_alpha;\n"
        "void main() {\n"
        "    fragColor = vec4(v_color, u_alpha);\n"
        "}\n";
}

const char* TexturedVertexShaderGlsl120()
{
    return
        "#version 120\n"
        "attribute vec3 a_pos;\n"
        "attribute vec2 a_uv;\n"
        "uniform mat4 u_mvp;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "    gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
        "    v_uv = a_uv;\n"
        "}\n";
}

const char* TexturedFragmentShaderGlsl120()
{
    return
        "#version 120\n"
        "uniform sampler2D u_texture;\n"
        "uniform float u_alpha;\n"
        "varying vec2 v_uv;\n"
        "void main() {\n"
        "    vec4 tex = texture2D(u_texture, v_uv);\n"
        "    gl_FragColor = vec4(tex.rgb, tex.a * u_alpha);\n"
        "}\n";
}

const char* TexturedVertexShaderGlsl330()
{
    return
        "#version 330 core\n"
        "layout(location = 0) in vec3 a_pos;\n"
        "layout(location = 2) in vec2 a_uv;\n"
        "uniform mat4 u_mvp;\n"
        "out vec2 v_uv;\n"
        "void main() {\n"
        "    gl_Position = u_mvp * vec4(a_pos, 1.0);\n"
        "    v_uv = a_uv;\n"
        "}\n";
}

const char* TexturedFragmentShaderGlsl330()
{
    return
        "#version 330 core\n"
        "uniform sampler2D u_texture;\n"
        "uniform float u_alpha;\n"
        "in vec2 v_uv;\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    vec4 tex = texture(u_texture, v_uv);\n"
        "    fragColor = vec4(tex.rgb, tex.a * u_alpha);\n"
        "}\n";
}
} // namespace

bool ViewportColoredProgram::initialize(QOpenGLContext* context)
{
    ready_ = false;
    program_.removeAllShaders();

    if(!context)
    {
        return false;
    }

    const bool core_profile = (context->format().profile() == QSurfaceFormat::CoreProfile
                               && context->format().majorVersion() >= 3);

    const char* vertex_src = core_profile ? VertexShaderGlsl330() : VertexShaderGlsl120();
    const char* fragment_src = core_profile ? FragmentShaderGlsl330() : FragmentShaderGlsl120();

    if(!program_.addShaderFromSourceCode(QOpenGLShader::Vertex, vertex_src))
    {
        return false;
    }
    if(!program_.addShaderFromSourceCode(QOpenGLShader::Fragment, fragment_src))
    {
        return false;
    }

    program_.bindAttributeLocation("a_pos", 0);
    program_.bindAttributeLocation("a_color", 1);

    if(!program_.link())
    {
        qWarning("OpenRGB3DSpatial: viewport colored shader link failed: %s",
                 program_.log().toUtf8().constData());
        return false;
    }

    u_mvp_ = program_.uniformLocation("u_mvp");
    u_point_size_ = program_.uniformLocation("u_point_size");
    u_alpha_ = program_.uniformLocation("u_alpha");
    ready_ = (u_mvp_ >= 0);
    return ready_;
}

bool ViewportColoredProgram::bind()
{
    if(!ready_)
    {
        return false;
    }
    return program_.bind();
}

void ViewportColoredProgram::release()
{
    program_.release();
}

void ViewportColoredProgram::setMvp(const float column_major_mvp[16])
{
    QMatrix4x4 mat;
    std::memcpy(mat.data(), column_major_mvp, 16 * sizeof(float));
    program_.setUniformValue(u_mvp_, mat);
}

void ViewportColoredProgram::setPointSize(float size_pixels)
{
    if(u_point_size_ >= 0)
    {
        program_.setUniformValue(u_point_size_, size_pixels);
    }
}

void ViewportColoredProgram::setAlpha(float alpha)
{
    if(u_alpha_ >= 0)
    {
        program_.setUniformValue(u_alpha_, alpha);
    }
}

bool ViewportTexturedProgram::initialize(QOpenGLContext* context)
{
    ready_ = false;
    program_.removeAllShaders();

    if(!context)
    {
        return false;
    }

    const bool core_profile = (context->format().profile() == QSurfaceFormat::CoreProfile
                               && context->format().majorVersion() >= 3);

    const char* vertex_src = core_profile ? TexturedVertexShaderGlsl330() : TexturedVertexShaderGlsl120();
    const char* fragment_src = core_profile ? TexturedFragmentShaderGlsl330() : TexturedFragmentShaderGlsl120();

    if(!program_.addShaderFromSourceCode(QOpenGLShader::Vertex, vertex_src))
    {
        return false;
    }
    if(!program_.addShaderFromSourceCode(QOpenGLShader::Fragment, fragment_src))
    {
        return false;
    }

    program_.bindAttributeLocation("a_pos", 0);
    if(core_profile)
    {
        program_.bindAttributeLocation("a_uv", 2);
    }
    else
    {
        program_.bindAttributeLocation("a_uv", 1);
    }

    if(!program_.link())
    {
        qWarning("OpenRGB3DSpatial: viewport textured shader link failed: %s",
                 program_.log().toUtf8().constData());
        return false;
    }

    u_mvp_ = program_.uniformLocation("u_mvp");
    u_alpha_ = program_.uniformLocation("u_alpha");
    u_texture_ = program_.uniformLocation("u_texture");
    ready_ = (u_mvp_ >= 0 && u_texture_ >= 0);
    return ready_;
}

bool ViewportTexturedProgram::bind()
{
    if(!ready_)
    {
        return false;
    }
    return program_.bind();
}

void ViewportTexturedProgram::release()
{
    program_.release();
}

void ViewportTexturedProgram::setMvp(const float column_major_mvp[16])
{
    QMatrix4x4 mat;
    std::memcpy(mat.data(), column_major_mvp, 16 * sizeof(float));
    program_.setUniformValue(u_mvp_, mat);
}

void ViewportTexturedProgram::setAlpha(float alpha)
{
    if(u_alpha_ >= 0)
    {
        program_.setUniformValue(u_alpha_, alpha);
    }
}

void ViewportTexturedProgram::setTextureUnit(int unit)
{
    if(u_texture_ >= 0)
    {
        program_.setUniformValue(u_texture_, unit);
    }
}
