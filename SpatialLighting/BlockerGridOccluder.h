// SPDX-License-Identifier: GPL-2.0-only

#ifndef BLOCKERGRIDOCCLUDER_H
#define BLOCKERGRIDOCCLUDER_H

#include "LEDPosition3D.h"

#include <cstdint>
#include <vector>

struct ControllerTransform;

namespace SpatialLighting
{

/** Per-virtual-controller blocker occupancy for O(path) ray tests instead of per-cell AABBs. */
struct BlockerGridOccluder
{
    int controller_index = -1;
    Vector3D center_offset{};
    Vector3D world_min{};
    Vector3D world_max{};
    int width = 0;
    int height = 0;
    int depth = 0;
    std::vector<float> x_edges{};
    std::vector<float> y_edges{};
    std::vector<float> z_edges{};
    /** width * height * depth; 1 = blocked cell. */
    std::vector<uint8_t> dense_cells{};
};

void BuildBlockerGridOccluders(std::vector<BlockerGridOccluder>& out, float grid_scale_mm);

bool SegmentHitsBlockerGrids(float ax,
                             float ay,
                             float az,
                             float bx,
                             float by,
                             float bz,
                             const std::vector<BlockerGridOccluder>& grids,
                             int also_skip_controller = -1);

} // namespace SpatialLighting

#endif
