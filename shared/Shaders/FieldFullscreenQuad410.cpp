// SPDX-License-Identifier: GPL-2.0-only

#include "FieldFullscreenQuad410.h"

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>

#ifndef GL_TRIANGLE_STRIP
#include <qopengl.h>
#endif

namespace FieldFullscreenQuad410
{
namespace
{

GLuint g_vao = 0;
GLuint g_vbo = 0;
QOpenGLContext* g_create_context = nullptr;

const float kQuad[] = {
    -1.0f, -1.0f,
     1.0f, -1.0f,
    -1.0f,  1.0f,
     1.0f,  1.0f,
};

bool ensure(QOpenGLExtraFunctions* xf, QOpenGLContext* ctx)
{
    if(!xf || !ctx)
    {
        return false;
    }
    if(g_vao && g_create_context == ctx)
    {
        return true;
    }
    if(g_vao || g_vbo)
    {
        Destroy(ctx);
    }

    xf->glGenVertexArrays(1, &g_vao);
    xf->glGenBuffers(1, &g_vbo);
    if(!g_vao || !g_vbo)
    {
        Destroy(ctx);
        return false;
    }

    xf->glBindVertexArray(g_vao);
    xf->glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    xf->glBufferData(GL_ARRAY_BUFFER, sizeof(kQuad), kQuad, GL_STATIC_DRAW);
    xf->glEnableVertexAttribArray(0);
    xf->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, (GLsizei)(2 * sizeof(float)),
                              reinterpret_cast<const void*>(0));
    xf->glBindBuffer(GL_ARRAY_BUFFER, 0);
    xf->glBindVertexArray(0);
    g_create_context = ctx;
    return true;
}

} // namespace

const char* VertexShaderSource()
{
    return R"GLSL(
#version 410 core
layout(location = 0) in vec2 a_position;
void main()
{
    gl_Position = vec4(a_position, 0.0, 1.0);
}
)GLSL";
}

bool Draw(QOpenGLContext* ctx)
{
    if(!ctx)
    {
        return false;
    }
    QOpenGLExtraFunctions* xf = ctx->extraFunctions();
    if(!ensure(xf, ctx))
    {
        return false;
    }
    xf->glBindVertexArray(g_vao);
    xf->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    xf->glBindVertexArray(0);
    return true;
}

void Destroy(QOpenGLContext* ctx)
{
    QOpenGLExtraFunctions* xf = ctx ? ctx->extraFunctions() : nullptr;
    if(xf && g_create_context == ctx)
    {
        if(g_vbo)
        {
            xf->glDeleteBuffers(1, &g_vbo);
        }
        if(g_vao)
        {
            xf->glDeleteVertexArrays(1, &g_vao);
        }
    }
    /* Always drop CPU-side handles so a failed makeCurrent during pool reset
       cannot leave stale VAO/VBO ids bound to a dead context pointer. */
    g_vao = 0;
    g_vbo = 0;
    g_create_context = nullptr;
}

} // namespace FieldFullscreenQuad410
