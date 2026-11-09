// SPDX-License-Identifier: GPL-2.0-only

#include "filesystem.h"
#include "LEDViewport3D.h"
#include "SceneProp3D.h"
#include "GridSpaceUtils.h"
#include "viewport/MeshAnim.h"
#include "viewport/MeshGeometry.h"
#include "viewport/MeshImport.h"
#include "viewport/ScenePropMeshCache.h"
#include "viewport/ScenePropMeshPaths.h"
#include "viewport/ViewportMath.h"

#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QTimer>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
void UnpackRgb(unsigned int rgb, float* r, float* g, float* b)
{
    *r = ((rgb >> 16) & 0xFF) / 255.0f;
    *g = ((rgb >> 8) & 0xFF) / 255.0f;
    *b = (rgb & 0xFF) / 255.0f;
}

/* Scale mesh verts with a shared AABB fit (scene-prop meshes; Animated Mesh is GPU-only). */
void AppendScaledMeshPosColorFit(std::vector<float>& out,
                                 const MeshImport::TriangleMesh& mesh,
                                 float cx, float cy, float cz,
                                 float scale_x, float scale_y, float scale_z,
                                 float r, float g, float b,
                                 size_t* tris_budget)
{
    if(!tris_budget || *tris_budget == 0 || mesh.empty())
    {
        return;
    }
    const size_t tri_count = mesh.triangleCount();
    const size_t step = (tri_count > *tris_budget)
                            ? std::max<size_t>(1, tri_count / *tris_budget)
                            : 1;
    size_t emitted = 0;
    out.reserve(out.size() + (tri_count / step) * 18);
    for(size_t t = 0; t < tri_count && emitted < *tris_budget; t += step)
    {
        const size_t i = t * 9;
        if(i + 8 >= mesh.positions.size())
        {
            break;
        }
        for(size_t v = 0; v < 3; ++v)
        {
            const size_t vi = i + v * 3;
            const float x = (mesh.positions[vi] - cx) * scale_x;
            const float y = (mesh.positions[vi + 1] - cy) * scale_y;
            const float z = (mesh.positions[vi + 2] - cz) * scale_z;
            MeshGeometry::PushPosColor(out, x, y, z, r, g, b);
        }
        ++emitted;
    }
    if(emitted < *tris_budget)
    {
        *tris_budget -= emitted;
    }
    else
    {
        *tris_budget = 0;
    }
}

/* Scale mesh-local verts so AABB fits target half-extents (grid units), centered. */
void AppendScaledMeshPosColor(std::vector<float>& out,
                              const MeshImport::TriangleMesh& mesh,
                              float hw, float hh, float hd,
                              float r, float g, float b)
{
    float sx = 0.0f, sy = 0.0f, sz = 0.0f;
    float cx = 0.0f, cy = 0.0f, cz = 0.0f;
    MeshImport::AabbExtents(mesh, &sx, &sy, &sz);
    MeshImport::AabbCenter(mesh, &cx, &cy, &cz);
    const float scale_x = (2.0f * hw) / std::max(sx, 1e-6f);
    const float scale_y = (2.0f * hh) / std::max(sy, 1e-6f);
    const float scale_z = (2.0f * hd) / std::max(sz, 1e-6f);
    size_t budget = 200000;
    AppendScaledMeshPosColorFit(out, mesh, cx, cy, cz, scale_x, scale_y, scale_z,
                                r, g, b, &budget);
}

struct MeshDrawCacheEntry
{
    std::string path;
    float hw = 0.0f;
    float hh = 0.0f;
    float hd = 0.0f;
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    std::vector<float> tris;
};

std::unordered_map<int, MeshDrawCacheEntry>& MeshDrawCache()
{
    static std::unordered_map<int, MeshDrawCacheEntry> cache;
    return cache;
}

const std::vector<float>& CachedScaledMesh(int prop_id,
                                           const std::string& path,
                                           const MeshImport::TriangleMesh& mesh,
                                           float hw, float hh, float hd,
                                           float r, float g, float b)
{
    MeshDrawCacheEntry& entry = MeshDrawCache()[prop_id];
    if(entry.path == path
       && entry.hw == hw && entry.hh == hh && entry.hd == hd
       && entry.r == r && entry.g == g && entry.b == b
       && !entry.tris.empty())
    {
        return entry.tris;
    }
    entry.path = path;
    entry.hw = hw;
    entry.hh = hh;
    entry.hd = hd;
    entry.r = r;
    entry.g = g;
    entry.b = b;
    entry.tris.clear();
    AppendScaledMeshPosColor(entry.tris, mesh, hw, hh, hd, r, g, b);
    return entry.tris;
}
} // namespace

void LEDViewport3D::SetSceneProps(std::vector<std::unique_ptr<SceneProp3D>>* props)
{
    MeshDrawCache().clear();
    scene_props_ = props;
    if(!scene_props_)
    {
        selected_scene_prop_idx_ = -1;
    }
    else if(selected_scene_prop_idx_ >= (int)scene_props_->size())
    {
        selected_scene_prop_idx_ = -1;
    }
    UpdateGizmoPosition();
    SyncMeshAnimTimer();
    update();
}

