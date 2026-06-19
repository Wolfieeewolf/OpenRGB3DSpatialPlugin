// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialLightingEngine.h"

#include "SpatialEffect3D.h"
#include "SpatialLighting/OccluderSpatialIndex.h"
#include "SpatialLighting/BlockerGridOccluder.h"
#include "SpatialLightingSceneProvider.h"
#include "SpatialRoom/SpatialRoomFrame.h"
#include "ControllerLayout3D.h"
#include "DisplayPlaneManager.h"
#include "DisplayPlane3D.h"
#include "Geometry3DUtils.h"
#include "GridSpaceUtils.h"
#include "LEDPosition3D.h"
#include "VirtualController3D.h"

#include <algorithm>
#include <cmath>

namespace SpatialLighting
{

namespace
{

constexpr float kRayEpsilon = 1e-4f;
constexpr float kMinSegBiasFrac = 0.02f;

Vec3 Add(Vec3 a, Vec3 b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 Sub(Vec3 a, Vec3 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 Scale(Vec3 v, float s)
{
    return {v.x * s, v.y * s, v.z * s};
}

float Len(Vec3 v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vec3 Normalize(Vec3 v)
{
    const float l = Len(v);
    if(l < 1e-8f)
    {
        return {0.0f, 0.0f, 1.0f};
    }
    const float inv = 1.0f / l;
    return {v.x * inv, v.y * inv, v.z * inv};
}

float Dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float Clamp01(float x)
{
    return std::max(0.0f, std::min(1.0f, x));
}

static Vec3 ToVec3(const Vector3D& v)
{
    return {v.x, v.y, v.z};
}

static void ExpandAabbFromPoint(OccluderAabb& box, Vec3 p)
{
    if(p.x < box.min.x) box.min.x = p.x;
    if(p.y < box.min.y) box.min.y = p.y;
    if(p.z < box.min.z) box.min.z = p.z;
    if(p.x > box.max.x) box.max.x = p.x;
    if(p.y > box.max.y) box.max.y = p.y;
    if(p.z > box.max.z) box.max.z = p.z;
}

static bool SegmentHitsAabb(Vec3 origin, Vec3 dir, float t_min, float t_max, const OccluderAabb& box)
{
    float t0 = t_min;
    float t1 = t_max;
    const float origin_v[3] = {origin.x, origin.y, origin.z};
    const float dir_v[3] = {dir.x, dir.y, dir.z};
    const float min_v[3] = {box.min.x, box.min.y, box.min.z};
    const float max_v[3] = {box.max.x, box.max.y, box.max.z};

    for(int axis = 0; axis < 3; ++axis)
    {
        if(std::fabs(dir_v[axis]) < 1e-8f)
        {
            if(origin_v[axis] < min_v[axis] || origin_v[axis] > max_v[axis])
            {
                return false;
            }
        }
        else
        {
            float inv = 1.0f / dir_v[axis];
            float t_near = (min_v[axis] - origin_v[axis]) * inv;
            float t_far = (max_v[axis] - origin_v[axis]) * inv;
            if(t_near > t_far)
            {
                std::swap(t_near, t_far);
            }
            t0 = std::max(t0, t_near);
            t1 = std::min(t1, t_far);
            if(t0 > t1)
            {
                return false;
            }
        }
    }
    return true;
}

static bool SegmentHitsFiniteQuad(Vec3 a, Vec3 dir, float t_min, float t_max, const OccluderQuad& quad)
{
    const Vec3 u_axis = Sub(quad.corners[1], quad.corners[0]);
    const Vec3 v_axis = Sub(quad.corners[3], quad.corners[0]);
    Vec3 normal = {
        u_axis.y * v_axis.z - u_axis.z * v_axis.y,
        u_axis.z * v_axis.x - u_axis.x * v_axis.z,
        u_axis.x * v_axis.y - u_axis.y * v_axis.x,
    };
    const float normal_len = Len(normal);
    if(normal_len < 1e-8f)
    {
        return false;
    }
    normal = Scale(normal, 1.0f / normal_len);

    const float denom = Dot(dir, normal);
    if(std::fabs(denom) < 1e-8f)
    {
        return false;
    }

    const float t_plane = Dot(Sub(quad.corners[0], a), normal) / denom;
    if(t_plane < t_min || t_plane > t_max)
    {
        return false;
    }

    const Vec3 hit = Add(a, Scale(dir, t_plane));
    const Vec3 rel = Sub(hit, quad.corners[0]);
    const float uu = Dot(u_axis, u_axis);
    const float vv = Dot(v_axis, v_axis);
    if(uu < 1e-10f || vv < 1e-10f)
    {
        return false;
    }
    const float u = Dot(rel, u_axis) / uu;
    const float v = Dot(rel, v_axis) / vv;
    constexpr float kPad = 0.02f;
    return u >= -kPad && u <= 1.0f + kPad && v >= -kPad && v <= 1.0f + kPad;
}

static bool SegmentHitsOccluderSet(Vec3 a,
                                   Vec3 b,
                                   const std::vector<OccluderAabb>& aabbs,
                                   const std::vector<OccluderQuad>& quads,
                                   int also_skip_controller,
                                   const OccluderSpatialIndex* aabb_index,
                                   const std::vector<BlockerGridOccluder>* blocker_grids)
{
    const std::vector<BlockerGridOccluder>* grids = blocker_grids;
    if(!grids)
    {
        grids = &SpatialLightingSceneProvider::instance()->frameBlockerGrids();
    }
    if(grids && !grids->empty() &&
       SegmentHitsBlockerGrids(a.x, a.y, a.z, b.x, b.y, b.z, *grids, also_skip_controller))
    {
        return true;
    }

    const Vec3 seg = Sub(b, a);
    const float seg_len = Len(seg);
    if(seg_len < 1e-5f)
    {
        return false;
    }
    const Vec3 dir = Scale(seg, 1.0f / seg_len);
    const float t_min = std::max(kRayEpsilon * 4.0f, seg_len * kMinSegBiasFrac);
    const float t_max = seg_len - t_min;
    if(t_max <= t_min)
    {
        return false;
    }

    const int skip_controller = SpatialLightingSceneProvider::instance()->shadingControllerIndex();
    thread_local std::vector<uint16_t> candidate_indices;
    candidate_indices.clear();

    if(aabb_index && aabb_index->IsBuilt())
    {
        aabb_index->CollectSegmentCandidates(a, b, candidate_indices);
        for(uint16_t box_index : candidate_indices)
        {
            if(box_index >= aabbs.size())
            {
                continue;
            }
            const OccluderAabb& box = aabbs[box_index];
            if((skip_controller >= 0 && box.controller_index == skip_controller) ||
               (also_skip_controller >= 0 && box.controller_index == also_skip_controller))
            {
                continue;
            }
            if(SegmentHitsAabb(a, dir, t_min, t_max, box))
            {
                return true;
            }
        }
    }
    else
    {
        for(const OccluderAabb& box : aabbs)
        {
            if((skip_controller >= 0 && box.controller_index == skip_controller) ||
               (also_skip_controller >= 0 && box.controller_index == also_skip_controller))
            {
                continue;
            }
            if(SegmentHitsAabb(a, dir, t_min, t_max, box))
            {
                return true;
            }
        }
    }

    for(const OccluderQuad& quad : quads)
    {
        if((skip_controller >= 0 && quad.controller_index == skip_controller) ||
           (also_skip_controller >= 0 && quad.controller_index == also_skip_controller))
        {
            continue;
        }
        if(SegmentHitsFiniteQuad(a, dir, t_min, t_max, quad))
        {
            return true;
        }
    }
    return false;
}

static float ComputeAmbientOcclusion(Vec3 led,
                                     const std::vector<OccluderAabb>& aabbs,
                                     const std::vector<OccluderQuad>& quads,
                                     float probe_span,
                                     const OccluderSpatialIndex* aabb_index,
                                     const std::vector<BlockerGridOccluder>* blocker_grids)
{
    static const Vec3 kDirs[6] = {
        {1.0f, 0.0f, 0.0f},  {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
        {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},  {0.0f, 0.0f, -1.0f},
    };

    const float span = std::max(probe_span, 0.15f);
    int blocked = 0;
    for(const Vec3& d : kDirs)
    {
        const Vec3 probe = Add(led, Scale(d, span));
        if(SegmentHitsOccluderSet(led, probe, aabbs, quads, -1, aabb_index, blocker_grids))
        {
            ++blocked;
        }
    }
    return 1.0f - (static_cast<float>(blocked) / 6.0f);
}

static const OccluderSpatialIndex* ResolveAabbSpatialIndex(const std::vector<OccluderAabb>& aabbs)
{
    SpatialLightingSceneProvider* provider = SpatialLightingSceneProvider::instance();
    if(provider && &aabbs == &provider->frameOccluderAabbs() && provider->frameOccluderSpatialIndex().IsBuilt())
    {
        return &provider->frameOccluderSpatialIndex();
    }
    return nullptr;
}

} // namespace

bool LedSegmentOccluded(float ax,
                        float ay,
                        float az,
                        float bx,
                        float by,
                        float bz,
                        const std::vector<OccluderAabb>& aabbs,
                        const std::vector<OccluderQuad>& quads,
                        int also_skip_controller)
{
    const Vec3 a = {ax, ay, az};
    const Vec3 b = {bx, by, bz};
    return SegmentHitsOccluderSet(a, b, aabbs, quads, also_skip_controller, ResolveAabbSpatialIndex(aabbs), nullptr);
}

float ComputeLedAmbientOcclusion(float led_x,
                                 float led_y,
                                 float led_z,
                                 const std::vector<OccluderAabb>& aabbs,
                                 const std::vector<OccluderQuad>& quads,
                                 float probe_span,
                                 const OccluderSpatialIndex* aabb_index)
{
    const OccluderSpatialIndex* index = aabb_index;
    if(!index)
    {
        index = ResolveAabbSpatialIndex(aabbs);
    }
    return ComputeAmbientOcclusion({led_x, led_y, led_z}, aabbs, quads, probe_span, index, nullptr);
}

float ComputeRoomAmbientShadeFactor(float room_x,
                                    float room_y,
                                    float room_z,
                                    float room_center_x,
                                    float room_center_y,
                                    float room_center_z,
                                    const std::vector<OccluderAabb>& aabbs,
                                    const std::vector<OccluderQuad>& quads,
                                    const std::vector<BlockerGridOccluder>& blocker_grids,
                                    float ao_strength_norm,
                                    float probe_span,
                                    const OccluderSpatialIndex* aabb_index)
{
    if(aabbs.empty() && quads.empty() && blocker_grids.empty())
    {
        return 1.0f;
    }

    const OccluderSpatialIndex* index = aabb_index;
    if(!index)
    {
        index = ResolveAabbSpatialIndex(aabbs);
    }

    const std::vector<BlockerGridOccluder>* grids =
        blocker_grids.empty() ? nullptr : &blocker_grids;

    float shade_factor = 1.0f;
    const Vec3 led = {room_x, room_y, room_z};
    const Vec3 center = {room_center_x, room_center_y, room_center_z};
    if(SegmentHitsOccluderSet(led, center, aabbs, quads, -1, index, grids))
    {
        shade_factor *= 0.18f;
    }

    float effective_ao = ao_strength_norm;
    if(SpatialRoom::IsRoomGridOverlayPass())
    {
        effective_ao = 0.0f;
    }

    if(effective_ao > 0.01f)
    {
        const float openness = ComputeAmbientOcclusion(led, aabbs, quads, probe_span, index, grids);
        shade_factor *= 1.0f - effective_ao * (1.0f - openness);
    }

    return shade_factor;
}

void BuildOccluderAabbSpatialIndex(const std::vector<OccluderAabb>& aabbs,
                                   const GridContext3D& grid,
                                   OccluderSpatialIndex& out_index)
{
    out_index.Build(aabbs,
                    grid.min_x,
                    grid.min_y,
                    grid.min_z,
                    grid.max_x,
                    grid.max_y,
                    grid.max_z);
}

Vec3 GridToVec3(float x, float y, float z)
{
    return {x, y, z};
}

static void PushQuad(std::vector<OccluderQuad>& out, Vec3 c0, Vec3 c1, Vec3 c2, Vec3 c3)
{
    OccluderQuad quad{};
    quad.corners[0] = c0;
    quad.corners[1] = c1;
    quad.corners[2] = c2;
    quad.corners[3] = c3;
    quad.double_sided = true;
    quad.controller_index = -1;
    out.push_back(quad);
}

void AppendRoomWallOccluders(std::vector<OccluderQuad>& out,
                             float min_x,
                             float min_y,
                             float min_z,
                             float max_x,
                             float max_y,
                             float max_z)
{
    const float eps = 1e-4f;
    PushQuad(out, {min_x, min_y, min_z}, {min_x, max_y, min_z}, {min_x, max_y, max_z}, {min_x, min_z, max_z});
    PushQuad(out, {max_x, min_y, min_z}, {max_x, min_y, max_z}, {max_x, max_y, max_z}, {max_x, max_y, min_z});
    PushQuad(out, {min_x, min_y, min_z}, {max_x, min_y, min_z}, {max_x, min_y, max_z}, {min_x, min_y, max_z});
    PushQuad(out, {min_x, max_y, min_z}, {min_x, max_y, max_z}, {max_x, max_y, max_z}, {max_x, max_y, min_z});
    PushQuad(out, {min_x, min_y, min_z}, {min_x, min_y, max_z}, {max_x, min_y, max_z}, {max_x, min_y, min_z});
    (void)eps;
}

static Vec3 ControllerLocalToWorld(const ::ControllerTransform* ctrl,
                                   const Vector3D& local_pos,
                                   const Vector3D& center_offset)
{
    const Vector3D rel = {
        local_pos.x - center_offset.x,
        local_pos.y - center_offset.y,
        local_pos.z - center_offset.z,
    };
    const Vector3D world = Geometry3D::TransformLocalToWorldScaled(rel, ctrl->transform);
    return ToVec3(world);
}

static Vector3D ControllerLocalCenterOffset(const ::ControllerTransform* ctrl)
{
    Vector3D min_bounds{};
    Vector3D max_bounds{};
    ControllerLayout3D::CalculateControllerLocalBounds(ctrl, min_bounds, max_bounds);
    return {
        (min_bounds.x + max_bounds.x) * 0.5f,
        (min_bounds.y + max_bounds.y) * 0.5f,
        (min_bounds.z + max_bounds.z) * 0.5f,
    };
}

static void PushControllerAabbFromPoints(std::vector<OccluderAabb>& out,
                                         int controller_index,
                                         const Vec3* points,
                                         int point_count)
{
    if(point_count <= 0)
    {
        return;
    }

    OccluderAabb box{};
    box.min = points[0];
    box.max = points[0];
    box.controller_index = controller_index;
    for(int i = 1; i < point_count; ++i)
    {
        ExpandAabbFromPoint(box, points[i]);
    }
    out.push_back(box);
}

void AppendCustomControllerLightBlockerAabbs(std::vector<OccluderAabb>& out,
                                           ::ControllerTransform* ctrl,
                                           int controller_index)
{
    if(!ctrl || !ctrl->virtual_controller)
    {
        return;
    }

    VirtualController3D* layout = ctrl->virtual_controller;
    const std::vector<CustomControllerLightBlocker>& blockers = layout->GetLightBlockers();
    if(blockers.empty())
    {
        return;
    }

    if(ctrl->world_positions_dirty)
    {
        ControllerLayout3D::UpdateWorldPositions(ctrl);
    }

    const Vector3D center_offset = ControllerLocalCenterOffset(ctrl);

    for(const CustomControllerLightBlocker& blocker : blockers)
    {
        Vector3D local_min{};
        Vector3D local_max{};
        layout->CellLocalBoundsMm(blocker.x, blocker.y, blocker.z, &local_min, &local_max);

        const Vector3D local_corners[4] = {
            {local_min.x, local_min.y, local_max.z},
            {local_max.x, local_min.y, local_max.z},
            {local_min.x, local_max.y, local_max.z},
            {local_max.x, local_max.y, local_max.z},
        };

        Vec3 world_corners[4];
        for(int i = 0; i < 4; ++i)
        {
            world_corners[i] = ControllerLocalToWorld(ctrl, local_corners[i], center_offset);
        }

        PushControllerAabbFromPoints(out, controller_index, world_corners, 4);
    }
}

void AppendControllerOccluders(std::vector<OccluderAabb>& out, float grid_scale_mm)
{
    const std::vector<std::unique_ptr<::ControllerTransform>>* transforms =
        SpatialLightingSceneProvider::instance()->controllers();
    if(!transforms)
    {
        return;
    }

    const float scale_mm = (grid_scale_mm > 0.001f) ? grid_scale_mm : 10.0f;
    const float body_pad = MMToGridUnits(22.0f, scale_mm);

    for(size_t ctrl_index = 0; ctrl_index < transforms->size(); ++ctrl_index)
    {
        ::ControllerTransform* ctrl = (*transforms)[ctrl_index].get();
        if(!ctrl || ctrl->hidden_by_virtual)
        {
            continue;
        }

        const int tag = static_cast<int>(ctrl_index);
        if(ctrl->led_positions.empty())
        {
            continue;
        }

        if(ctrl->world_positions_dirty)
        {
            ControllerLayout3D::UpdateWorldPositions(ctrl);
        }

        Vector3D local_min{};
        Vector3D local_max{};
        ControllerLayout3D::CalculateControllerLocalBounds(ctrl, local_min, local_max);

        local_min.x -= body_pad;
        local_min.y -= body_pad;
        local_min.z -= body_pad;
        local_max.x += body_pad;
        local_max.y += body_pad;
        local_max.z += body_pad;

        const float span_x = local_max.x - local_min.x;
        const float span_y = local_max.y - local_min.y;
        const float span_z = local_max.z - local_min.z;
        if(span_x < body_pad)
        {
            const float cx = (local_min.x + local_max.x) * 0.5f;
            local_min.x = cx - body_pad * 0.5f;
            local_max.x = cx + body_pad * 0.5f;
        }
        if(span_y < body_pad)
        {
            const float cy = (local_min.y + local_max.y) * 0.5f;
            local_min.y = cy - body_pad * 0.5f;
            local_max.y = cy + body_pad * 0.5f;
        }
        if(span_z < body_pad)
        {
            const float cz = (local_min.z + local_max.z) * 0.5f;
            local_min.z = cz - body_pad * 0.5f;
            local_max.z = cz + body_pad * 0.5f;
        }

        const Vector3D center_offset = ControllerLocalCenterOffset(ctrl);
        const Vector3D local_corners[8] = {
            {local_min.x, local_min.y, local_min.z},
            {local_max.x, local_min.y, local_min.z},
            {local_min.x, local_max.y, local_min.z},
            {local_max.x, local_max.y, local_min.z},
            {local_min.x, local_min.y, local_max.z},
            {local_max.x, local_min.y, local_max.z},
            {local_min.x, local_max.y, local_max.z},
            {local_max.x, local_max.y, local_max.z},
        };

        Vec3 world_corners[8];
        for(int i = 0; i < 8; ++i)
        {
            world_corners[i] = ControllerLocalToWorld(ctrl, local_corners[i], center_offset);
        }

        PushControllerAabbFromPoints(out, tag, world_corners, 8);
    }
}

static void AppendVirtualControllerBlockerAabbs(std::vector<OccluderAabb>& out)
{
    const std::vector<std::unique_ptr<::ControllerTransform>>* transforms =
        SpatialLightingSceneProvider::instance()->controllers();
    if(!transforms)
    {
        return;
    }

    for(size_t ctrl_index = 0; ctrl_index < transforms->size(); ++ctrl_index)
    {
        ::ControllerTransform* ctrl = (*transforms)[ctrl_index].get();
        if(!ctrl || ctrl->hidden_by_virtual || !ctrl->virtual_controller)
        {
            continue;
        }
        AppendCustomControllerLightBlockerAabbs(out, ctrl, static_cast<int>(ctrl_index));
    }
}

void BuildSpatialOccluders(std::vector<OccluderQuad>& out,
                           std::vector<OccluderAabb>& aabbs,
                           const GridContext3D& grid,
                           const OccluderBuildOptions& options)
{
    out.clear();
    aabbs.clear();
    if(options.display_planes)
    {
        AppendDisplayPlaneOccluders(out, grid.grid_scale_mm);
    }
    if(options.room_walls)
    {
        AppendRoomWallOccluders(out,
                                grid.min_x,
                                grid.min_y,
                                grid.min_z,
                                grid.max_x,
                                grid.max_y,
                                grid.max_z);
    }
    if(options.controllers)
    {
        AppendControllerOccluders(aabbs, grid.grid_scale_mm);
    }
}

void AppendDisplayPlaneOccluders(std::vector<OccluderQuad>& out, float grid_scale_mm)
{
    const float scale_mm = (grid_scale_mm > 0.001f) ? grid_scale_mm : 10.0f;
    for(DisplayPlane3D* plane : DisplayPlaneManager::instance()->GetDisplayPlanes())
    {
        if(!plane || !plane->IsVisible())
        {
            continue;
        }

        const float width_units = MMToGridUnits(plane->GetWidthMM(), scale_mm);
        const float height_units = MMToGridUnits(plane->GetHeightMM(), scale_mm);
        if(width_units <= 0.0f || height_units <= 0.0f)
        {
            continue;
        }

        const float half_w = width_units * 0.5f;
        const float half_h = height_units * 0.5f;
        const Vector3D local_corners[4] = {
            {-half_w, -half_h, 0.0f},
            {half_w, -half_h, 0.0f},
            {half_w, half_h, 0.0f},
            {-half_w, half_h, 0.0f},
        };

        OccluderQuad quad;
        for(int i = 0; i < 4; ++i)
        {
            const Vector3D world =
                Geometry3D::TransformDisplayPlaneLocalToWorld(local_corners[i], plane->GetTransform());
            quad.corners[i] = ToVec3(world);
        }

        const Vec3 e1 = Sub(quad.corners[1], quad.corners[0]);
        const Vec3 e2 = Sub(quad.corners[3], quad.corners[0]);
        const Vec3 n = {
            e1.y * e2.z - e1.z * e2.y,
            e1.z * e2.x - e1.x * e2.z,
            e1.x * e2.y - e1.y * e2.x,
        };
        quad.normal = Normalize(n);
        quad.double_sided = true;
        quad.controller_index = -1;
        out.push_back(quad);
    }
}

namespace
{

struct ShadeRgb
{
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
};

ShadeRgb ShadeLedFromSource(const RoomScene& scene,
                            const Vec3& led,
                            const EmissiveSource& src,
                            bool has_occluders,
                            float ao)
{
    const Vec3 fire = src.position;
    const Vec3 to_fire = Sub(fire, led);
    const float dist = Len(to_fire);

    const float glow_r = std::max(src.radius, 0.02f);
    const float light_r = std::max(src.light_radius, glow_r);

    const float glow_t = dist / glow_r;
    float emissive = src.emissive_strength * std::exp(-glow_t * glow_t * 1.35f);

    const float norm_d = dist / light_r;
    const float falloff = 1.0f / (1.0f + norm_d * norm_d * scene.shade.direct_falloff);

    float visibility = 1.0f;
    if(scene.shade.use_occlusion && has_occluders && dist > glow_r * 0.2f)
    {
        visibility = SegmentHitsOccluderSet(led,
                                            fire,
                                            scene.occluder_aabbs,
                                            scene.occluders,
                                            -1,
                                            scene.occluder_aabb_index,
                                            scene.blocker_grids.empty() ? nullptr : &scene.blocker_grids)
                         ? 0.0f
                         : 1.0f;
    }

    emissive *= visibility;
    const float direct = src.light_strength * falloff * visibility;

    const float fill_atten = std::max(scene.shade.room_fill_atten, 0.12f);
    const float room_fill =
        scene.shade.room_fill_strength * std::exp(-dist / (light_r * fill_atten * 2.2f)) * visibility;

    const float ambient = scene.shade.ambient_level * ao * visibility;
    const float total = emissive + direct + room_fill + ambient;

    ShadeRgb out{};
    out.r = Clamp01(src.r * total);
    out.g = Clamp01(src.g * total);
    out.b = Clamp01(src.b * total);
    return out;
}

} // namespace

RGBColor ShadeLed(const RoomScene& scene, float led_x, float led_y, float led_z)
{
    const Vec3 led = {led_x, led_y, led_z};
    const bool has_occluders = !scene.occluders.empty() || !scene.occluder_aabbs.empty();

    float ao = 1.0f;
    if(scene.shade.use_occlusion && scene.shade.use_ambient_occlusion && has_occluders &&
       scene.shade.ao_strength > 0.001f)
    {
        ao = ComputeAmbientOcclusion(led,
                                     scene.occluder_aabbs,
                                     scene.occluders,
                                     scene.shade.ao_probe_span,
                                     scene.occluder_aabb_index,
                                     scene.blocker_grids.empty() ? nullptr : &scene.blocker_grids);
        ao = 1.0f - scene.shade.ao_strength * (1.0f - ao);
    }

    float sum_r = 0.0f;
    float sum_g = 0.0f;
    float sum_b = 0.0f;
    if(scene.sources.empty())
    {
        const ShadeRgb c = ShadeLedFromSource(scene, led, scene.source, has_occluders, ao);
        sum_r = c.r;
        sum_g = c.g;
        sum_b = c.b;
    }
    else
    {
        for(const EmissiveSource& src : scene.sources)
        {
            const ShadeRgb c = ShadeLedFromSource(scene, led, src, has_occluders, ao);
            sum_r += c.r;
            sum_g += c.g;
            sum_b += c.b;
        }
    }

    return ToRGBColor((int)(Clamp01(sum_r) * 255.0f + 0.5f),
                      (int)(Clamp01(sum_g) * 255.0f + 0.5f),
                      (int)(Clamp01(sum_b) * 255.0f + 0.5f));
}

} // namespace SpatialLighting
