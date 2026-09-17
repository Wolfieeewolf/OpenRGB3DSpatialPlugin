// SPDX-License-Identifier: GPL-2.0-only

#ifndef MESHBATCH_H
#define MESHBATCH_H

#include <cstddef>

/** CPU → VAO/VBO batch for unlit draws (lines / triangles / points). */
class MeshBatch
{
public:
    enum class Layout
    {
        PosColor,   // 6 floats/vert: xyz + rgb
        PosColorUv, // 8 floats/vert: xyz + rgb + uv
    };

    enum class Primitive
    {
        Lines,
        Triangles,
        Points,
    };

    MeshBatch() = default;
    ~MeshBatch();

    MeshBatch(const MeshBatch&) = delete;
    MeshBatch& operator=(const MeshBatch&) = delete;
    MeshBatch(MeshBatch&& other) noexcept;
    MeshBatch& operator=(MeshBatch&& other) noexcept;

    void Destroy();
    /** Drop VAO/VBO ids without glDelete (context lost / recreated). */
    void Abandon();
    bool IsValid() const { return vao_ != 0 && vertex_count_ > 0; }

    bool Upload(Layout layout, const float* interleaved, size_t vertex_count);
    void Draw(Primitive primitive) const;

private:
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    size_t vertex_count_ = 0;
    Layout layout_ = Layout::PosColor;
};

#endif
