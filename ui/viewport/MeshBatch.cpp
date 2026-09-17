// SPDX-License-Identifier: GPL-2.0-only

#include "MeshBatch.h"

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>

namespace
{
QOpenGLExtraFunctions* Extra()
{
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    return ctx ? ctx->extraFunctions() : nullptr;
}

GLenum ToGlPrimitive(MeshBatch::Primitive primitive)
{
    switch(primitive)
    {
        case MeshBatch::Primitive::Lines:     return GL_LINES;
        case MeshBatch::Primitive::Triangles: return GL_TRIANGLES;
        case MeshBatch::Primitive::Points:    return GL_POINTS;
    }
    return GL_TRIANGLES;
}
} // namespace

MeshBatch::~MeshBatch()
{
    Destroy();
}

MeshBatch::MeshBatch(MeshBatch&& other) noexcept
    : vao_(other.vao_)
    , vbo_(other.vbo_)
    , vertex_count_(other.vertex_count_)
    , layout_(other.layout_)
{
    other.vao_ = 0;
    other.vbo_ = 0;
    other.vertex_count_ = 0;
}

MeshBatch& MeshBatch::operator=(MeshBatch&& other) noexcept
{
    if(this != &other)
    {
        Destroy();
        vao_ = other.vao_;
        vbo_ = other.vbo_;
        vertex_count_ = other.vertex_count_;
        layout_ = other.layout_;
        other.vao_ = 0;
        other.vbo_ = 0;
        other.vertex_count_ = 0;
    }
    return *this;
}

void MeshBatch::Destroy()
{
    QOpenGLExtraFunctions* xf = Extra();
    if(xf)
    {
        if(vbo_)
        {
            xf->glDeleteBuffers(1, &vbo_);
        }
        if(vao_)
        {
            xf->glDeleteVertexArrays(1, &vao_);
        }
    }
    Abandon();
}

void MeshBatch::Abandon()
{
    vao_ = 0;
    vbo_ = 0;
    vertex_count_ = 0;
}

bool MeshBatch::Upload(Layout layout, const float* interleaved, size_t vertex_count)
{
    QOpenGLExtraFunctions* xf = Extra();
    if(!xf || !interleaved || vertex_count == 0)
    {
        return false;
    }

    if(!vao_)
    {
        xf->glGenVertexArrays(1, &vao_);
    }
    if(!vbo_)
    {
        xf->glGenBuffers(1, &vbo_);
    }
    if(!vao_ || !vbo_)
    {
        Destroy();
        return false;
    }

    layout_ = layout;
    vertex_count_ = vertex_count;
    const int stride_floats = (layout == Layout::PosColorUv) ? 8 : 6;
    const GLsizeiptr bytes = (GLsizeiptr)(vertex_count * (size_t)stride_floats * sizeof(float));

    xf->glBindVertexArray(vao_);
    xf->glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    xf->glBufferData(GL_ARRAY_BUFFER, bytes, interleaved, GL_DYNAMIC_DRAW);

    const GLsizei stride = (GLsizei)(stride_floats * (int)sizeof(float));
    xf->glEnableVertexAttribArray(0);
    xf->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(0));
    xf->glEnableVertexAttribArray(1);
    xf->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(3 * sizeof(float)));
    if(layout == Layout::PosColorUv)
    {
        xf->glEnableVertexAttribArray(2);
        xf->glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(6 * sizeof(float)));
    }
    else
    {
        xf->glDisableVertexAttribArray(2);
    }

    xf->glBindBuffer(GL_ARRAY_BUFFER, 0);
    xf->glBindVertexArray(0);
    return true;
}

void MeshBatch::Draw(Primitive primitive) const
{
    QOpenGLExtraFunctions* xf = Extra();
    if(!xf || !IsValid())
    {
        return;
    }
    xf->glBindVertexArray(vao_);
    xf->glDrawArrays(ToGlPrimitive(primitive), 0, (GLsizei)vertex_count_);
    xf->glBindVertexArray(0);
}
