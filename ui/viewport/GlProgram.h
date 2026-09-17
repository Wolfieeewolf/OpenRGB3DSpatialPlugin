// SPDX-License-Identifier: GPL-2.0-only

#ifndef GLPROGRAM_H
#define GLPROGRAM_H

#include <QString>

class QOpenGLShaderProgram;

/** Thin GLSL program wrapper (column-major uniforms, layout-location attrs). */
class GlProgram
{
public:
    GlProgram() = default;
    ~GlProgram();

    GlProgram(const GlProgram&) = delete;
    GlProgram& operator=(const GlProgram&) = delete;
    GlProgram(GlProgram&& other) noexcept;
    GlProgram& operator=(GlProgram&& other) noexcept;

    bool Compile(const char* vertex_src, const char* fragment_src, QString* error_log = nullptr);
    void Destroy();
    bool IsValid() const;

    void Bind() const;
    void Unbind() const;

    void SetUniformMat4(const char* name, const float* column_major_16) const;
    void SetUniform1i(const char* name, int value) const;
    void SetUniform1f(const char* name, float value) const;

    unsigned int Id() const;

private:
    QOpenGLShaderProgram* program_ = nullptr;
};

#endif