void LEDViewport3D::SelectSceneProp(int index)
{
    if(scene_props_ && index >= 0 && index < (int)scene_props_->size())
    {
        deselectRoomViewport();
        gizmo.CancelDrag();
        effect_volume_active_ = false;
        effect_volume_grid_.reset();
        selected_controller_indices.clear();
        selected_controller_idx = -1;
        selected_ref_point_idx = -1;
        selected_display_plane_idx = -1;
        selected_scene_prop_idx_ = index;
        gizmo.SetTarget((*scene_props_)[(size_t)index].get());
        gizmo.SetGridSnap(grid_snap_enabled, 1.0f);
        UpdateGizmoPosition();
    }
    else
    {
        selected_scene_prop_idx_ = -1;
        if(selected_controller_idx < 0 && selected_ref_point_idx < 0 && selected_display_plane_idx < 0)
        {
            gizmo.SetTarget(static_cast<SceneProp3D*>(nullptr));
        }
    }
    update();
}

void LEDViewport3D::NotifyScenePropChanged()
{
    MeshDrawCache().clear();
    if(scene_props_ && selected_scene_prop_idx_ >= 0
       && selected_scene_prop_idx_ < (int)scene_props_->size())
    {
        SceneProp3D* prop = (*scene_props_)[(size_t)selected_scene_prop_idx_].get();
        if(prop)
        {
            const Transform3D& t = prop->GetTransform();
            emit ScenePropPositionChanged(selected_scene_prop_idx_,
                                          t.position.x, t.position.y, t.position.z);
            emit ScenePropRotationChanged(selected_scene_prop_idx_,
                                          t.rotation.x, t.rotation.y, t.rotation.z);
        }
    }
    UpdateGizmoPosition();
    SyncMeshAnimTimer();
    update();
}

void LEDViewport3D::SetMeshAnimOverlays(std::vector<MeshAnimOverlay> overlays)
{
    /* Called from the effect render tick — do not update() here (would recurse). */
    mesh_anim_overlays_ = std::move(overlays);
    SyncMeshAnimTimer();
}

