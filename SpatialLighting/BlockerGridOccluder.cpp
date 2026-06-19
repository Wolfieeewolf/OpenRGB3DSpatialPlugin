// SPDX-License-Identifier: GPL-2.0-only

#include "BlockerGridOccluder.h"

#include "ControllerLayout3D.h"
#include "Geometry3DUtils.h"
#include "GridSpaceUtils.h"
#include "SpatialLightingEngine.h"
#include "SpatialLightingSceneProvider.h"
#include "VirtualController3D.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace SpatialLighting
{
namespace
{

constexpr float kRayEpsilon = 1e-4f;
constexpr float kMinSegBiasFrac = 0.02f;

Vector3D ControllerLocalCenterOffset(const ::ControllerTransform* ctrl)
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

Vec3 ControllerLocalToWorld(const ::ControllerTransform* ctrl, const Vector3D& local_pos, const Vector3D& center_offset)
{
    const Vector3D rel = {
        local_pos.x - center_offset.x,
        local_pos.y - center_offset.y,
        local_pos.z - center_offset.z,
    };
    const Vector3D world = Geometry3D::TransformLocalToWorldScaled(rel, ctrl->transform);
    return {world.x, world.y, world.z};
}

void ExpandWorldBounds(Vector3D* world_min, Vector3D* world_max, Vec3 point)
{
    if(!world_min || !world_max)
    {
        return;
    }
    world_min->x = std::min(world_min->x, point.x);
    world_min->y = std::min(world_min->y, point.y);
    world_min->z = std::min(world_min->z, point.z);
    world_max->x = std::max(world_max->x, point.x);
    world_max->y = std::max(world_max->y, point.y);
    world_max->z = std::max(world_max->z, point.z);
}

Vector3D WorldToControllerLocal(Vec3 world, const ::ControllerTransform* ctrl, const Vector3D& center_offset)
{
    const Vector3D world_pos = {world.x, world.y, world.z};
    const Vector3D rel = Geometry3D::TransformWorldToLocalScaled(world_pos, ctrl->transform);
    return {
        rel.x + center_offset.x,
        rel.y + center_offset.y,
        rel.z + center_offset.z,
    };
}

void BuildAxisEdges(const VirtualController3D* layout,
                    float grid_scale_mm,
                    int axis_count,
                    float (VirtualController3D::*size_fn)(int) const,
                    std::vector<float>& out_edges)
{
    out_edges.assign(static_cast<size_t>(axis_count) + 1u, 0.0f);
    float acc = 0.0f;
    for(int index = 0; index < axis_count; ++index)
    {
        out_edges[static_cast<size_t>(index)] = acc;
        acc += MMToGridUnits((layout->*size_fn)(index), grid_scale_mm);
    }
    out_edges[static_cast<size_t>(axis_count)] = acc;
}

bool SegmentIntersectsAabb(Vec3 a,
                            Vec3 b,
                            float min_x,
                            float min_y,
                            float min_z,
                            float max_x,
                            float max_y,
                            float max_z)
{
    const float seg_min_x = std::min(a.x, b.x);
    const float seg_min_y = std::min(a.y, b.y);
    const float seg_min_z = std::min(a.z, b.z);
    const float seg_max_x = std::max(a.x, b.x);
    const float seg_max_y = std::max(a.y, b.y);
    const float seg_max_z = std::max(a.z, b.z);
    return seg_min_x <= max_x && seg_max_x >= min_x && seg_min_y <= max_y && seg_max_y >= min_y &&
           seg_min_z <= max_z && seg_max_z >= min_z;
}

bool CellIsBlocked(const BlockerGridOccluder& grid, int x, int y, int z)
{
    if(x < 0 || y < 0 || z < 0 || x >= grid.width || y >= grid.height || z >= grid.depth)
    {
        return false;
    }
    const size_t index =
        static_cast<size_t>(x) + static_cast<size_t>(y) * static_cast<size_t>(grid.width) +
        static_cast<size_t>(z) * static_cast<size_t>(grid.width) * static_cast<size_t>(grid.height);
    return index < grid.dense_cells.size() && grid.dense_cells[index] != 0;
}

int FindCellIndex(float coord, const std::vector<float>& edges)
{
    if(edges.size() < 2 || coord < edges.front() || coord >= edges.back())
    {
        return -1;
    }
    const auto upper = std::upper_bound(edges.begin(), edges.end(), coord);
    const int index = static_cast<int>(upper - edges.begin()) - 1;
    if(index < 0 || index >= static_cast<int>(edges.size()) - 1)
    {
        return -1;
    }
    return index;
}

float NextBoundaryT(float pos, float dir_component, float boundary)
{
    if(std::fabs(dir_component) < 1e-8f)
    {
        return std::numeric_limits<float>::max();
    }
    return (boundary - pos) / dir_component;
}

bool SegmentHitsBlockerGrid(Vec3 a_world,
                            Vec3 b_world,
                            const BlockerGridOccluder& grid,
                            const ::ControllerTransform* ctrl)
{
    if(!ctrl || grid.dense_cells.empty() || grid.x_edges.size() < 2 || grid.y_edges.size() < 2 ||
       grid.z_edges.size() < 2)
    {
        return false;
    }

    if(!SegmentIntersectsAabb(a_world,
                             b_world,
                             grid.world_min.x,
                             grid.world_min.y,
                             grid.world_min.z,
                             grid.world_max.x,
                             grid.world_max.y,
                             grid.world_max.z))
    {
        return false;
    }

    const Vector3D local_a = WorldToControllerLocal(a_world, ctrl, grid.center_offset);
    const Vector3D local_b = WorldToControllerLocal(b_world, ctrl, grid.center_offset);
    const Vec3 seg = {local_b.x - local_a.x, local_b.y - local_a.y, local_b.z - local_a.z};
    const float seg_len = std::sqrt(seg.x * seg.x + seg.y * seg.y + seg.z * seg.z);
    if(seg_len < 1e-5f)
    {
        return false;
    }

    const Vec3 dir = {seg.x / seg_len, seg.y / seg_len, seg.z / seg_len};
    const float t_min = std::max(kRayEpsilon * 4.0f, seg_len * kMinSegBiasFrac);
    const float t_max = seg_len - t_min;
    if(t_max <= t_min)
    {
        return false;
    }

    float px = local_a.x + dir.x * t_min;
    float py = local_a.y + dir.y * t_min;
    float pz = local_a.z + dir.z * t_min;

    int ix = FindCellIndex(px, grid.x_edges);
    int iy = FindCellIndex(py, grid.y_edges);
    int iz = FindCellIndex(pz, grid.z_edges);
    if(ix < 0 || iy < 0 || iz < 0)
    {
        return false;
    }

    const int step_x = (dir.x > 0.0f) ? 1 : ((dir.x < 0.0f) ? -1 : 0);
    const int step_y = (dir.y > 0.0f) ? 1 : ((dir.y < 0.0f) ? -1 : 0);
    const int step_z = (dir.z > 0.0f) ? 1 : ((dir.z < 0.0f) ? -1 : 0);

    float t_x = (step_x > 0) ? NextBoundaryT(px, dir.x, grid.x_edges[static_cast<size_t>(ix + 1)])
                             : NextBoundaryT(px, dir.x, grid.x_edges[static_cast<size_t>(ix)]);
    float t_y = (step_y > 0) ? NextBoundaryT(py, dir.y, grid.y_edges[static_cast<size_t>(iy + 1)])
                             : NextBoundaryT(py, dir.y, grid.y_edges[static_cast<size_t>(iy)]);
    float t_z = (step_z > 0) ? NextBoundaryT(pz, dir.z, grid.z_edges[static_cast<size_t>(iz + 1)])
                             : NextBoundaryT(pz, dir.z, grid.z_edges[static_cast<size_t>(iz)]);

    const int max_steps = grid.width + grid.height + grid.depth + 4;
    for(int step = 0; step < max_steps; ++step)
    {
        if(CellIsBlocked(grid, ix, iy, iz))
        {
            return true;
        }

        if(t_x < t_y)
        {
            if(t_x < t_z)
            {
                if(t_x > t_max)
                {
                    break;
                }
                px += dir.x * t_x;
                py += dir.y * t_x;
                pz += dir.z * t_x;
                ix += step_x;
                t_y -= t_x;
                t_z -= t_x;
                t_x = (step_x > 0) ? NextBoundaryT(px, dir.x, grid.x_edges[static_cast<size_t>(ix + 1)])
                                   : NextBoundaryT(px, dir.x, grid.x_edges[static_cast<size_t>(ix)]);
            }
            else
            {
                if(t_z > t_max)
                {
                    break;
                }
                px += dir.x * t_z;
                py += dir.y * t_z;
                pz += dir.z * t_z;
                iz += step_z;
                t_x -= t_z;
                t_y -= t_z;
                t_z = (step_z > 0) ? NextBoundaryT(pz, dir.z, grid.z_edges[static_cast<size_t>(iz + 1)])
                                   : NextBoundaryT(pz, dir.z, grid.z_edges[static_cast<size_t>(iz)]);
            }
        }
        else
        {
            if(t_y < t_z)
            {
                if(t_y > t_max)
                {
                    break;
                }
                px += dir.x * t_y;
                py += dir.y * t_y;
                pz += dir.z * t_y;
                iy += step_y;
                t_x -= t_y;
                t_z -= t_y;
                t_y = (step_y > 0) ? NextBoundaryT(py, dir.y, grid.y_edges[static_cast<size_t>(iy + 1)])
                                   : NextBoundaryT(py, dir.y, grid.y_edges[static_cast<size_t>(iy)]);
            }
            else
            {
                if(t_z > t_max)
                {
                    break;
                }
                px += dir.x * t_z;
                py += dir.y * t_z;
                pz += dir.z * t_z;
                iz += step_z;
                t_x -= t_z;
                t_y -= t_z;
                t_z = (step_z > 0) ? NextBoundaryT(pz, dir.z, grid.z_edges[static_cast<size_t>(iz + 1)])
                                   : NextBoundaryT(pz, dir.z, grid.z_edges[static_cast<size_t>(iz)]);
            }
        }

        if(ix < 0 || iy < 0 || iz < 0 || ix >= grid.width || iy >= grid.height || iz >= grid.depth)
        {
            break;
        }
    }

    return false;
}

bool SegmentHitsBlockerGridsImpl(Vec3 a,
                                 Vec3 b,
                                 const std::vector<BlockerGridOccluder>& grids,
                                 int also_skip_controller)
{
    const std::vector<std::unique_ptr<::ControllerTransform>>* transforms =
        SpatialLightingSceneProvider::instance()->controllers();
    if(!transforms || grids.empty())
    {
        return false;
    }

    const int skip_controller = SpatialLightingSceneProvider::instance()->shadingControllerIndex();
    for(const BlockerGridOccluder& grid : grids)
    {
        if((skip_controller >= 0 && grid.controller_index == skip_controller) ||
           (also_skip_controller >= 0 && grid.controller_index == also_skip_controller))
        {
            continue;
        }
        if(grid.controller_index < 0 || grid.controller_index >= static_cast<int>(transforms->size()))
        {
            continue;
        }
        ::ControllerTransform* ctrl = (*transforms)[static_cast<size_t>(grid.controller_index)].get();
        if(SegmentHitsBlockerGrid(a, b, grid, ctrl))
        {
            return true;
        }
    }
    return false;
}

} // namespace

void BuildBlockerGridOccluders(std::vector<BlockerGridOccluder>& out, float grid_scale_mm)
{
    out.clear();
    const std::vector<std::unique_ptr<::ControllerTransform>>* transforms =
        SpatialLightingSceneProvider::instance()->controllers();
    if(!transforms)
    {
        return;
    }

    const float scale_mm = (grid_scale_mm > 0.001f) ? grid_scale_mm : 10.0f;
    for(size_t ctrl_index = 0; ctrl_index < transforms->size(); ++ctrl_index)
    {
        ::ControllerTransform* ctrl = (*transforms)[ctrl_index].get();
        if(!ctrl || ctrl->hidden_by_virtual || !ctrl->virtual_controller)
        {
            continue;
        }

        VirtualController3D* layout = ctrl->virtual_controller;
        const std::vector<CustomControllerLightBlocker>& blockers = layout->GetLightBlockers();
        if(blockers.empty())
        {
            continue;
        }

        if(ctrl->world_positions_dirty)
        {
            ControllerLayout3D::UpdateWorldPositions(ctrl);
        }

        BlockerGridOccluder grid{};
        grid.controller_index = static_cast<int>(ctrl_index);
        grid.center_offset = ControllerLocalCenterOffset(ctrl);
        grid.width = layout->GetWidth();
        grid.height = layout->GetHeight();
        grid.depth = layout->GetDepth();
        BuildAxisEdges(layout, scale_mm, grid.width, &VirtualController3D::ColumnWidthMm, grid.x_edges);
        BuildAxisEdges(layout, scale_mm, grid.height, &VirtualController3D::RowHeightMm, grid.y_edges);
        BuildAxisEdges(layout, scale_mm, grid.depth, &VirtualController3D::LayerDepthMm, grid.z_edges);

        const size_t cell_count =
            static_cast<size_t>(grid.width) * static_cast<size_t>(grid.height) * static_cast<size_t>(grid.depth);
        grid.dense_cells.assign(cell_count, 0);
        for(const CustomControllerLightBlocker& blocker : blockers)
        {
            if(blocker.x < 0 || blocker.y < 0 || blocker.z < 0 || blocker.x >= grid.width || blocker.y >= grid.height ||
               blocker.z >= grid.depth)
            {
                continue;
            }
            const size_t index = static_cast<size_t>(blocker.x) +
                                 static_cast<size_t>(blocker.y) * static_cast<size_t>(grid.width) +
                                 static_cast<size_t>(blocker.z) * static_cast<size_t>(grid.width) *
                                     static_cast<size_t>(grid.height);
            grid.dense_cells[index] = 1;
        }

        const Vector3D local_corners[8] = {
            {0.0f, 0.0f, 0.0f},
            {grid.x_edges.back(), 0.0f, 0.0f},
            {0.0f, grid.y_edges.back(), 0.0f},
            {grid.x_edges.back(), grid.y_edges.back(), 0.0f},
            {0.0f, 0.0f, grid.z_edges.back()},
            {grid.x_edges.back(), 0.0f, grid.z_edges.back()},
            {0.0f, grid.y_edges.back(), grid.z_edges.back()},
            {grid.x_edges.back(), grid.y_edges.back(), grid.z_edges.back()},
        };
        grid.world_min = {std::numeric_limits<float>::max(),
                          std::numeric_limits<float>::max(),
                          std::numeric_limits<float>::max()};
        grid.world_max = {std::numeric_limits<float>::lowest(),
                          std::numeric_limits<float>::lowest(),
                          std::numeric_limits<float>::lowest()};
        for(const Vector3D& corner : local_corners)
        {
            const Vec3 world = ControllerLocalToWorld(ctrl, corner, grid.center_offset);
            ExpandWorldBounds(&grid.world_min, &grid.world_max, world);
        }

        out.push_back(std::move(grid));
    }
}

bool SegmentHitsBlockerGrids(float ax,
                             float ay,
                             float az,
                             float bx,
                             float by,
                             float bz,
                             const std::vector<BlockerGridOccluder>& grids,
                             int also_skip_controller)
{
    return SegmentHitsBlockerGridsImpl({ax, ay, az}, {bx, by, bz}, grids, also_skip_controller);
}

} // namespace SpatialLighting
