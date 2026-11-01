// SPDX-License-Identifier: GPL-2.0-only

#include "GlProgram.h"

#include <QMatrix4x4>
#include <QOpenGLContext>
#include <QOpenGLShaderProgram>

#include <cstring>

GlProgram::~GlProgram()
{
    Destroy();
}

GlProgram::GlProgram(GlProgram&& other) noexcept
    : program_(other.program_)
{
    other.program_ = nullptr;
}

GlProgram& GlProgram::operator=(GlProgram&& other) noexcept
{
    if(this != &other)
    {
        Destroy();
        program_ = other.program_;
        other.program_ = nullptr;
    }
    return *this;
}

bool GlProgram::Compile(const char* vertex_src, const char* fragment_src, QString* error_log)
{
    Destroy();

    if(!QOpenGLContext::currentContext())
    {
        if(error_log)
        {
            *error_log = QStringLiteral("No current OpenGL context");
        }
        return false;
    }

    program_ = new QOpenGLShaderProgram();
    if(!program_->addShaderFromSourceCode(QOpenGLShader::Vertex, vertex_src))
    {
        if(error_log)
        {
            *error_log = program_->log();
        }
        Destroy();
        return false;
    }
    if(!program_->addShaderFromSourceCode(QOpenGLShader::Fragment, fragment_src))
    {
        if(error_log)
        {
            *error_log = program_->log();
        }
        Destroy();
        return false;
    }
    if(!program_->link())
    {
        if(error_log)
        {
            *error_log = program_->log();
        }
        Destroy();
        return false;
    }
    return true;
}

void GlProgram::Destroy()
{
    delete program_;
    program_ = nullptr;
}

bool GlProgram::IsValid() const
{
    return program_ && program_->isLinked();
}

void GlProgram::Bind() const
{
    if(program_)
    {
        program_->bind();
    }
}

void GlProgram::Unbind() const
{
    if(program_)
    {
        program_->release();
    }
}

void GlProgram::SetUniformMat4(const char* name, const float* column_major_16) const
{
    if(!program_ || !column_major_16)
    {
        return;
    }
    const int loc = program_->uniformLocation(name);
    if(loc < 0)
    {
        return;
    }
    /* QMatrix4x4(const float*) expects row-major; ViewportMat4 is OpenGL column-major.
       Copy into data() (column-major storage) so setUniformValue uploads the real MVP. */
    QMatrix4x4 mat;
    std::memcpy(mat.data(), column_major_16, sizeof(float) * 16);
    program_->setUniformValue(loc, mat);
}

void GlProgram::SetUniform1i(const char* name, int value) const
{
    if(!program_)
    {
        return;
    }
    const int loc = program_->uniformLocation(name);
    if(loc < 0)
    {
        return;
    }
    program_->setUniformValue(loc, value);
}

void GlProgram::SetUniform1f(const char* name, float value) const
{
    if(!program_)
    {
        return;
    }
    const int loc = program_->uniformLocation(name);
    if(loc < 0)
    {
        return;
    }
    program_->setUniformValue(loc, value);
}

unsigned int GlProgram::Id() const
{
    return program_ ? program_->programId() : 0;
}
