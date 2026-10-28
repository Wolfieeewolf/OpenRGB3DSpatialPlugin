// SPDX-License-Identifier: GPL-2.0-only

#ifndef OCCLUDERSPATIALINDEX_H
#define OCCLUDERSPATIALINDEX_H

#include "SpatialLighting/SpatialLightingEngine.h"

#include <cstdint>
#include <vector>

namespace SpatialLighting
{

/** Uniform grid over AABB occluders for sub-linear segment queries. */
class OccluderSpatialIndex
{
public:
    void Clear();
    void Build(const std::vector<OccluderAabb>& aabbs,
               float bounds_min_x,
               float bounds_min_y,
               float bounds_min_z,
               float bounds_max_x,
               float bounds_max_y,
               float bounds_max_z);

    bool IsBuilt() const { return !cells_.empty(); }

    /** Collect AABB indices whose grid cells overlap the segment bounding box (deduped). */
    void CollectSegmentCandidates(Vec3 a, Vec3 b, std::vector<uint16_t>& out_candidates) const;

    /** Collect AABB indices in an axis-aligned box (for short AO probes). */
    void CollectBoxCandidates(float min_x,
                              float min_y,
                              float min_z,
                              float max_x,
                              float max_y,
                              float max_z,
                              std::vector<uint16_t>& out_candidates) const;

private:
    int CellIndex(int ix, int iy, int iz) const;
    void AppendCellCandidates(int cell_index, std::vector<uint16_t>& out_candidates) const;

    float origin_x_ = 0.0f;
    float origin_y_ = 0.0f;
    float origin_z_ = 0.0f;
    float cell_size_ = 1.0f;
    int cells_x_ = 0;
    int cells_y_ = 0;
    int cells_z_ = 0;
    std::vector<std::vector<uint16_t>> cells_;
};

} // namespace SpatialLighting

#endif
