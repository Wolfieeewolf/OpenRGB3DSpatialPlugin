// SPDX-License-Identifier: GPL-2.0-only

#include "BlockerGridOccluder.h"

#include "ControllerLayout3D.h"
#include "Geometry3DUtils.h"
#include "GridSpaceUtils.h"
#include "SpatialEffect3D.h"
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

bool RoomBlockerCellBlocks(const RoomBlockerField& field, int x, int y, int z, int skip_controller, int also_skip_controller)
{
    if(x < 0 || y < 0 || z < 0 || x >= field.width || y >= field.height || z >= field.depth)
    {
        return false;
    }
    const size_t index =
        static_cast<size_t>(x) + static_cast<size_t>(y) * static_cast<size_t>(field.width) +
        static_cast<size_t>(z) * static_cast<size_t>(field.width) * static_cast<size_t>(field.height);
    if(index >= field.cells.size())
    {
        return false;
    }
    const uint16_t owner = field.cells[index];
    if(owner == 0)
    {
        return false;
    }
    const int controller_index = static_cast<int>(owner) - 1;
    if((skip_controller >= 0 && controller_index == skip_controller) ||
       (also_skip_controller >= 0 && controller_index == also_skip_controller))
    {
        return false;
    }
    return true;
}

void BuildUniformAxisEdges(float origin,
                           float cell_size,
                           int axis_count,
                           std::vector<float>& out_edges)
{
    out_edges.resize(static_cast<size_t>(axis_count) + 1u);
    for(int index = 0; index <= axis_count; ++index)
    {
        out_edges[static_cast<size_t>(index)] = origin + static_cast<float>(index) * cell_size;
    }
}

bool SegmentHitsRoomBlockerFieldImpl(Vec3 a_world,
                                     Vec3 b_world,
                                     const RoomBlockerField& field,
                                     int also_skip_controller)
{
    if(!field.IsValid())
    {
        return false;
    }

    const float max_x = field.origin_x + static_cast<float>(field.width) * field.cell_size;
    const float max_y = field.origin_y + static_cast<float>(field.height) * field.cell_size;
    const float max_z = field.origin_z + static_cast<float>(field.depth) * field.cell_size;
    if(!SegmentIntersectsAabb(a_world, b_world, field.origin_x, field.origin_y, field.origin_z, max_x, max_y, max_z))
    {
        return false;
    }

    thread_local std::vector<float> x_edges;
    thread_local std::vector<float> y_edges;
    thread_local std::vector<float> z_edges;
    BuildUniformAxisEdges(field.origin_x, field.cell_size, field.width, x_edges);
    BuildUniformAxisEdges(field.origin_y, field.cell_size, field.height, y_edges);
    BuildUniformAxisEdges(field.origin_z, field.cell_size, field.depth, z_edges);

    const int skip_controller = SpatialLightingSceneProvider::instance()->shadingControllerIndex();
    const auto cell_blocks = [&](int x, int y, int z) {
        return RoomBlockerCellBlocks(field, x, y, z, skip_controller, also_skip_controller);
    };

    const Vec3 seg = {b_world.x - a_world.x, b_world.y - a_world.y, b_world.z - a_world.z};
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

    float px = a_world.x + dir.x * t_min;
    float py = a_world.y + dir.y * t_min;
    float pz = a_world.z + dir.z * t_min;

    int ix = FindCellIndex(px, x_edges);
    int iy = FindCellIndex(py, y_edges);
    int iz = FindCellIndex(pz, z_edges);
    if(ix < 0 || iy < 0 || iz < 0)
    {
        return false;
    }

    const int step_x = (dir.x > 0.0f) ? 1 : ((dir.x < 0.0f) ? -1 : 0);
    const int step_y = (dir.y > 0.0f) ? 1 : ((dir.y < 0.0f) ? -1 : 0);
    const int step_z = (dir.z > 0.0f) ? 1 : ((dir.z < 0.0f) ? -1 : 0);

    float t_x = (step_x > 0) ? NextBoundaryT(px, dir.x, x_edges[static_cast<size_t>(ix + 1)])
                             : NextBoundaryT(px, dir.x, x_edges[static_cast<size_t>(ix)]);
    float t_y = (step_y > 0) ? NextBoundaryT(py, dir.y, y_edges[static_cast<size_t>(iy + 1)])
                             : NextBoundaryT(py, dir.y, y_edges[static_cast<size_t>(iy)]);
    float t_z = (step_z > 0) ? NextBoundaryT(pz, dir.z, z_edges[static_cast<size_t>(iz + 1)])
                             : NextBoundaryT(pz, dir.z, z_edges[static_cast<size_t>(iz)]);

    const int max_steps = field.width + field.height + field.depth + 4;
    for(int step = 0; step < max_steps; ++step)
    {
        if(cell_blocks(ix, iy, iz))
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
                t_x = (step_x > 0) ? NextBoundaryT(px, dir.x, x_edges[static_cast<size_t>(ix + 1)])
                                   : NextBoundaryT(px, dir.x, x_edges[static_cast<size_t>(ix)]);
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
                t_z = (step_z > 0) ? NextBoundaryT(pz, dir.z, z_edges[static_cast<size_t>(iz + 1)])
                                   : NextBoundaryT(pz, dir.z, z_edges[static_cast<size_t>(iz)]);
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
                t_y = (step_y > 0) ? NextBoundaryT(py, dir.y, y_edges[static_cast<size_t>(iy + 1)])
                                   : NextBoundaryT(py, dir.y, y_edges[static_cast<size_t>(iy)]);
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
                t_z = (step_z > 0) ? NextBoundaryT(pz, dir.z, z_edges[static_cast<size_t>(iz + 1)])
                                   : NextBoundaryT(pz, dir.z, z_edges[static_cast<size_t>(iz)]);
            }
        }

        if(ix < 0 || iy < 0 || iz < 0 || ix >= field.width || iy >= field.height || iz >= field.depth)
        {
            break;
        }
    }

    return false;
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
                                 int also_skip_controller,
                                 const RoomBlockerField* merged_field)
{
    if(merged_field && merged_field->IsValid())
    {
        return SegmentHitsRoomBlockerFieldImpl(a, b, *merged_field, also_skip_controller);
    }

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

void MarkWorldAabbInRoomField(RoomBlockerField& field,
                              float min_x,
                              float min_y,
                              float min_z,
                              float max_x,
                              float max_y,
                              float max_z,
                              int controller_index)
{
    if(!field.IsValid() || controller_index < 0)
    {
        return;
    }

    const float inv = 1.0f / field.cell_size;
    int ix0 = static_cast<int>(std::floor((min_x - field.origin_x) * inv));
    int iy0 = static_cast<int>(std::floor((min_y - field.origin_y) * inv));
    int iz0 = static_cast<int>(std::floor((min_z - field.origin_z) * inv));
    int ix1 = static_cast<int>(std::floor((max_x - field.origin_x) * inv));
    int iy1 = static_cast<int>(std::floor((max_y - field.origin_y) * inv));
    int iz1 = static_cast<int>(std::floor((max_z - field.origin_z) * inv));

    ix0 = std::clamp(ix0, 0, field.width - 1);
    iy0 = std::clamp(iy0, 0, field.height - 1);
    iz0 = std::clamp(iz0, 0, field.depth - 1);
    ix1 = std::clamp(ix1, 0, field.width - 1);
    iy1 = std::clamp(iy1, 0, field.height - 1);
    iz1 = std::clamp(iz1, 0, field.depth - 1);

    const uint16_t owner = static_cast<uint16_t>(controller_index + 1);
    for(int iz = iz0; iz <= iz1; ++iz)
    {
        for(int iy = iy0; iy <= iy1; ++iy)
        {
            for(int ix = ix0; ix <= ix1; ++ix)
            {
                const size_t index =
                    static_cast<size_t>(ix) + static_cast<size_t>(iy) * static_cast<size_t>(field.width) +
                    static_cast<size_t>(iz) * static_cast<size_t>(field.width) * static_cast<size_t>(field.height);
                field.cells[index] = owner;
            }
        }
    }
}

} // namespace

