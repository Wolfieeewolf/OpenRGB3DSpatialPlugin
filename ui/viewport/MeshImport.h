// SPDX-License-Identifier: GPL-2.0-only

#ifndef MESHIMPORT_H
#define MESHIMPORT_H

#include <string>
#include <vector>

namespace MeshImport
{
struct TriangleMesh
{
    std::vector<float> positions;
    float aabb_min[3] = {0.0f, 0.0f, 0.0f};
    float aabb_max[3] = {0.0f, 0.0f, 0.0f};

    bool empty() const { return positions.size() < 9; }
    size_t triangleCount() const { return positions.size() / 9; }
};

constexpr size_t kMaxTriangles = 750000;

std::string OpenFileFilter();
bool IsBlockedExtension(const std::string& path_or_ext);
bool LoadTriangleMesh(const std::string& path, TriangleMesh* out, std::string* error_out = nullptr);
void AabbExtents(const TriangleMesh& mesh, float* out_sx, float* out_sy, float* out_sz);
void AabbCenter(const TriangleMesh& mesh, float* out_cx, float* out_cy, float* out_cz);
}

#endif