unsigned int LEDViewport3D::EnsureMeshAnimTexture(const std::string& scene_path,
                                                   int image_index,
                                                   const MeshAnim::ImageRGBA& image)
{
    if(image.empty() || image_index < 0 || scene_path.empty())
    {
        return 0;
    }
    /* Bump with kMeshGpuCacheFormat when upload layout changes (e.g. V-flip). */
    const std::string key = scene_path + "#t7#" + std::to_string(image_index);
    auto it = mesh_anim_textures_.find(key);
    if(it != mesh_anim_textures_.end())
    {
        return it->second;
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    if(tex == 0)
    {
        return 0;
    }
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    /* Top-down ImageRGBA uploaded as-is. MeshUnlit converts Assimp UV→glTF UV
       (V=0 = image top). First GL row = image top works with that convention. */
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    mesh_anim_textures_[key] = tex;
    return tex;
}

void LEDViewport3D::ClearMeshAnimTextures()
{
    for(auto& kv : mesh_anim_textures_)
    {
        if(kv.second != 0)
        {
            glDeleteTextures(1, &kv.second);
        }
    }
    mesh_anim_textures_.clear();
}

void LEDViewport3D::ClearMeshAnimGpuCache()
{
    for(MeshAnimGpuPart& part : mesh_anim_gpu_.parts)
    {
        part.batch.Destroy();
    }
    mesh_anim_gpu_ = MeshAnimGpuCache{};
    ClearMeshAnimTextures();
}

namespace
{
std::string MeshAnimGpuFingerprint(const LEDViewport3D::MeshAnimOverlay& overlay)
{
    /* Bump when MeshUnlit / material upload layout changes so stale part
       batches (factor-only mats, UV xforms, occlusion) are rebuilt. */
    constexpr int kMeshGpuCacheFormat = 7;
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss.precision(4);
    oss << kMeshGpuCacheFormat << '|'
        << overlay.path << '|' << overlay.hw << ',' << overlay.hh << ',' << overlay.hd << '|';
    for(const LEDViewport3D::MeshAnimPartTint& pt : overlay.parts)
    {
        oss << pt.part_index << ':' << (pt.visible ? 1 : 0) << ';';
    }
    return oss.str();
}

ViewportMat4 Mat4ToViewport(const MeshAnim::Mat4& m)
{
    ViewportMat4 out;
    std::memcpy(out.m, m.m, sizeof(float) * 16);
    return out;
}

void ComputeBindFit(const MeshAnim::Scene& anim,
                    float hw, float hh, float hd,
                    float* cx, float* cy, float* cz,
                    float* scale_x, float* scale_y, float* scale_z)
{
    const MeshImport::TriangleMesh& bp = anim.bind_pose;
    float min_x = bp.aabb_min[0], min_y = bp.aabb_min[1], min_z = bp.aabb_min[2];
    float max_x = bp.aabb_max[0], max_y = bp.aabb_max[1], max_z = bp.aabb_max[2];
    if(bp.empty())
    {
        min_x = min_y = min_z = 0.0f;
        max_x = max_y = max_z = 1.0f;
    }
    *cx = 0.5f * (min_x + max_x);
    *cy = 0.5f * (min_y + max_y);
    *cz = 0.5f * (min_z + max_z);
    const float sx = std::max(max_x - min_x, 1e-6f);
    const float sy = std::max(max_y - min_y, 1e-6f);
    const float sz = std::max(max_z - min_z, 1e-6f);
    /* Uniform scale — keep spheres round (Sketchfab-style). Fill the volume on the
       limiting axis; letterbox the others instead of stretching. */
    const float s = std::min((2.0f * hw) / sx, std::min((2.0f * hh) / sy, (2.0f * hd) / sz));
    *scale_x = s;
    *scale_y = s;
    *scale_z = s;
}
} // namespace

void LEDViewport3D::EnsureMeshAnimGpuCache(const MeshAnimOverlay& overlay, const MeshAnim::Scene& anim)
{
    /* Synthesize part list when empty so we never need a CPU draw fallback. */
    std::vector<MeshAnimPartTint> parts = overlay.parts;
    if(parts.empty())
    {
        parts.reserve(anim.meshes.size());
        for(size_t i = 0; i < anim.meshes.size(); ++i)
        {
            MeshAnimPartTint pt;
            pt.part_index = (int)i;
            pt.visible = true;
            pt.r = overlay.r;
            pt.g = overlay.g;
            pt.b = overlay.b;
            parts.push_back(pt);
        }
    }

    MeshAnimOverlay fp_overlay = overlay;
    fp_overlay.parts = parts;
    const std::string fp = MeshAnimGpuFingerprint(fp_overlay);
    if(mesh_anim_gpu_.fingerprint == fp && !mesh_anim_gpu_.parts.empty())
    {
        return;
    }

    ClearMeshAnimGpuCache();
    if(parts.empty())
    {
        mesh_anim_gpu_.fingerprint = fp;
        return;
    }

    ComputeBindFit(anim, overlay.hw, overlay.hh, overlay.hd,
                   &mesh_anim_gpu_.fit_cx, &mesh_anim_gpu_.fit_cy, &mesh_anim_gpu_.fit_cz,
                   &mesh_anim_gpu_.scale_x, &mesh_anim_gpu_.scale_y, &mesh_anim_gpu_.scale_z);
    mesh_anim_gpu_.path = overlay.path;
    mesh_anim_gpu_.fingerprint = fp;

    /* Fair per-part budget — shared leftover budget starved large later parts into speckles. */
    constexpr size_t kMaxGpuTris = 400000;
    std::vector<int> visible_parts;
    size_t total_tris = 0;
    for(const MeshAnimPartTint& pt : parts)
    {
        if(!pt.visible || pt.part_index < 0 || pt.part_index >= (int)anim.meshes.size())
        {
            continue;
        }
        const MeshAnim::NodeMesh& nm = anim.meshes[(size_t)pt.part_index];
        if(nm.base_a < 0.01f)
        {
            continue; /* glTF "hide" / fully transparent materials */
        }
        if(nm.material_index >= 0 && nm.material_index < (int)anim.materials.size()
           && anim.materials[(size_t)nm.material_index].isFullyTransparent())
        {
            continue;
        }
        const size_t tri_count = nm.local_positions.size() / 9;
        if(tri_count == 0)
        {
            continue;
        }
        visible_parts.push_back(pt.part_index);
        total_tris += tri_count;
    }
    if(visible_parts.empty() || total_tris == 0)
    {
        return;
    }

    for(const MeshAnimPartTint& pt : parts)
    {
        if(!pt.visible || pt.part_index < 0 || pt.part_index >= (int)anim.meshes.size())
        {
            continue;
        }
        const MeshAnim::NodeMesh& nm = anim.meshes[(size_t)pt.part_index];
        if(nm.base_a < 0.01f)
        {
            continue;
        }
        if(nm.material_index >= 0 && nm.material_index < (int)anim.materials.size()
           && anim.materials[(size_t)nm.material_index].isFullyTransparent())
        {
            continue;
        }
        const size_t tri_count = nm.local_positions.size() / 9;
        if(tri_count == 0)
        {
            continue;
        }

        /* Proportional share of the global cap (min so tiny parts stay solid). */
        size_t part_budget = (kMaxGpuTris * tri_count) / total_tris;
        if(part_budget < 512)
        {
            part_budget = 512;
        }
        if(part_budget > tri_count)
        {
            part_budget = tri_count;
        }

        int base_tex = -1;
        int emis_tex = -1;
        int normal_tex = -1;
        int mr_tex = -1;
        int occlusion_tex = -1;
        const bool has_mat = nm.material_index >= 0
            && nm.material_index < (int)anim.materials.size();
        if(has_mat)
        {
            const MeshAnim::Material& mat = anim.materials[(size_t)nm.material_index];
            base_tex = mat.base_tex;
            emis_tex = mat.emis_tex;
            normal_tex = mat.normal_tex;
            mr_tex = mat.mr_tex;
            occlusion_tex = mat.occlusion_tex;
        }
        /* Always use MeshUnlit when a material exists (factors-only Sketchfab glass
           still needs PBR-lite / BLEND / double-sided even with no textures). */
        const bool use_mesh_shader = has_mat;
        const bool need_uv = use_mesh_shader;

        const size_t step = (tri_count > part_budget)
                                ? std::max<size_t>(1, tri_count / part_budget)
                                : 1;
        std::vector<float> interleaved;
        interleaved.reserve(std::min(tri_count, part_budget) * 3 * (need_uv ? 8u : 6u));
        size_t emitted = 0;
        /* Material path: keep vertex tint ~white so emissive HDR isn't crushed. */
        const float vr = use_mesh_shader ? 1.0f : pt.r;
        const float vg = use_mesh_shader ? 1.0f : pt.g;
        const float vb = use_mesh_shader ? 1.0f : pt.b;
        const bool has_real_uv = nm.hasUvs();
        for(size_t t = 0; t < tri_count && emitted < part_budget; t += step)
        {
            const size_t pi = t * 9;
            const size_t ui = t * 6;
            if(pi + 8 >= nm.local_positions.size())
            {
                break;
            }
            for(size_t v = 0; v < 3; ++v)
            {
                const size_t vi = pi + v * 3;
                const float x = nm.local_positions[vi];
                const float y = nm.local_positions[vi + 1];
                const float z = nm.local_positions[vi + 2];
                if(need_uv)
                {
                    float uu = 0.0f;
                    float vv = 0.0f;
                    if(has_real_uv && ui + v * 2 + 1 < nm.local_uvs.size())
                    {
                        uu = nm.local_uvs[ui + v * 2];
                        vv = nm.local_uvs[ui + v * 2 + 1];
                    }
                    MeshGeometry::PushPosColorUv(
                        interleaved, x, y, z, vr, vg, vb, uu, vv);
                }
                else
                {
                    MeshGeometry::PushPosColor(interleaved, x, y, z, vr, vg, vb);
                }
            }
            ++emitted;
        }
        if(emitted == 0)
        {
            continue;
        }

        MeshAnimGpuPart gpu;
        gpu.part_index = pt.part_index;
        gpu.node_index = nm.node_index;
        gpu.material_index = nm.material_index;
        gpu.base_tex = (base_tex >= 0 && base_tex < (int)anim.images.size()) ? base_tex : -1;
        gpu.emis_tex = (emis_tex >= 0 && emis_tex < (int)anim.images.size()) ? emis_tex : -1;
        gpu.normal_tex = (normal_tex >= 0 && normal_tex < (int)anim.images.size()) ? normal_tex : -1;
        gpu.mr_tex = (mr_tex >= 0 && mr_tex < (int)anim.images.size()) ? mr_tex : -1;
        gpu.occlusion_tex = (occlusion_tex >= 0 && occlusion_tex < (int)anim.images.size()) ? occlusion_tex : -1;
        gpu.use_mesh_shader = use_mesh_shader;
        gpu.r = vr;
        gpu.g = vg;
        gpu.b = vb;
        if(need_uv)
        {
            gpu.batch.Upload(MeshBatch::Layout::PosColorUv,
                             interleaved.data(), interleaved.size() / 8);
            if(gpu.base_tex >= 0)
            {
                EnsureMeshAnimTexture(overlay.path, gpu.base_tex, anim.images[(size_t)gpu.base_tex]);
            }
            if(gpu.emis_tex >= 0)
            {
                EnsureMeshAnimTexture(overlay.path, gpu.emis_tex, anim.images[(size_t)gpu.emis_tex]);
            }
            if(gpu.normal_tex >= 0)
            {
                EnsureMeshAnimTexture(overlay.path, gpu.normal_tex, anim.images[(size_t)gpu.normal_tex]);
            }
            if(gpu.mr_tex >= 0)
            {
                EnsureMeshAnimTexture(overlay.path, gpu.mr_tex, anim.images[(size_t)gpu.mr_tex]);
            }
            if(gpu.occlusion_tex >= 0)
            {
                EnsureMeshAnimTexture(overlay.path, gpu.occlusion_tex, anim.images[(size_t)gpu.occlusion_tex]);
            }
        }
        else
        {
            gpu.batch.Upload(MeshBatch::Layout::PosColor,
                             interleaved.data(), interleaved.size() / 6);
        }
        mesh_anim_gpu_.parts.push_back(std::move(gpu));
    }
}

void LEDViewport3D::DrawMeshAnimOverlaysGpu(const MeshAnimOverlay& overlay, const MeshAnim::Scene& anim)
{
    if(mesh_anim_gpu_.parts.empty())
    {
        return;
    }

    std::vector<MeshAnim::Mat4> worlds;
    const float t = (overlay.animate && anim.hasAnimation()) ? overlay.time_sec : 0.0f;
    MeshAnim::EvaluateNodeWorlds(anim, t, &worlds);

    const ViewportMat4 overlay_m = ViewportMath::FromTransform3D(overlay.transform);
    const ViewportMat4 fit = ViewportMath::Multiply(
        ViewportMath::Scale(mesh_anim_gpu_.scale_x, mesh_anim_gpu_.scale_y, mesh_anim_gpu_.scale_z),
        ViewportMath::Translation(-mesh_anim_gpu_.fit_cx, -mesh_anim_gpu_.fit_cy, -mesh_anim_gpu_.fit_cz));
    const ViewportMat4 overlay_fit = ViewportMath::Multiply(overlay_m, fit);

    /* Directional soft-shadow map from the same key light as MeshUnlit.
       Skip when every part is BLEND/glass — depth pass can't represent them and
       FBO thrash was leaving Sketchfab neon looking like Soft-glow soup only. */
    unsigned int shadow_tex = 0;
    bool any_opaque = false;
    for(const MeshAnimGpuPart& part : mesh_anim_gpu_.parts)
    {
        if(!part.use_mesh_shader || part.material_index < 0
           || part.material_index >= (int)anim.materials.size())
        {
            continue;
        }
        const MeshAnim::Material& mat = anim.materials[(size_t)part.material_index];
        if(mat.alpha_mode != MeshAnim::AlphaMode::Blend && !mat.isFullyTransparent())
        {
            any_opaque = true;
            break;
        }
    }
    const bool want_shadow = any_opaque && gl_prog_mesh_depth_.IsValid() && ensureMeshShadowFbo();
    ViewportMat4 light_view_proj = ViewportMath::Identity();
    if(want_shadow)
    {
        const float span = std::max({overlay.hw, overlay.hh, overlay.hd, 0.25f}) * 2.35f;
        const float cx = overlay.transform.position.x;
        const float cy = overlay.transform.position.y;
        const float cz = overlay.transform.position.z;
        const ViewportVec3 center{cx, cy, cz};
        const ViewportVec3 L{0.35f, 0.85f, 0.40f};
        const float Llen = std::sqrt(L.x * L.x + L.y * L.y + L.z * L.z);
        const float invL = (Llen > 1e-5f) ? (1.0f / Llen) : 1.0f;
        const ViewportVec3 eye{
            cx + L.x * invL * span * 1.6f,
            cy + L.y * invL * span * 1.6f,
            cz + L.z * invL * span * 1.6f};
        const ViewportMat4 light_view = ViewportMath::LookAt(eye, center, {0.0f, 1.0f, 0.0f});
        const ViewportMat4 light_proj = ViewportMath::Ortho(
            -span, span, -span, span, 0.05f, span * 4.0f);
        light_view_proj = ViewportMath::Multiply(light_proj, light_view);

        /* Restore whatever FBO was bound (HDR bloom scene, not always default). */
        GLint prev_fbo = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prev_fbo);
        GLint prev_vp[4] = {0, 0, 1, 1};
        glGetIntegerv(GL_VIEWPORT, prev_vp);
        GLfloat prev_clear[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        glGetFloatv(GL_COLOR_CLEAR_VALUE, prev_clear);

        mesh_shadow_fbo_->bind();
        glViewport(0, 0, kMeshShadowMapSize, kMeshShadowMapSize);
        /* Far depth = white in the depth-as-color map; must not leak to paintGL. */
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        for(const MeshAnimGpuPart& part : mesh_anim_gpu_.parts)
        {
            if(!part.batch.IsValid() || !part.use_mesh_shader)
            {
                continue;
            }
            if(part.material_index >= 0 && part.material_index < (int)anim.materials.size())
            {
                const MeshAnim::Material& mat = anim.materials[(size_t)part.material_index];
                if(mat.alpha_mode == MeshAnim::AlphaMode::Blend || mat.isFullyTransparent())
                {
                    continue;
                }
            }
            bool visible = true;
            for(const MeshAnimPartTint& pt : overlay.parts)
            {
                if(pt.part_index == part.part_index)
                {
                    visible = pt.visible;
                    break;
                }
            }
            if(!visible)
            {
                continue;
            }
            ViewportMat4 model = overlay_fit;
            if(part.node_index >= 0 && part.node_index < (int)worlds.size())
            {
                model = ViewportMath::Multiply(overlay_fit, Mat4ToViewport(worlds[(size_t)part.node_index]));
            }
            const ViewportMat4 light_mvp = ViewportMath::Multiply(light_view_proj, model);
            drawMeshDepthBatch(part.batch, light_mvp);
        }

        mesh_shadow_fbo_->release();
        if(QOpenGLContext* ctx = context())
        {
            if(QOpenGLExtraFunctions* xf = ctx->extraFunctions())
            {
                xf->glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prev_fbo);
            }
        }
        glViewport(prev_vp[0], prev_vp[1], prev_vp[2], prev_vp[3]);
        glClearColor(prev_clear[0], prev_clear[1], prev_clear[2], prev_clear[3]);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        shadow_tex = mesh_shadow_fbo_->texture();
    }

    for(const MeshAnimGpuPart& part : mesh_anim_gpu_.parts)
    {
        if(!part.batch.IsValid())
        {
            continue;
        }
        float tint_r = 1.0f, tint_g = 1.0f, tint_b = 1.0f;
        float part_emis = 1.0f;
        for(const MeshAnimPartTint& pt : overlay.parts)
        {
            if(pt.part_index == part.part_index && pt.visible)
            {
                part_emis = std::max(0.05f, pt.emis_scale);
                if(overlay.effect_tint)
                {
                    tint_r = std::max(0.05f, pt.r);
                    tint_g = std::max(0.05f, pt.g);
                    tint_b = std::max(0.05f, pt.b);
                }
                break;
            }
        }
        const float part_bright = overlay.bright_scale * part_emis;
        ViewportMat4 model = overlay_fit;
        if(part.node_index >= 0 && part.node_index < (int)worlds.size())
        {
            model = ViewportMath::Multiply(overlay_fit, Mat4ToViewport(worlds[(size_t)part.node_index]));
        }
        if(part.use_mesh_shader
           && part.material_index >= 0
           && part.material_index < (int)anim.materials.size())
        {
            const MeshAnim::Material& mat = anim.materials[(size_t)part.material_index];
            unsigned int base_id = 0;
            unsigned int emis_id = 0;
            unsigned int normal_id = 0;
            unsigned int mr_id = 0;
            unsigned int occ_id = 0;
            if(part.base_tex >= 0)
            {
                base_id = EnsureMeshAnimTexture(
                    overlay.path, part.base_tex, anim.images[(size_t)part.base_tex]);
            }
            if(part.emis_tex >= 0)
            {
                emis_id = EnsureMeshAnimTexture(
                    overlay.path, part.emis_tex, anim.images[(size_t)part.emis_tex]);
            }
            if(part.normal_tex >= 0)
            {
                normal_id = EnsureMeshAnimTexture(
                    overlay.path, part.normal_tex, anim.images[(size_t)part.normal_tex]);
            }
            if(part.mr_tex >= 0)
            {
                mr_id = EnsureMeshAnimTexture(
                    overlay.path, part.mr_tex, anim.images[(size_t)part.mr_tex]);
            }
            if(part.occlusion_tex >= 0)
            {
                occ_id = EnsureMeshAnimTexture(
                    overlay.path, part.occlusion_tex, anim.images[(size_t)part.occlusion_tex]);
            }
            const ViewportMat4 light_mvp = ViewportMath::Multiply(light_view_proj, model);
            drawMeshMaterialBatch(part.batch, mat, base_id, emis_id, normal_id, mr_id, occ_id,
                                  shadow_tex, shadow_tex != 0 ? &light_mvp : nullptr,
                                  overlay.a, part_bright,
                                  tint_r, tint_g, tint_b, &model);
            continue;
        }
        if(part.base_tex >= 0)
        {
            const unsigned int tex_id = EnsureMeshAnimTexture(
                overlay.path, part.base_tex, anim.images[(size_t)part.base_tex]);
            if(tex_id != 0)
            {
                drawTexturedBatch(part.batch, tex_id, overlay.a, &model);
                continue;
            }
        }
        drawUnlitBatch(part.batch, MeshBatch::Primitive::Triangles, 1.0f, overlay.a, &model);
    }
}

