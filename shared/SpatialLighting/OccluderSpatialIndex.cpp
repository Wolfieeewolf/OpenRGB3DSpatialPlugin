// SPDX-License-Identifier: GPL-2.0-only

#include "OccluderSpatialIndex.h"

#include <algorithm>
#include <cmath>

namespace SpatialLighting
{
namespace
{

constexpr int kMaxCellsPerAxis = 48;
constexpr float kMinCellSize = 0.35f;
constexpr float kMaxCellSize = 12.0f;

thread_local std::vector<uint32_t> g_candidate_stamp;
thread_local uint32_t g_candidate_generation = 1;

void MarkCandidate(uint16_t index, std::vector<uint16_t>& out)
{
    if(g_candidate_stamp.size() <= index)
    {
        g_candidate_stamp.resize(static_cast<size_t>(index) + 1u, 0u);
    }
    if(g_candidate_stamp[index] != g_candidate_generation)
    {
        g_candidate_stamp[index] = g_candidate_generation;
        out.push_back(index);
    }
}

void BumpCandidateGeneration()
{
    ++g_candidate_generation;
    if(g_candidate_generation == 0u)
    {
        std::fill(g_candidate_stamp.begin(), g_candidate_stamp.end(), 0u);
        g_candidate_generation = 1u;
    }
}

} // namespace

void OccluderSpatialIndex::Clear()
{
    origin_x_ = 0.0f;
    origin_y_ = 0.0f;
    origin_z_ = 0.0f;
    cell_size_ = 1.0f;
    cells_x_ = 0;
    cells_y_ = 0;
    cells_z_ = 0;
    cells_.clear();
}

int OccluderSpatialIndex::CellIndex(int ix, int iy, int iz) const
{
    if(ix < 0 || iy < 0 || iz < 0 || ix >= cells_x_ || iy >= cells_y_ || iz >= cells_z_)
    {
        return -1;
    }
    return ix + iy * cells_x_ + iz * cells_x_ * cells_y_;
}

void OccluderSpatialIndex::AppendCellCandidates(int cell_index, std::vector<uint16_t>& out_candidates) const
{
    if(cell_index < 0 || cell_index >= static_cast<int>(cells_.size()))
    {
        return;
    }
    for(uint16_t index : cells_[static_cast<size_t>(cell_index)])
    {
        MarkCandidate(index, out_candidates);
    }
}

void OccluderSpatialIndex::Build(const std::vector<OccluderAabb>& aabbs,
                                 float bounds_min_x,
                                 float bounds_min_y,
                                 float bounds_min_z,
                                 float bounds_max_x,
                                 float bounds_max_y,
                                 float bounds_max_z)
{
    Clear();
    if(aabbs.empty())
    {
        return;
    }

    float min_x = bounds_min_x;
    float min_y = bounds_min_y;
    float min_z = bounds_min_z;
    float max_x = bounds_max_x;
    float max_y = bounds_max_y;
    float max_z = bounds_max_z;
    for(const OccluderAabb& box : aabbs)
    {
        min_x = std::min(min_x, box.min.x);
        min_y = std::min(min_y, box.min.y);
        min_z = std::min(min_z, box.min.z);
        max_x = std::max(max_x, box.max.x);
        max_y = std::max(max_y, box.max.y);
        max_z = std::max(max_z, box.max.z);
    }

    const float span_x = std::max(max_x - min_x, kMinCellSize);
    const float span_y = std::max(max_y - min_y, kMinCellSize);
    const float span_z = std::max(max_z - min_z, kMinCellSize);
    const float max_span = std::max(span_x, std::max(span_y, span_z));

    cell_size_ = std::clamp(max_span / 24.0f, kMinCellSize, kMaxCellSize);
    origin_x_ = min_x;
    origin_y_ = min_y;
    origin_z_ = min_z;

    cells_x_ = std::clamp(static_cast<int>(std::ceil(span_x / cell_size_)), 1, kMaxCellsPerAxis);
    cells_y_ = std::clamp(static_cast<int>(std::ceil(span_y / cell_size_)), 1, kMaxCellsPerAxis);
    cells_z_ = std::clamp(static_cast<int>(std::ceil(span_z / cell_size_)), 1, kMaxCellsPerAxis);
    cells_.assign(static_cast<size_t>(cells_x_) * static_cast<size_t>(cells_y_) * static_cast<size_t>(cells_z_), {});

    for(uint16_t index = 0; index < aabbs.size(); ++index)
    {
        const OccluderAabb& box = aabbs[index];
        const int ix0 = std::max(0, static_cast<int>(std::floor((box.min.x - origin_x_) / cell_size_)));
        const int iy0 = std::max(0, static_cast<int>(std::floor((box.min.y - origin_y_) / cell_size_)));
        const int iz0 = std::max(0, static_cast<int>(std::floor((box.min.z - origin_z_) / cell_size_)));
        const int ix1 = std::min(cells_x_ - 1, static_cast<int>(std::floor((box.max.x - origin_x_) / cell_size_)));
        const int iy1 = std::min(cells_y_ - 1, static_cast<int>(std::floor((box.max.y - origin_y_) / cell_size_)));
        const int iz1 = std::min(cells_z_ - 1, static_cast<int>(std::floor((box.max.z - origin_z_) / cell_size_)));

        for(int iz = iz0; iz <= iz1; ++iz)
        {
            for(int iy = iy0; iy <= iy1; ++iy)
            {
                for(int ix = ix0; ix <= ix1; ++ix)
                {
                    const int cell = CellIndex(ix, iy, iz);
                    if(cell >= 0)
                    {
                        cells_[static_cast<size_t>(cell)].push_back(index);
                    }
                }
            }
        }
    }
}

void OccluderSpatialIndex::CollectBoxCandidates(float min_x,
                                                float min_y,
                                                float min_z,
                                                float max_x,
                                                float max_y,
                                                float max_z,
                                                std::vector<uint16_t>& out_candidates) const
{
    if(cells_.empty())
    {
        return;
    }

    BumpCandidateGeneration();

    const int ix0 = std::max(0, static_cast<int>(std::floor((min_x - origin_x_) / cell_size_)));
    const int iy0 = std::max(0, static_cast<int>(std::floor((min_y - origin_y_) / cell_size_)));
    const int iz0 = std::max(0, static_cast<int>(std::floor((min_z - origin_z_) / cell_size_)));
    const int ix1 = std::min(cells_x_ - 1, static_cast<int>(std::floor((max_x - origin_x_) / cell_size_)));
    const int iy1 = std::min(cells_y_ - 1, static_cast<int>(std::floor((max_y - origin_y_) / cell_size_)));
    const int iz1 = std::min(cells_z_ - 1, static_cast<int>(std::floor((max_z - origin_z_) / cell_size_)));

    for(int iz = iz0; iz <= iz1; ++iz)
    {
        for(int iy = iy0; iy <= iy1; ++iy)
        {
            for(int ix = ix0; ix <= ix1; ++ix)
            {
                AppendCellCandidates(CellIndex(ix, iy, iz), out_candidates);
            }
        }
    }
}

void OccluderSpatialIndex::CollectSegmentCandidates(Vec3 a, Vec3 b, std::vector<uint16_t>& out_candidates) const
{
    if(cells_.empty())
    {
        return;
    }

    const float min_x = std::min(a.x, b.x);
    const float min_y = std::min(a.y, b.y);
    const float min_z = std::min(a.z, b.z);
    const float max_x = std::max(a.x, b.x);
    const float max_y = std::max(a.y, b.y);
    const float max_z = std::max(a.z, b.z);
    const float pad = cell_size_ * 0.5f;
    CollectBoxCandidates(min_x - pad, min_y - pad, min_z - pad, max_x + pad, max_y + pad, max_z + pad, out_candidates);
}

} // namespace SpatialLighting
