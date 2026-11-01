// SPDX-License-Identifier: GPL-2.0-only

#include "MeshImport.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <sstream>

namespace MeshImport
{
namespace
{
constexpr float kEps = 1e-5f;

std::string LowerExtension(const std::string& path)
{
    std::string ext;
    const size_t slash = path.find_last_of("/\\");
    const size_t dot = path.find_last_of('.');
    if(dot == std::string::npos || (slash != std::string::npos && dot < slash))
    {
        return ext;
    }
    ext = path.substr(dot + 1);
    for(char& c : ext)
    {
        c = (char)std::tolower((unsigned char)c);
    }
    return ext;
}

bool ExtEquals(const std::string& ext, const char* needle)
{
    return ext == needle;
}

void AppendMeshTriangles(const aiMesh* mesh, const aiMatrix4x4& xform, TriangleMesh* out)
{
    if(!mesh || !out || !mesh->HasPositions() || mesh->mNumFaces == 0)
    {
        return;
    }

    for(unsigned int f = 0; f < mesh->mNumFaces; ++f)
    {
        const aiFace& face = mesh->mFaces[f];
        if(face.mNumIndices < 3)
        {
            continue;
        }
        for(unsigned int i = 1; i + 1 < face.mNumIndices; ++i)
        {
            const unsigned int idx[3] = {
                face.mIndices[0],
                face.mIndices[i],
                face.mIndices[i + 1],
            };
            for(unsigned int v = 0; v < 3; ++v)
            {
                const aiVector3D& p = mesh->mVertices[idx[v]];
                aiVector3D tp = xform * p;
                out->positions.push_back(tp.x);
                out->positions.push_back(tp.y);
                out->positions.push_back(tp.z);
            }
        }
    }
}

void WalkNode(const aiScene* scene, const aiNode* node, const aiMatrix4x4& parent, TriangleMesh* out)
{
    if(!scene || !node || !out)
    {
        return;
    }
    const aiMatrix4x4 xform = parent * node->mTransformation;
    for(unsigned int i = 0; i < node->mNumMeshes; ++i)
    {
        const unsigned int mesh_index = node->mMeshes[i];
        if(mesh_index >= scene->mNumMeshes)
        {
            continue;
        }
        AppendMeshTriangles(scene->mMeshes[mesh_index], xform, out);
        if(out->triangleCount() > kMaxTriangles)
        {
            return;
        }
    }
    for(unsigned int c = 0; c < node->mNumChildren; ++c)
    {
        WalkNode(scene, node->mChildren[c], xform, out);
        if(out->triangleCount() > kMaxTriangles)
        {
            return;
        }
    }
}

void RecomputeAabb(TriangleMesh* mesh)
{
    if(!mesh || mesh->positions.size() < 3)
    {
        return;
    }
    float min_x = FLT_MAX, min_y = FLT_MAX, min_z = FLT_MAX;
    float max_x = -FLT_MAX, max_y = -FLT_MAX, max_z = -FLT_MAX;
    for(size_t i = 0; i + 2 < mesh->positions.size(); i += 3)
    {
        const float x = mesh->positions[i];
        const float y = mesh->positions[i + 1];
        const float z = mesh->positions[i + 2];
        min_x = std::min(min_x, x); max_x = std::max(max_x, x);
        min_y = std::min(min_y, y); max_y = std::max(max_y, y);
        min_z = std::min(min_z, z); max_z = std::max(max_z, z);
    }
    mesh->aabb_min[0] = min_x; mesh->aabb_min[1] = min_y; mesh->aabb_min[2] = min_z;
    mesh->aabb_max[0] = max_x; mesh->aabb_max[1] = max_y; mesh->aabb_max[2] = max_z;
}
} // namespace

bool IsBlockedExtension(const std::string& path_or_ext)
{
    std::string ext = LowerExtension(path_or_ext);
    if(ext.empty())
    {
        ext = path_or_ext;
        for(char& c : ext)
        {
            c = (char)std::tolower((unsigned char)c);
        }
        if(!ext.empty() && ext.front() == '.')
        {
            ext.erase(ext.begin());
        }
    }
    return ExtEquals(ext, "blend")
        || ExtEquals(ext, "blend1")
        || ExtEquals(ext, "ifc")
        || ExtEquals(ext, "ifczip");
}

std::string OpenFileFilter()
{
    return "3D meshes (*.obj *.stl *.3mf *.glb *.gltf);;"
           "Wavefront OBJ (*.obj);;"
           "STL (*.stl);;"
           "3MF (*.3mf);;"
           "glTF (*.glb *.gltf);;"
           "All Files (*)";
}

bool LoadTriangleMesh(const std::string& path, TriangleMesh* out, std::string* error_out)
{
    if(!out)
    {
        return false;
    }
    out->positions.clear();

    if(IsBlockedExtension(path))
    {
        if(error_out)
        {
            *error_out =
                "Native Blender (.blend) and IFC files are not supported — "
                "Assimp's importers for these can hang or crash. "
                "Export from Blender as glTF (.glb), FBX, or OBJ and import that.";
        }
        return false;
    }

    Assimp::Importer importer;
    const unsigned int flags =
        aiProcess_Triangulate
      | aiProcess_JoinIdenticalVertices
      | aiProcess_SortByPType;

    const aiScene* scene = importer.ReadFile(path, flags);
    if(!scene || !scene->mRootNode
       || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
       || scene->mNumMeshes == 0)
    {
        if(error_out)
        {
            *error_out = importer.GetErrorString();
            if(error_out->empty())
            {
                *error_out = "Assimp failed to load mesh";
            }
        }
        return false;
    }

    aiMatrix4x4 identity;
    WalkNode(scene, scene->mRootNode, identity, out);
    if(out->triangleCount() > kMaxTriangles)
    {
        out->positions.clear();
        if(error_out)
        {
            std::ostringstream oss;
            oss << "Mesh has too many triangles (limit "
                << kMaxTriangles
                << "). Simplify or export a lower-poly mesh.";
            *error_out = oss.str();
        }
        return false;
    }
    if(out->empty())
    {
        if(error_out)
        {
            *error_out = "Mesh contained no triangles";
        }
        return false;
    }

    RecomputeAabb(out);
    return true;
}

void AabbExtents(const TriangleMesh& mesh, float* out_sx, float* out_sy, float* out_sz)
{
    const float sx = std::max(mesh.aabb_max[0] - mesh.aabb_min[0], kEps);
    const float sy = std::max(mesh.aabb_max[1] - mesh.aabb_min[1], kEps);
    const float sz = std::max(mesh.aabb_max[2] - mesh.aabb_min[2], kEps);
    if(out_sx) *out_sx = sx;
    if(out_sy) *out_sy = sy;
    if(out_sz) *out_sz = sz;
}

void AabbCenter(const TriangleMesh& mesh, float* out_cx, float* out_cy, float* out_cz)
{
    if(out_cx) *out_cx = 0.5f * (mesh.aabb_min[0] + mesh.aabb_max[0]);
    if(out_cy) *out_cy = 0.5f * (mesh.aabb_min[1] + mesh.aabb_max[1]);
    if(out_cz) *out_cz = 0.5f * (mesh.aabb_min[2] + mesh.aabb_max[2]);
}
} // namespace MeshImport