void BuildRoomBlockerField(RoomBlockerField& out, float grid_scale_mm, const GridContext3D* clip_grid)
{
    out = RoomBlockerField{};
    const std::vector<std::unique_ptr<::ControllerTransform>>* transforms =
        SpatialLightingSceneProvider::instance()->controllers();
    if(!transforms)
    {
        return;
    }

    const float scale_mm = (grid_scale_mm > 0.001f) ? grid_scale_mm : 10.0f;
    float cell_size = std::max(0.25f, MMToGridUnits(8.0f, scale_mm));

    Vector3D bounds_min{std::numeric_limits<float>::max(),
                          std::numeric_limits<float>::max(),
                          std::numeric_limits<float>::max()};
    Vector3D bounds_max{std::numeric_limits<float>::lowest(),
                        std::numeric_limits<float>::lowest(),
                        std::numeric_limits<float>::lowest()};
    bool has_bounds = false;

    struct BlockerWorldBox
    {
        float min_x;
        float min_y;
        float min_z;
        float max_x;
        float max_y;
        float max_z;
        int controller_index;
    };
    std::vector<BlockerWorldBox> blocker_boxes;

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

        const Vector3D center_offset = ControllerLocalCenterOffset(ctrl);
        const int width = layout->GetWidth();
        const int height = layout->GetHeight();
        const int depth = layout->GetDepth();
        std::vector<float> x_edges;
        std::vector<float> y_edges;
        std::vector<float> z_edges;
        BuildAxisEdges(layout, scale_mm, width, &VirtualController3D::ColumnWidthMm, x_edges);
        BuildAxisEdges(layout, scale_mm, height, &VirtualController3D::RowHeightMm, y_edges);
        BuildAxisEdges(layout, scale_mm, depth, &VirtualController3D::LayerDepthMm, z_edges);

        for(const CustomControllerLightBlocker& blocker : blockers)
        {
            if(blocker.x < 0 || blocker.y < 0 || blocker.z < 0 || blocker.x >= width || blocker.y >= height ||
               blocker.z >= depth)
            {
                continue;
            }

            const Vector3D local_min = {x_edges[static_cast<size_t>(blocker.x)],
                                        y_edges[static_cast<size_t>(blocker.y)],
                                        z_edges[static_cast<size_t>(blocker.z)]};
            const Vector3D local_max = {x_edges[static_cast<size_t>(blocker.x + 1)],
                                        y_edges[static_cast<size_t>(blocker.y + 1)],
                                        z_edges[static_cast<size_t>(blocker.z + 1)]};

            Vector3D world_min{std::numeric_limits<float>::max(),
                             std::numeric_limits<float>::max(),
                             std::numeric_limits<float>::max()};
            Vector3D world_max{std::numeric_limits<float>::lowest(),
                             std::numeric_limits<float>::lowest(),
                             std::numeric_limits<float>::lowest()};
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
            for(const Vector3D& corner : local_corners)
            {
                const Vec3 world = ControllerLocalToWorld(ctrl, corner, center_offset);
                ExpandWorldBounds(&world_min, &world_max, world);
            }

            blocker_boxes.push_back({world_min.x,
                                     world_min.y,
                                     world_min.z,
                                     world_max.x,
                                     world_max.y,
                                     world_max.z,
                                     static_cast<int>(ctrl_index)});
            ExpandWorldBounds(&bounds_min, &bounds_max, {world_min.x, world_min.y, world_min.z});
            ExpandWorldBounds(&bounds_min, &bounds_max, {world_max.x, world_max.y, world_max.z});
            has_bounds = true;
        }
    }

    if(!has_bounds || blocker_boxes.empty())
    {
        return;
    }

    const float pad = cell_size * 0.5f;
    float origin_x = bounds_min.x - pad;
    float origin_y = bounds_min.y - pad;
    float origin_z = bounds_min.z - pad;
    float span_x = (bounds_max.x - bounds_min.x) + pad * 2.0f;
    float span_y = (bounds_max.y - bounds_min.y) + pad * 2.0f;
    float span_z = (bounds_max.z - bounds_min.z) + pad * 2.0f;

    if(clip_grid)
    {
        origin_x = std::max(origin_x, clip_grid->min_x - pad);
        origin_y = std::max(origin_y, clip_grid->min_y - pad);
        origin_z = std::max(origin_z, clip_grid->min_z - pad);
        const float clip_max_x = clip_grid->max_x + pad;
        const float clip_max_y = clip_grid->max_y + pad;
        const float clip_max_z = clip_grid->max_z + pad;
        span_x = std::min(span_x, clip_max_x - origin_x);
        span_y = std::min(span_y, clip_max_y - origin_y);
        span_z = std::min(span_z, clip_max_z - origin_z);
    }

    span_x = std::max(span_x, cell_size);
    span_y = std::max(span_y, cell_size);
    span_z = std::max(span_z, cell_size);

    int width = static_cast<int>(std::ceil(span_x / cell_size));
    int height = static_cast<int>(std::ceil(span_y / cell_size));
    int depth = static_cast<int>(std::ceil(span_z / cell_size));

    constexpr size_t kMaxCells = 4u * 1024u * 1024u;
    while(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth) > kMaxCells)
    {
        cell_size *= 2.0f;
        width = static_cast<int>(std::ceil(span_x / cell_size));
        height = static_cast<int>(std::ceil(span_y / cell_size));
        depth = static_cast<int>(std::ceil(span_z / cell_size));
    }

    if(width <= 0 || height <= 0 || depth <= 0)
    {
        return;
    }

    out.origin_x = origin_x;
    out.origin_y = origin_y;
    out.origin_z = origin_z;
    out.cell_size = cell_size;
    out.width = width;
    out.height = height;
    out.depth = depth;
    out.cells.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth), 0);

    for(const BlockerWorldBox& box : blocker_boxes)
    {
        MarkWorldAabbInRoomField(out,
                                 box.min_x,
                                 box.min_y,
                                 box.min_z,
                                 box.max_x,
                                 box.max_y,
                                 box.max_z,
                                 box.controller_index);
    }
}

bool SegmentHitsRoomBlockerField(float ax,
                                 float ay,
                                 float az,
                                 float bx,
                                 float by,
                                 float bz,
                                 const RoomBlockerField& field,
                                 int also_skip_controller)
{
    return SegmentHitsRoomBlockerFieldImpl({ax, ay, az}, {bx, by, bz}, field, also_skip_controller);
}

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
        for(const Vector3D& local_corner : local_corners)
        {
            const Vec3 world = ControllerLocalToWorld(ctrl, local_corner, grid.center_offset);
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
    const RoomBlockerField* merged_field = nullptr;
    SpatialLightingSceneProvider* provider = SpatialLightingSceneProvider::instance();
    if(provider && provider->frameRoomBlockerField().IsValid())
    {
        merged_field = &provider->frameRoomBlockerField();
    }
    return SegmentHitsBlockerGridsImpl({ax, ay, az}, {bx, by, bz}, grids, also_skip_controller, merged_field);
}

} // namespace SpatialLighting
