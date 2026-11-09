// SPDX-License-Identifier: GPL-2.0-only

#ifndef MESHANIM_H
#define MESHANIM_H

#include "MeshImport.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * Node-TRS glTF/Assimp animation (no skins / morphs / path tracing).
 * Per-part identity, UVs, base/emissive textures, transmission, metallic/roughness (PBR-lite).
 */
namespace MeshAnim
{
struct Mat4
{
    float m[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
};

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Quat
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct NodeTRS
{
    Vec3 translation;
    Quat rotation;
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

struct ImageRGBA
{
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba; /* width*height*4 */
    std::string source_key;         /* Assimp path or absolute file for dedupe */

    bool empty() const { return width <= 0 || height <= 0 || rgba.size() < 4; }
};

enum class AlphaMode : int
{
    Opaque = 0,
    Mask = 1,
    Blend = 2
};

struct Material
{
    float base_r = 1.0f;
    float base_g = 1.0f;
    float base_b = 1.0f;
    float base_a = 1.0f;
    float emis_r = 0.0f;
    float emis_g = 0.0f;
    float emis_b = 0.0f;
    float emis_strength = 0.0f;
    int base_tex = -1; /* index into Scene::images */
    int emis_tex = -1;
    int normal_tex = -1;
    int mr_tex = -1; /* glTF metallicRoughness (G=rough, B=metal) */
    AlphaMode alpha_mode = AlphaMode::Opaque;
    float alpha_cutoff = 0.5f;
    bool double_sided = false;
    float base_uv_scale_u = 1.0f;
    float base_uv_scale_v = 1.0f;
    float base_uv_offset_u = 0.0f;
    float base_uv_offset_v = 0.0f;
    float base_uv_rotation = 0.0f; /* radians, counter-clockwise */
    float emis_uv_scale_u = 1.0f;
    float emis_uv_scale_v = 1.0f;
    float emis_uv_offset_u = 0.0f;
    float emis_uv_offset_v = 0.0f;
    float emis_uv_rotation = 0.0f;
    float normal_scale = 1.0f;
    /** KHR_materials_transmission (0 = opaque surface, 1 = full glass). */
    float transmission = 0.0f;
    /** glTF metallicFactor default is 1.0 when omitted. */
    float metallic = 1.0f;
    float roughness = 1.0f;
    /** KHR_materials_specular specularFactor (1 = full lobe, 0 = matte/unlit-ish). */
    float specular_factor = 1.0f;
    int occlusion_tex = -1;
    float occlusion_strength = 1.0f;

    bool isFullyTransparent() const
    {
        if(emis_tex >= 0 || emis_strength > 1e-3f)
        {
            return false;
        }
        return base_a < 0.01f && base_tex < 0;
    }
};

struct NodeMesh
{
    int node_index = -1;
    /** Stable id for JSON overrides: "nodeName/meshName" or "part_N". */
    std::string part_id;
    std::string display_name;
    int material_index = -1;
    /** Assimp diffuse / baseColor factor. */
    float base_r = 1.0f;
    float base_g = 1.0f;
    float base_b = 1.0f;
    float base_a = 1.0f;
    /** Local-space positions (9 floats per triangle). */
    std::vector<float> local_positions;
    /** UV0 matching positions (6 floats per triangle); 0 if missing. */
    std::vector<float> local_uvs;

    bool hasUvs() const
    {
        return local_uvs.size() == (local_positions.size() / 3) * 2
               && !local_uvs.empty();
    }
};

struct Vec3Track
{
    std::vector<float> times;
    std::vector<float> values; /* 3 per key */
};

struct QuatTrack
{
    std::vector<float> times;
    std::vector<float> values; /* 4 per key (xyzw) */
};

struct NodeClip
{
    int node_index = -1;
    Vec3Track translation;
    QuatTrack rotation;
    Vec3Track scale;
};

struct AssetSummary
{
    size_t mesh_count = 0;
    size_t material_count = 0;
    size_t texture_count = 0;
    size_t clip_count = 0;
    size_t triangle_count = 0;
    size_t vertex_count = 0;
    bool has_uvs = false;
    bool has_skins = false;
    bool has_morphs = false;
};

struct Scene
{
    std::vector<int> parent;          /* -1 = root */
    std::vector<NodeTRS> bind_local;
    std::vector<std::string> node_names;
    std::vector<NodeMesh> meshes;
    std::vector<NodeClip> clips;      /* first animation only */
    std::vector<Material> materials;
    std::vector<ImageRGBA> images;
    float duration_sec = 0.0f;
    MeshImport::TriangleMesh bind_pose;
    AssetSummary summary;

    bool empty() const { return meshes.empty() && bind_pose.empty(); }
    bool hasAnimation() const { return !clips.empty() && duration_sec > 1e-6f; }
};

bool Load(const std::string& path, Scene* out, std::string* error_out = nullptr);

/** Sample clips → node world matrices (no vertex bake). Cheap per-frame GPU draw path. */
void EvaluateNodeWorlds(const Scene& scene, float time_sec, std::vector<Mat4>* out_world);

/** Transform a point by a column-major Mat4. */
void TransformPoint(const Mat4& m, float x, float y, float z, float* ox, float* oy, float* oz);

/** Bilinear sample of an RGBA image (u,v Assimp/OpenGL: V=0 bottom). */
void SampleImageRGBA(const ImageRGBA& image, float u, float v,
                     float* r, float* g, float* b, float* a = nullptr);

/** Raw glTF KHR_texture_transform on Assimp-flipped mesh UVs. */
void ApplyGltfUvXform(float u, float v,
                      float scale_u, float scale_v,
                      float offset_u, float offset_v,
                      float rotation_rad,
                      float* ou, float* ov);

/** Sample material base colour (factor × optional base texture + UV xform). */
void SampleMaterialBase(const Scene& scene, int material_index, float u, float v,
                        float* r, float* g, float* b);

/** Sample emissive contribution at mesh UVs (factor × optional emis tex + UV xform).
    May exceed 1.0 when emissiveStrength is high (HDR). */
void SampleMaterialEmissive(const Scene& scene, int material_index, float u, float v,
                            float* r, float* g, float* b);

/** MeshUnlit-like look at UVs for Soft glow / LED bake (tonemapped to ~0..1).
    `bright` is viewport brightness scale (e.g. Brightness/100 * max(1, Emissive)). */
void SampleMaterialViewportLook(const Scene& scene, int material_index, float u, float v,
                                float bright, float* r, float* g, float* b);

class Cache
{
public:
    static Cache* instance();

    const Scene* GetOrLoad(const std::string& absolute_path,
                           std::string* error_out = nullptr);
    void Invalidate(const std::string& absolute_path);
    void Clear();

private:
    Cache() = default;
    std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<Scene>> cache_;
};
} // namespace MeshAnim

#endif
