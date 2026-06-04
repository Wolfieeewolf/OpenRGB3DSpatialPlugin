// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialLightingEngine.h"

#include "SpatialLightingSceneProvider.h"
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

static void PushControllerAabb(std::vector<OccluderAabb>& out, int controller_index, const Vec3 corners[8])
{
    OccluderAabb box{};
    box.min = corners[0];
    box.max = corners[0];
    box.controller_index = controller_index;
    for(int i = 1; i < 8; ++i)
    {
        ExpandAabbFromPoint(box, corners[i]);
    }
    out.push_back(box);
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
                                   const std::vector<OccluderQuad>& quads)
{
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
    for(const OccluderAabb& box : aabbs)
    {
        if(skip_controller >= 0 && box.controller_index == skip_controller)
        {
            continue;
        }
        if(SegmentHitsAabb(a, dir, t_min, t_max, box))
        {
            return true;
        }
    }
    for(const OccluderQuad& quad : quads)
    {
        if(skip_controller >= 0 && quad.controller_index == skip_controller)
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

float ComputeAmbientOcclusion(Vec3 led,
                              const std::vector<OccluderAabb>& aabbs,
                              const std::vector<OccluderQuad>& quads,
                              float probe_span)
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
        if(SegmentHitsOccluderSet(led, probe, aabbs, quads))
        {
            ++blocked;
        }
    }
    return 1.0f - (static_cast<float>(blocked) / 6.0f);
}

} // namespace

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

static Vec3 ControllerLocalToWorld(const ControllerTransform* ctrl, const Vector3D& local_pos)
{
    Vector3D min_bounds{};
    Vector3D max_bounds{};
    ControllerLayout3D::CalculateControllerLocalBounds(ctrl, min_bounds, max_bounds);
    const Vector3D center = {
        (min_bounds.x + max_bounds.x) * 0.5f,
        (min_bounds.y + max_bounds.y) * 0.5f,
        (min_bounds.z + max_bounds.z) * 0.5f,
    };
    const Vector3D rel = {
        local_pos.x - center.x,
        local_pos.y - center.y,
        local_pos.z - center.z,
    };
    const Vector3D world = Geometry3D::TransformLocalToWorldScaled(rel, ctrl->transform);
    return ToVec3(world);
}

void AppendCustomControllerLightBlockerAabbs(std::vector<OccluderAabb>& out,
                                           ControllerTransform* ctrl,
                                           int controller_index,
                                           float grid_scale_mm)
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

    const float scale_x = MMToGridUnits(layout->GetSpacingX(), grid_scale_mm);
    const float scale_y = MMToGridUnits(layout->GetSpacingY(), grid_scale_mm);
    const float scale_z = MMToGridUnits(layout->GetSpacingZ(), grid_scale_mm);

    for(const CustomControllerLightBlocker& blocker : blockers)
    {
        const Vector3D local_min = {
            static_cast<float>(blocker.x) * scale_x,
            static_cast<float>(blocker.y) * scale_y,
            static_cast<float>(blocker.z) * scale_z,
        };
        const Vector3D local_max = {
            static_cast<float>(blocker.x + 1) * scale_x,
            static_cast<float>(blocker.y + 1) * scale_y,
            static_cast<float>(blocker.z + 1) * scale_z,
        };

        const Vec3 w[8] = {
            ControllerLocalToWorld(ctrl, local_min),
            ControllerLocalToWorld(ctrl, {local_max.x, local_min.y, local_min.z}),
            ControllerLocalToWorld(ctrl, {local_min.x, local_max.y, local_min.z}),
            ControllerLocalToWorld(ctrl, {local_max.x, local_max.y, local_min.z}),
            ControllerLocalToWorld(ctrl, {local_min.x, local_min.y, local_max.z}),
            ControllerLocalToWorld(ctrl, {local_max.x, local_min.y, local_max.z}),
            ControllerLocalToWorld(ctrl, {local_min.x, local_max.y, local_max.z}),
            ControllerLocalToWorld(ctrl, local_max),
        };

        PushControllerAabb(out, controller_index, w);
    }
}

