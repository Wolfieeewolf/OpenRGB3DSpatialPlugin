// SPDX-License-Identifier: GPL-2.0-only

#ifndef VIEWPORTPROGRAMS_H
#define VIEWPORTPROGRAMS_H

#include <QOpenGLShaderProgram>

class QOpenGLContext;

/** Colored position shader for viewport lines and points (compat GLSL 1.20 / core 3.3). */
class ViewportColoredProgram
{
public:
    bool initialize(QOpenGLContext* context);
    bool isReady() const { return ready_; }

    bool bind();
    void release();
    void setMvp(const float column_major_mvp[16]);
    void setPointSize(float size_pixels);
    void setAlpha(float alpha);

private:
    bool ready_ = false;
    QOpenGLShaderProgram program_;
    int u_mvp_ = -1;
    int u_point_size_ = -1;
    int u_alpha_ = -1;
};

/** Textured RGBA quads for display-plane screen preview. */
class ViewportTexturedProgram
{
public:
    bool initialize(QOpenGLContext* context);
    bool isReady() const { return ready_; }

    bool bind();
    void release();
    void setMvp(const float column_major_mvp[16]);
    void setAlpha(float alpha);
    void setTextureUnit(int unit);

private:
    bool ready_ = false;
    QOpenGLShaderProgram program_;
    int u_mvp_ = -1;
    int u_alpha_ = -1;
    int u_texture_ = -1;
};

#endif
