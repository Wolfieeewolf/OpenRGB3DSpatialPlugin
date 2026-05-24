// SPDX-License-Identifier: GPL-2.0-only

#ifndef VIEWPORTGPUMESH_H
#define VIEWPORTGPUMESH_H

#include <vector>

class QOpenGLFunctions;

/** GPU VBO pair for interleaved-style separate position/color attributes. */
class ViewportGpuMesh
{
public:
    void destroy(QOpenGLFunctions* gl);
    void upload(QOpenGLFunctions* gl, const std::vector<float>& positions, const std::vector<float>& colors);
    bool isValid() const { return vertex_count_ > 0 && vbo_pos_ != 0; }
    int vertexCount() const { return vertex_count_; }

    void draw(QOpenGLFunctions* gl, unsigned int primitive, bool core_profile) const;

private:
    unsigned int vbo_pos_ = 0;
    unsigned int vbo_col_ = 0;
    int vertex_count_ = 0;
};

#endif
