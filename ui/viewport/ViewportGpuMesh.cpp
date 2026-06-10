// SPDX-License-Identifier: GPL-2.0-only

#include "ViewportGpuMesh.h"

#include <QOpenGLFunctions>

#include "ViewportLegacyGL.h"

void ViewportGpuMesh::destroy(QOpenGLFunctions* gl)
{
    if(!gl)
    {
        vertex_count_ = 0;
        return;
    }
    if(vbo_pos_ != 0)
    {
        gl->glDeleteBuffers(1, &vbo_pos_);
        vbo_pos_ = 0;
    }
    if(vbo_col_ != 0)
    {
        gl->glDeleteBuffers(1, &vbo_col_);
        vbo_col_ = 0;
    }
    vertex_count_ = 0;
}

void ViewportGpuMesh::upload(QOpenGLFunctions* gl, const std::vector<float>& positions, const std::vector<float>& colors)
{
    if(!gl)
    {
        return;
    }

    const int vertex_count = (int)positions.size() / 3;
    if(vertex_count <= 0 || colors.size() != positions.size())
    {
        destroy(gl);
        return;
    }

    if(vbo_pos_ == 0)
    {
        gl->glGenBuffers(1, &vbo_pos_);
        gl->glGenBuffers(1, &vbo_col_);
    }

    vertex_count_ = vertex_count;

    gl->glBindBuffer(GL_ARRAY_BUFFER, vbo_pos_);
    gl->glBufferData(GL_ARRAY_BUFFER, (int)(positions.size() * sizeof(float)), positions.data(), GL_STATIC_DRAW);

    gl->glBindBuffer(GL_ARRAY_BUFFER, vbo_col_);
    gl->glBufferData(GL_ARRAY_BUFFER, (int)(colors.size() * sizeof(float)), colors.data(), GL_STATIC_DRAW);

    gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ViewportGpuMesh::draw(QOpenGLFunctions* gl, unsigned int primitive, bool core_profile) const
{
    if(!gl || !isValid())
    {
        return;
    }

    if(core_profile)
    {
        gl->glBindBuffer(GL_ARRAY_BUFFER, vbo_pos_);
        gl->glEnableVertexAttribArray(0);
        gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

        gl->glBindBuffer(GL_ARRAY_BUFFER, vbo_col_);
        gl->glEnableVertexAttribArray(1);
        gl->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

        gl->glDrawArrays(primitive, 0, vertex_count_);

        gl->glDisableVertexAttribArray(1);
        gl->glDisableVertexAttribArray(0);
        gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
        return;
    }

    gl->glBindBuffer(GL_ARRAY_BUFFER, vbo_pos_);
    glVertexPointer(3, GL_FLOAT, 0, nullptr);
    glEnableClientState(GL_VERTEX_ARRAY);

    gl->glBindBuffer(GL_ARRAY_BUFFER, vbo_col_);
    glColorPointer(3, GL_FLOAT, 0, nullptr);
    glEnableClientState(GL_COLOR_ARRAY);

    gl->glDrawArrays(primitive, 0, vertex_count_);

    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
}