void AppendControllerOccluders(std::vector<OccluderAabb>& out)
{
    const std::vector<std::unique_ptr<ControllerTransform>>* transforms =
        SpatialLightingSceneProvider::instance()->controllers();
    if(!transforms)
    {
        return;
    }

    for(size_t ctrl_index = 0; ctrl_index < transforms->size(); ++ctrl_index)
    {
        ControllerTransform* ctrl = (*transforms)[ctrl_index].get();
        if(!ctrl || ctrl->hidden_by_virtual)
        {
            continue;
        }

        const int tag = static_cast<int>(ctrl_index);
        if(ctrl->virtual_controller)
        {
            continue;
        }

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

        const Vec3 w[8] = {
            ControllerLocalToWorld(ctrl, {local_min.x, local_min.y, local_min.z}),
            ControllerLocalToWorld(ctrl, {local_max.x, local_min.y, local_min.z}),
            ControllerLocalToWorld(ctrl, {local_min.x, local_max.y, local_min.z}),
            ControllerLocalToWorld(ctrl, {local_max.x, local_max.y, local_min.z}),
            ControllerLocalToWorld(ctrl, {local_min.x, local_min.y, local_max.z}),
            ControllerLocalToWorld(ctrl, {local_max.x, local_min.y, local_max.z}),
            ControllerLocalToWorld(ctrl, {local_min.x, local_max.y, local_max.z}),
            ControllerLocalToWorld(ctrl, {local_max.x, local_max.y, local_max.z}),
        };

        PushControllerAabb(out, tag, w);
    }
}

static void AppendVirtualControllerBlockerAabbs(std::vector<OccluderAabb>& out, float grid_scale_mm)
{
    const std::vector<std::unique_ptr<ControllerTransform>>* transforms =
        SpatialLightingSceneProvider::instance()->controllers();
    if(!transforms)
    {
        return;
    }

    for(size_t ctrl_index = 0; ctrl_index < transforms->size(); ++ctrl_index)
    {
        ControllerTransform* ctrl = (*transforms)[ctrl_index].get();
        if(!ctrl || ctrl->hidden_by_virtual || !ctrl->virtual_controller)
        {
            continue;
        }
        AppendCustomControllerLightBlockerAabbs(out, ctrl, static_cast<int>(ctrl_index), grid_scale_mm);
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
        AppendControllerOccluders(aabbs);
        AppendVirtualControllerBlockerAabbs(aabbs, grid.grid_scale_mm);
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

RGBColor ShadeLed(const RoomScene& scene, float led_x, float led_y, float led_z)
{
    const Vec3 led = {led_x, led_y, led_z};
    const EmissiveSource& src = scene.source;
    const Vec3 fire = src.position;
    const Vec3 to_fire = Sub(fire, led);
    const float dist = Len(to_fire);

    const float glow_r = std::max(src.radius, 0.02f);
    const float light_r = std::max(src.light_radius, glow_r);

    const float glow_t = dist / glow_r;
    float emissive = src.emissive_strength * std::exp(-glow_t * glow_t * 1.35f);

    const float norm_d = dist / light_r;
    const float falloff = 1.0f / (1.0f + norm_d * norm_d * scene.shade.direct_falloff);

    const bool overlay_preview = SpatialLightingSceneProvider::instance()->roomGridOverlayPreview();
    const bool has_occluders = !scene.occluders.empty() || !scene.occluder_aabbs.empty();

    float visibility = 1.0f;
    if(!overlay_preview && scene.shade.use_occlusion && has_occluders && dist > glow_r * 0.2f)
    {
        visibility = SegmentHitsOccluderSet(led, fire, scene.occluder_aabbs, scene.occluders) ? 0.0f : 1.0f;
    }

    emissive *= visibility;
    const float direct = src.light_strength * falloff * visibility;

    float ao = 1.0f;
    if(!overlay_preview && scene.shade.use_occlusion && scene.shade.use_ambient_occlusion && has_occluders &&
       scene.shade.ao_strength > 0.001f)
    {
        ao = ComputeAmbientOcclusion(led, scene.occluder_aabbs, scene.occluders, scene.shade.ao_probe_span);
        ao = 1.0f - scene.shade.ao_strength * (1.0f - ao);
    }

    const float fill_atten = std::max(scene.shade.room_fill_atten, 0.15f);
    const float room_fill =
        scene.shade.room_fill_strength * std::exp(-dist / (light_r * fill_atten * 4.0f)) * visibility;

    const float ambient = scene.shade.ambient_level * ao * visibility;
    const float total = emissive + direct + room_fill + ambient;

    const float r = Clamp01(src.r * total);
    const float g = Clamp01(src.g * total);
    const float b = Clamp01(src.b * total);

    return ToRGBColor((int)(r * 255.0f + 0.5f), (int)(g * 255.0f + 0.5f), (int)(b * 255.0f + 0.5f));
}

} // namespace SpatialLighting
