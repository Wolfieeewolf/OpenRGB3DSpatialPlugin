// SPDX-License-Identifier: GPL-2.0-only

#include "VoxelRoomCore.h"
#include "SpatialBasisUtils.h"
#include "SpatialCoordinateSpaces.h"

#include <algorithm>
#include <cstddef>
#include <cmath>

namespace VoxelRoomCore
{
namespace
{
static inline float Clamp01(float x)
{
    return std::clamp(x, 0.0f, 1.0f);
}

static inline int ClampByte(int v)
{
    return std::clamp(v, 0, 255);
}

static SpatialBasisUtils::BasisVectors ResolveBasis(const Basis& basis)
{
    return SpatialBasisUtils::BuildOrthonormalBasis(basis.forward_x,
                                                     basis.forward_y,
                                                     basis.forward_z,
                                                     basis.up_x,
                                                     basis.up_y,
                                                     basis.up_z);
}

static bool FetchVoxelRgba(const VoxelGrid& grid, int ix, int iy, int iz, float& r, float& g, float& b, float& a)
{
    if(ix < 0 || iy < 0 || iz < 0 || ix >= grid.size_x || iy >= grid.size_y || iz >= grid.size_z)
    {
        return false;
    }

    const size_t sx = (size_t)grid.size_x;
    const size_t sy = (size_t)grid.size_y;
    const size_t sz = (size_t)grid.size_z;
    const size_t ux = (size_t)ix;
    const size_t uy = (size_t)iy;
    const size_t uz = (size_t)iz;
    if(sx == 0 || sy == 0 || sz == 0 || ux >= sx || uy >= sy || uz >= sz)
    {
        return false;
    }

    const size_t voxel_index = ((ux * sy + uy) * sz + uz);
    if(voxel_index > (SIZE_MAX / 4u))
    {
        return false;
    }
    const size_t idx = voxel_index * 4u;
    if(idx + 3u >= grid.rgba.size())
    {
        return false;
    }

    r = (float)grid.rgba[idx + 0u] / 255.0f;
    g = (float)grid.rgba[idx + 1u] / 255.0f;
    b = (float)grid.rgba[idx + 2u] / 255.0f;
    a = (float)grid.rgba[idx + 3u] / 255.0f;
    return true;
}
}

static bool ExpectedVoxelRgbaByteCount(const VoxelGrid& g, size_t& out_count)
{
    if(g.size_x <= 0 || g.size_y <= 0 || g.size_z <= 0)
    {
        return false;
    }
    const size_t sx = (size_t)g.size_x;
    const size_t sy = (size_t)g.size_y;
    const size_t sz = (size_t)g.size_z;
    if(sx > (SIZE_MAX / sy))
    {
        return false;
    }
    const size_t xy = sx * sy;
    if(xy > (SIZE_MAX / sz))
    {
        return false;
    }
    const size_t xyz = xy * sz;
    if(xyz > (SIZE_MAX / 4u))
    {
        return false;
    }
    out_count = xyz * 4u;
    return true;
}

RGBColor ComputeRoomMappedVoxelColor(const VoxelGrid& grid,
                                     const Basis& basis,
                                     const RoomSamplePoint& sample,
                                     float anchor_world_x,
                                     float anchor_world_y,
                                     float anchor_world_z,
                                     const MapperSettings& settings,
                                     bool* out_used_voxel)
{
    if(out_used_voxel)
    {
        *out_used_voxel = false;
    }

    size_t expected_bytes = 0;
    if(!grid.valid ||
       grid.size_x <= 1 || grid.size_y <= 1 || grid.size_z <= 1 ||
       grid.voxel_size <= 1e-6f ||
       !ExpectedVoxelRgbaByteCount(grid, expected_bytes) ||
       grid.rgba.size() < expected_bytes)
    {
        return (RGBColor)0x00000000;
    }

    const SpatialBasisUtils::BasisVectors b = ResolveBasis(basis);

    const float blocks_per_grid = settings.room_to_world_scale;
    const SpatialCoordinateSpaces::RoomGridDelta local =
        SpatialCoordinateSpaces::RoomGridToPlayerLocalBlocks(sample.room_x,
                                                             sample.room_y,
                                                             sample.room_z,
                                                             sample.origin_x,
                                                             sample.origin_y,
                                                             sample.origin_z,
                                                             blocks_per_grid);

    float world_x = 0.0f;
    float world_y = 0.0f;
    float world_z = 0.0f;
    SpatialCoordinateSpaces::PlayerLocalBlocksToGameWorld(b,
                                                          anchor_world_x,
                                                          anchor_world_y,
                                                          anchor_world_z,
                                                          local,
                                                          world_x,
                                                          world_y,
                                                          world_z);

    const float gx = (world_x - grid.min_x) / grid.voxel_size;
    const float gy = (world_y - grid.min_y) / grid.voxel_size;
    const float gz = (world_z - grid.min_z) / grid.voxel_size;

    const int ix0 = (int)std::floor(gx);
    const int iy0 = (int)std::floor(gy);
    const int iz0 = (int)std::floor(gz);

    if(ix0 < 0 || iy0 < 0 || iz0 < 0 ||
       ix0 >= grid.size_x || iy0 >= grid.size_y || iz0 >= grid.size_z)
    {
        return (RGBColor)0x00000000;
    }

    const int ix1 = std::min(ix0 + 1, grid.size_x - 1);
    const int iy1 = std::min(iy0 + 1, grid.size_y - 1);
    const int iz1 = std::min(iz0 + 1, grid.size_z - 1);
    const float tx = gx - (float)ix0;
    const float ty = gy - (float)iy0;
    const float tz = gz - (float)iz0;

    if(settings.nearest_sample)
    {
        int ix = (int)std::floor(gx + 0.5f);
        int iy = (int)std::floor(gy + 0.5f);
        int iz = (int)std::floor(gz + 0.5f);
        ix = std::clamp(ix, 0, grid.size_x - 1);
        iy = std::clamp(iy, 0, grid.size_y - 1);
        iz = std::clamp(iz, 0, grid.size_z - 1);

        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 0.0f;
        if(FetchVoxelRgba(grid, ix, iy, iz, r, g, b, a) && a > settings.alpha_cutoff)
        {
            if(out_used_voxel)
            {
                *out_used_voxel = true;
            }
            const int r8 = ClampByte((int)std::lround(255.0f * Clamp01(r)));
            const int g8 = ClampByte((int)std::lround(255.0f * Clamp01(g)));
            const int b8 = ClampByte((int)std::lround(255.0f * Clamp01(b)));
            return (RGBColor)((b8 << 16) | (g8 << 8) | r8);
        }

        static const int kNeighborOffsets[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                                   {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
        for(const auto& off : kNeighborOffsets)
        {
            const int nx = ix + off[0];
            const int ny = iy + off[1];
            const int nz = iz + off[2];
            float nr = 0.0f;
            float ng = 0.0f;
            float nb = 0.0f;
            float na = 0.0f;
            if(FetchVoxelRgba(grid, nx, ny, nz, nr, ng, nb, na) && na > settings.alpha_cutoff)
            {
                if(out_used_voxel)
                {
                    *out_used_voxel = true;
                }
                const int r8 = ClampByte((int)std::lround(255.0f * Clamp01(nr)));
                const int g8 = ClampByte((int)std::lround(255.0f * Clamp01(ng)));
                const int b8 = ClampByte((int)std::lround(255.0f * Clamp01(nb)));
                return (RGBColor)((b8 << 16) | (g8 << 8) | r8);
            }
        }
        return (RGBColor)0x00000000;
    }

    float out_r = 0.0f;
    float out_g = 0.0f;
    float out_b = 0.0f;
    float out_a = 0.0f;

    for(int sx = 0; sx < 2; sx++)
    {
        const int ix = (sx == 0) ? ix0 : ix1;
        const float wx = (sx == 0) ? (1.0f - tx) : tx;
        for(int sy = 0; sy < 2; sy++)
        {
            const int iy = (sy == 0) ? iy0 : iy1;
            const float wy = (sy == 0) ? (1.0f - ty) : ty;
            for(int sz = 0; sz < 2; sz++)
            {
                const int iz = (sz == 0) ? iz0 : iz1;
                const float wz = (sz == 0) ? (1.0f - tz) : tz;
                const float w = wx * wy * wz;
                float r, g, b, a;
                if(FetchVoxelRgba(grid, ix, iy, iz, r, g, b, a))
                {
                    out_r += r * a * w;
                    out_g += g * a * w;
                    out_b += b * a * w;
                    out_a += a * w;
                }
            }
        }
    }

    if(out_a <= settings.alpha_cutoff)
    {
        return (RGBColor)0x00000000;
    }

    float inv = 1.0f;
    if(out_a > 1e-6f)
    {
        inv = 1.0f / out_a;
    }
    const int r8 = ClampByte((int)std::lround(255.0f * Clamp01(out_r * inv)));
    const int g8 = ClampByte((int)std::lround(255.0f * Clamp01(out_g * inv)));
    const int b8 = ClampByte((int)std::lround(255.0f * Clamp01(out_b * inv)));
    if(out_used_voxel)
    {
        *out_used_voxel = true;
    }
    return (RGBColor)((b8 << 16) | (g8 << 8) | r8);
}

}
