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

/**
 * Make a mesh available under meshes_dir.
 * - Already under meshes_dir → return relative path (no write).
 * - Else copy once as the original filename if missing (never _1 clones).
 * - For .gltf/.obj, also copy Assimp-referenced texture / .bin sidecars (no renames).
 * - Prefer .glb (textures embedded). If that filename already exists → reuse it.
 * @return relative path under meshes_dir, or empty on failure.
 */
std::string InstallToMeshesDir(const std::string& meshes_dir,
                               const std::string& source_path,
                               std::string* error_out = nullptr);
}

#endif