void LEDViewport3D::DrawMeshAnimOverlays()
{
    if(mesh_anim_overlays_.empty())
    {
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for(const MeshAnimOverlay& overlay : mesh_anim_overlays_)
    {
        if(overlay.path.empty() || overlay.hw <= 0.0f || overlay.hh <= 0.0f || overlay.hd <= 0.0f)
        {
            continue;
        }
        const MeshAnim::Scene* anim = MeshAnim::Cache::instance()->GetOrLoad(overlay.path);
        if(!anim)
        {
            continue;
        }

        /* GPU-only materials path. */
        EnsureMeshAnimGpuCache(overlay, *anim);
        if(!mesh_anim_gpu_.parts.empty())
        {
            DrawMeshAnimOverlaysGpu(overlay, *anim);
        }
    }
}

void LEDViewport3D::SyncMeshAnimTimer()
{
    /* Animation lives on Effects library Animated Mesh overlays only. */
    bool need = false;
    for(const MeshAnimOverlay& overlay : mesh_anim_overlays_)
    {
        if(overlay.animate && !overlay.path.empty())
        {
            const MeshAnim::Scene* anim = MeshAnim::Cache::instance()->GetOrLoad(overlay.path);
            if(anim && anim->hasAnimation())
            {
                need = true;
                break;
            }
        }
    }

    if(need)
    {
        if(!mesh_anim_timer_)
        {
            mesh_anim_timer_ = new QTimer(this);
            connect(mesh_anim_timer_, &QTimer::timeout, this, [this]() {
                update();
            });
        }
        if(!mesh_anim_timer_->isActive())
        {
            mesh_anim_timer_->start(33);
        }
    }
    else if(mesh_anim_timer_)
    {
        mesh_anim_timer_->stop();
    }
}

void LEDViewport3D::DrawSceneProps()
{
    if(!scene_props_)
    {
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for(size_t i = 0; i < scene_props_->size(); ++i)
    {
        SceneProp3D* prop = (*scene_props_)[i].get();
        if(!prop || !prop->IsVisible())
        {
            continue;
        }

        const float w = MMToGridUnits(prop->GetWidthMM(), grid_scale_mm);
        const float h = MMToGridUnits(prop->GetHeightMM(), grid_scale_mm);
        const float d = MMToGridUnits(prop->GetDepthMM(), grid_scale_mm);
        if(w <= 0.0f || h <= 0.0f || d <= 0.0f)
        {
            continue;
        }

        const float hw = w * 0.5f;
        const float hh = h * 0.5f;
        const float hd = d * 0.5f;
        float solid_r, solid_g, solid_b;
        float glass_r, glass_g, glass_b;
        UnpackRgb(prop->GetColor(), &solid_r, &solid_g, &solid_b);
        UnpackRgb(prop->GetGlassColor(), &glass_r, &glass_g, &glass_b);
        const float block_r = 0.12f;
        const float block_g = 0.12f;
        const float block_b = 0.14f;
        const bool selected = ((int)i == selected_scene_prop_idx_);
        ViewportMat4 model = ViewportMath::FromTransform3D(prop->GetTransform());

        if(prop->GetShape() == ScenePropShape::Mesh)
        {
            /* Scene props stay static; animated GLBs use Effects → Media → Animated Mesh. */
            const filesystem::path abs =
                ScenePropMeshPaths::Resolve(prop->GetMeshAsset());
            const std::string path = abs.string();
            const MeshImport::TriangleMesh* mesh =
                ScenePropMeshCache::instance()->GetOrLoad(path);
            if(!mesh || mesh->empty())
            {
                continue;
            }

            float r = solid_r, g = solid_g, b = solid_b;
            float alpha = selected ? 0.62f : 0.50f;
            if(prop->GetMeshBodyKind() == ScenePropFaceKind::Blocker)
            {
                r = block_r; g = block_g; b = block_b;
                alpha = selected ? 0.88f : 0.82f;
            }
            else if(prop->GetMeshBodyKind() == ScenePropFaceKind::Glass)
            {
                r = glass_r; g = glass_g; b = glass_b;
                alpha = selected ? 0.28f : 0.22f;
            }

            const std::vector<float>& tris =
                CachedScaledMesh(prop->GetId(), path, *mesh, hw, hh, hd, r, g, b);
            std::vector<float> edges;
            const float er = selected ? 0.95f : 0.55f;
            const float eg = selected ? 0.85f : 0.55f;
            const float eb = selected ? 0.25f : 0.58f;
            MeshGeometry::AppendAxisAlignedBoxEdges(edges, -hw, -hh, -hd, hw, hh, hd, er, eg, eb);

            if(prop->GetMeshBodyKind() == ScenePropFaceKind::Glass)
            {
                glDepthMask(GL_FALSE);
            }
            if(!tris.empty())
            {
                scene_prop_faces_batch_.Upload(MeshBatch::Layout::PosColor,
                                               tris.data(), tris.size() / 6);
                drawUnlitBatch(scene_prop_faces_batch_, MeshBatch::Primitive::Triangles, 1.0f,
                               alpha, &model);
            }
            if(prop->GetMeshBodyKind() == ScenePropFaceKind::Glass)
            {
                glDepthMask(GL_TRUE);
            }
            scene_prop_edges_batch_.Upload(MeshBatch::Layout::PosColor, edges.data(), edges.size() / 6);
            drawUnlitBatch(scene_prop_edges_batch_, MeshBatch::Primitive::Lines,
                           selected ? 2.5f : 1.5f, selected ? 0.95f : 0.70f, &model);
            continue;
        }

        std::vector<float> solid_faces;
        std::vector<float> glass_faces;
        std::vector<float> blocker_faces;
        std::vector<float> edges;

        for(int face_i = 0; face_i < SceneProp3D::kFaceCount; ++face_i)
        {
            const ScenePropFace face = (ScenePropFace)face_i;
            Vector3D corners[4];
            SceneProp3D::FaceLocalCorners(face, hw, hh, hd, corners);

            const ScenePropFaceKind kind = prop->GetFaceKind(face);
            float r = solid_r;
            float g = solid_g;
            float b = solid_b;
            std::vector<float>* dest = &solid_faces;
            if(kind == ScenePropFaceKind::Glass)
            {
                r = glass_r;
                g = glass_g;
                b = glass_b;
                dest = &glass_faces;
            }
            else if(kind == ScenePropFaceKind::Blocker)
            {
                r = block_r;
                g = block_g;
                b = block_b;
                dest = &blocker_faces;
            }
            MeshGeometry::PushQuadAsTris(*dest,
                                        corners[0].x, corners[0].y, corners[0].z,
                                        corners[1].x, corners[1].y, corners[1].z,
                                        corners[2].x, corners[2].y, corners[2].z,
                                        corners[3].x, corners[3].y, corners[3].z,
                                        r, g, b);
        }

        const float er = selected ? 0.95f : 0.55f;
        const float eg = selected ? 0.85f : 0.55f;
        const float eb = selected ? 0.25f : 0.58f;
        MeshGeometry::AppendAxisAlignedBoxEdges(edges, -hw, -hh, -hd, hw, hh, hd, er, eg, eb);

        if(!blocker_faces.empty())
        {
            scene_prop_faces_batch_.Upload(MeshBatch::Layout::PosColor,
                                           blocker_faces.data(), blocker_faces.size() / 6);
            drawUnlitBatch(scene_prop_faces_batch_, MeshBatch::Primitive::Triangles, 1.0f,
                           selected ? 0.88f : 0.82f, &model);
        }
        if(!solid_faces.empty())
        {
            scene_prop_faces_batch_.Upload(MeshBatch::Layout::PosColor,
                                           solid_faces.data(), solid_faces.size() / 6);
            drawUnlitBatch(scene_prop_faces_batch_, MeshBatch::Primitive::Triangles, 1.0f,
                           selected ? 0.62f : 0.50f, &model);
        }
        if(!glass_faces.empty())
        {
            glDepthMask(GL_FALSE);
            scene_prop_faces_batch_.Upload(MeshBatch::Layout::PosColor,
                                           glass_faces.data(), glass_faces.size() / 6);
            drawUnlitBatch(scene_prop_faces_batch_, MeshBatch::Primitive::Triangles, 1.0f,
                           selected ? 0.28f : 0.22f, &model);
            glDepthMask(GL_TRUE);
        }

        scene_prop_edges_batch_.Upload(MeshBatch::Layout::PosColor, edges.data(), edges.size() / 6);
        drawUnlitBatch(scene_prop_edges_batch_, MeshBatch::Primitive::Lines,
                       selected ? 2.5f : 1.5f, selected ? 0.95f : 0.70f, &model);
    }
}

int LEDViewport3D::PickSceneProp(const Ray3D& ray)
{
    if(!scene_props_)
    {
        return -1;
    }

    float closest = FLT_MAX;
    int best = -1;

    for(size_t i = 0; i < scene_props_->size(); ++i)
    {
        SceneProp3D* prop = (*scene_props_)[i].get();
        if(!prop || !prop->IsVisible())
        {
            continue;
        }
        const float w = MMToGridUnits(prop->GetWidthMM(), grid_scale_mm);
        const float h = MMToGridUnits(prop->GetHeightMM(), grid_scale_mm);
        const float d = MMToGridUnits(prop->GetDepthMM(), grid_scale_mm);
        if(w <= 0.0f || h <= 0.0f || d <= 0.0f)
        {
            continue;
        }
        const float hw = w * 0.5f;
        const float hh = h * 0.5f;
        const float hd = d * 0.5f;

        Vector3D local_corners[8] = {
            {-hw, -hh, -hd}, { hw, -hh, -hd}, { hw,  hh, -hd}, {-hw,  hh, -hd},
            {-hw, -hh,  hd}, { hw, -hh,  hd}, { hw,  hh,  hd}, {-hw,  hh,  hd},
        };
        Vector3D world_min = TransformLocalToWorld(local_corners[0], prop->GetTransform());
        Vector3D world_max = world_min;
        for(int c = 1; c < 8; ++c)
        {
            Vector3D wc = TransformLocalToWorld(local_corners[c], prop->GetTransform());
            world_min.x = std::min(world_min.x, wc.x);
            world_min.y = std::min(world_min.y, wc.y);
            world_min.z = std::min(world_min.z, wc.z);
            world_max.x = std::max(world_max.x, wc.x);
            world_max.y = std::max(world_max.y, wc.y);
            world_max.z = std::max(world_max.z, wc.z);
        }

        Box3D box;
        box.min[0] = world_min.x; box.min[1] = world_min.y; box.min[2] = world_min.z;
        box.max[0] = world_max.x; box.max[1] = world_max.y; box.max[2] = world_max.z;

        float tmin = 0.0f;
        float tmax = FLT_MAX;
        bool hit = true;
        for(int axis = 0; axis < 3; ++axis)
        {
            const float o = ray.origin[axis];
            const float dir = ray.direction[axis];
            if(std::fabs(dir) < 1e-8f)
            {
                if(o < box.min[axis] || o > box.max[axis])
                {
                    hit = false;
                    break;
                }
                continue;
            }
            float t1 = (box.min[axis] - o) / dir;
            float t2 = (box.max[axis] - o) / dir;
            if(t1 > t2)
            {
                std::swap(t1, t2);
            }
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if(tmin > tmax)
            {
                hit = false;
                break;
            }
        }
        if(hit && tmin >= 0.0f && tmin < closest)
        {
            closest = tmin;
            best = (int)i;
        }
    }
    return best;
}
