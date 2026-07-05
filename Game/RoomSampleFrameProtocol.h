// SPDX-License-Identifier: GPL-2.0-only
//
// Room-directed sampling: plugin publishes room grid config; MC mod raycasts only those cells.

#ifndef ROOMSAMPLEFRAMEPROTOCOL_H
#define ROOMSAMPLEFRAMEPROTOCOL_H

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace RoomSampleFrameProtocol
{

constexpr std::uint32_t kConfigMagic = 0x52434647u; // 'RCFG'
constexpr std::uint32_t kFrameMagic = 0x5253414Du;   // 'RSAM'
constexpr std::uint16_t kVersion = 1;
constexpr std::uint32_t kConfigHeaderBytes = 128;
constexpr std::uint32_t kFrameHeaderBytes = 64;
constexpr std::uint32_t kShmTotalBytes = 4194304u; // 4 MiB (480k RGBA compresses via LZ4)
constexpr std::uint32_t kFlagLz4 = 1u << 0;
constexpr std::uint32_t kFlagEnabled = 1u << 1;
constexpr std::size_t kDefaultTargetCells = 800u * 600u;
constexpr std::size_t kMaxCells = 512000u;

#pragma pack(push, 1)
struct ConfigHeader
{
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t header_bytes;
    std::uint32_t sequence;
    std::uint32_t config_id;
    std::uint32_t flags;
    std::int32_t size_x;
    std::int32_t size_y;
    std::int32_t size_z;
    float room_min_x;
    float room_min_y;
    float room_min_z;
    float room_max_x;
    float room_max_y;
    float room_max_z;
    float effect_origin_x;
    float effect_origin_y;
    float effect_origin_z;
    float room_to_world_scale;
    float heading_offset_deg;
    float pos_offset_forward_blocks;
    float pos_offset_right_blocks;
    float pos_offset_up_blocks;
    std::uint32_t target_cells;
    std::uint8_t reserved[36];
};

struct FrameHeader
{
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t header_bytes;
    std::uint32_t sequence;
    std::uint32_t frame_id;
    std::uint64_t timestamp_ms;
    std::uint32_t config_id;
    std::int32_t size_x;
    std::int32_t size_y;
    std::int32_t size_z;
    std::uint32_t rgba_raw_size;
    std::uint32_t rgba_stored_size;
    std::uint32_t flags;
    std::uint8_t reserved[12];
};
#pragma pack(pop)

static_assert(sizeof(ConfigHeader) == kConfigHeaderBytes, "RoomSampleFrameProtocol::ConfigHeader size mismatch");
static_assert(sizeof(FrameHeader) == kFrameHeaderBytes, "RoomSampleFrameProtocol::FrameHeader size mismatch");

inline bool TryComputeRgbaBytes(int sx, int sy, int sz, std::size_t& out_bytes)
{
    if(sx <= 0 || sy <= 0 || sz <= 0)
    {
        return false;
    }
    const std::size_t ux = (std::size_t)sx;
    const std::size_t uy = (std::size_t)sy;
    const std::size_t uz = (std::size_t)sz;
    if(ux > (SIZE_MAX / uy) || ux * uy > (SIZE_MAX / uz) || ux * uy * uz > (SIZE_MAX / 4u))
    {
        return false;
    }
    out_bytes = ux * uy * uz * 4u;
    return true;
}

inline void ComputeGridDimensions(float span_x,
                                  float span_y,
                                  float span_z,
                                  int& out_nx,
                                  int& out_ny,
                                  int& out_nz,
                                  std::size_t target_cells = kDefaultTargetCells)
{
    const float sx = std::max(1e-3f, span_x);
    const float sy = std::max(1e-3f, span_y);
    const float sz = std::max(1e-3f, span_z);
    const double volume = (double)sx * (double)sy * (double)sz;
    target_cells = std::min(target_cells, kMaxCells);
    target_cells = std::max(target_cells, (std::size_t)64);

    const double density = std::cbrt((double)target_cells / volume);
    out_nx = std::clamp((int)std::ceil((double)sx * density), 4, 512);
    out_ny = std::clamp((int)std::ceil((double)sy * density), 4, 384);
    out_nz = std::clamp((int)std::ceil((double)sz * density), 4, 512);

    auto cell_count = [&]() -> std::size_t {
        return (std::size_t)out_nx * (std::size_t)out_ny * (std::size_t)out_nz;
    };

    while(cell_count() > target_cells && cell_count() > 64u)
    {
        if(out_nx >= out_ny && out_nx >= out_nz && out_nx > 4)
        {
            out_nx--;
        }
        else if(out_ny >= out_nz && out_ny > 4)
        {
            out_ny--;
        }
        else if(out_nz > 4)
        {
            out_nz--;
        }
        else
        {
            break;
        }
    }
}

}

#endif
