// SPDX-License-Identifier: GPL-2.0-only

#include "VoxelRoomCore.h"

#include <algorithm>
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

static bool ResolveBasis(const Basis& basis,
                         float& out_rx,
                         float& out_ry,
                         float& out_rz,
                         float& out_ux,
                         float& out_uy,
                         float& out_uz,
                         float& out_fx,
                         float& out_fy,
                         float& out_fz)
{
    out_ux = basis.up_x;
    out_uy = basis.up_y;
    out_uz = basis.up_z;
    float ul = std::sqrt(out_ux * out_ux + out_uy * out_uy + out_uz * out_uz);
    if(ul <= 1e-6f)
    {
        out_ux = 0.0f;
        out_uy = 1.0f;
        out_uz = 0.0f;
    }
    else
    {
        out_ux /= ul;
        out_uy /= ul;
        out_uz /= ul;
    }

    out_fx = basis.forward_x;
    out_fy = basis.forward_y;
    out_fz = basis.forward_z;
    float fl = std::sqrt(out_fx * out_fx + out_fy * out_fy + out_fz * out_fz);
    if(fl <= 1e-6f)
    {
        out_fx = 0.0f;
        out_fy = 0.0f;
        out_fz = 1.0f;
    }
    else
    {
        out_fx /= fl;
        out_fy /= fl;
        out_fz /= fl;
    }

    const float proj = out_fx * out_ux + out_fy * out_uy + out_fz * out_uz;
    out_fx -= proj * out_ux;
    out_fy -= proj * out_uy;
    out_fz -= proj * out_uz;
    fl = std::sqrt(out_fx * out_fx + out_fy * out_fy + out_fz * out_fz);
    if(fl <= 1e-6f)
    {
        out_fx = 0.0f;
        out_fy = 0.0f;
        out_fz = 1.0f;
    }
    else
    {
        out_fx /= fl;
        out_fy /= fl;
        out_fz /= fl;
    }

    out_rx = out_fy * out_uz - out_fz * out_uy;
    out_ry = out_fz * out_ux - out_fx * out_uz;
    out_rz = out_fx * out_uy - out_fy * out_ux;
    float rl = std::sqrt(out_rx * out_rx + out_ry * out_ry + out_rz * out_rz);
    if(rl <= 1e-6f)
    {
        out_rx = 1.0f;
        out_ry = 0.0f;
        out_rz = 0.0f;
    }
    else
    {
        out_rx /= rl;
        out_ry /= rl;
        out_rz /= rl;
    }

    return basis.valid;
}

static bool FetchVoxelRgba(const VoxelGrid& grid, int ix, int iy, int iz, float& r, float& g, float& b, float& a)
{
    if(ix < 0 || iy < 0 || iz < 0 || ix >= grid.size_x || iy >= grid.size_y || iz >= grid.size_z)
    {
        return false;
    }

    const int idx = ((ix * grid.size_y + iy) * grid.size_z + iz) * 4;
    if(idx < 0 || idx + 3 >= (int)grid.rgba.size())
    {
        return false;
    }

    r = (float)grid.rgba[(size_t)(idx + 0)] / 255.0f;
    g = (float)grid.rgba[(size_t)(idx + 1)] / 255.0f;
    b = (float)grid.rgba[(size_t)(idx + 2)] / 255.0f;
    a = (float)grid.rgba[(size_t)(idx + 3)] / 255.0f;
    return true;
}
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

    if(!grid.valid ||
       grid.size_x <= 1 || grid.size_y <= 1 || grid.size_z <= 1 ||
       grid.voxel_size <= 1e-6f ||
       grid.rgba.size() < (size_t)(grid.size_x * grid.size_y * grid.size_z * 4))
    {
        return (RGBColor)0x00000000;
    }

    float rx, ry, rz, ux, uy, uz, fx, fy, fz;
    ResolveBasis(basis, rx, ry, rz, ux, uy, uz, fx, fy, fz);

    const float dx = (sample.room_x - sample.origin_x) * settings.room_to_world_scale;
    const float dy = (sample.room_y - sample.origin_y) * settings.room_to_world_scale;
    const float dz = (sample.room_z - sample.origin_z) * settings.room_to_world_scale;

    const float world_x = anchor_world_x + dx * rx + dy * ux + dz * fx;
    const float world_y = anchor_world_y + dx * ry + dy * uy + dz * fy;
    const float world_z = anchor_world_z + dx * rz + dy * uz + dz * fz;

    const float gx = (world_x - grid.min_x) / grid.voxel_size;
    const float gy = (world_y - grid.min_y) / grid.voxel_size;
    const float gz = (world_z - grid.min_z) / grid.voxel_size;

    const int ix0 = (int)std::floor(gx);
    const int iy0 = (int)std::floor(gy);
    const int iz0 = (int)std::floor(gz);
    const int ix1 = ix0 + 1;
    const int iy1 = iy0 + 1;
    const int iz1 = iz0 + 1;
    const float tx = gx - (float)ix0;
    const float ty = gy - (float)iy0;
    const float tz = gz - (float)iz0;

    if(ix0 < 0 || iy0 < 0 || iz0 < 0 ||
       ix1 >= grid.size_x || iy1 >= grid.size_y || iz1 >= grid.size_z)
    {
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
