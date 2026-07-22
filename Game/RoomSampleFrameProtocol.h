// SPDX-License-Identifier: GPL-2.0-only
//
// Room Ambilight: OpenGL-style cubemap from the eye, sampled per mapped LED by room direction
// onto the existing controllers/grid. Rays stay within the published room bounds.
// Not equirect panorama. OpenRGB stays OpenGL — no Vulkan.

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
constexpr std::uint32_t kShmTotalBytes = 8388608u; // 8 MiB — headroom for 512²×6 sparse cubemaps
constexpr std::uint32_t kFlagLz4 = 1u << 0;
constexpr std::uint32_t kFlagEnabled = 1u << 1;
/** Config file has important_cell_count in reserved[0..3] and that many uint32 flat indices after the header. */
constexpr std::uint32_t kFlagImportantCells = 1u << 2;
/** Mod may fill open-air LED rays with outdoor sky/weather atmosphere. */
constexpr std::uint32_t kFlagSkyEnabled = 1u << 3;
/**
 * Cubemap frame: size_x == size_y == face resolution, size_z == 6.
 * Face order (iz): +X,-X,+Y,-Y,+Z,-Z in player-local space (right, left, up, down, forward, back).
 * Flat texel: ((u * face_size + v) * 6 + face) * 4  with u,v in [0, face_size).
 * Only LED-covered texels are filled (important indices); the rest stay unused.
 */
constexpr std::uint32_t kFlagCubemap = 1u << 4;
/** Mirror player-local +X (right ↔ left) — room layout vs look alignment. */
constexpr std::uint32_t kFlagFlipRight = 1u << 5;
/** Mirror player-local +Z (forward ↔ back). Room +Z is behind the origin by default. */
constexpr std::uint32_t kFlagFlipForward = 1u << 6;
constexpr std::size_t kDefaultTargetCells = 800u * 600u;
constexpr std::size_t kMaxCells = 512000u;
constexpr std::uint32_t kMaxImportantCells = 16384u;
constexpr int kCubemapFaceCount = 6;
/** Default face size — LED-first cost is O(LEDs), so 128 is precision not fill cost. */
constexpr int kCubemapFaceSizeDefault = 128;
constexpr int kCubemapFaceSizeMax = 512;
/** reserved[0..3] = important count; reserved[4..7] = UV texture max dim (plugin → mod). */
constexpr std::size_t kReservedImportantCountOffset = 0;
constexpr std::size_t kReservedUvMaxDimOffset = 4;
constexpr int kUvTextureDimMin = 64;
constexpr int kUvTextureDimMax = 4096;
constexpr int kUvTextureDimDefault = 512;

inline int SnapUvTextureDim(int dim)
{
    if(dim <= 0)
    {
        return kUvTextureDimDefault;
    }
    // Stops from light → 4K experiment (not HD texture-pack faces on the cubemap).
    static const int kSteps[] = {64, 128, 256, 512, 720, 1024, 1080, 2048, 4096};
    int best = kUvTextureDimDefault;
    int bestDist = 100000;
    for(int step : kSteps)
    {
        const int d = std::abs(dim - step);
        if(d < bestDist)
        {
            bestDist = d;
            best = step;
        }
    }
    return best;
}

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

inline bool IsCubemapLayout(int sx, int sy, int sz, std::uint32_t flags)
{
    return (flags & kFlagCubemap) != 0 && sx > 0 && sx == sy && sz == kCubemapFaceCount
           && sx <= kCubemapFaceSizeMax;
}

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

/** Map a unit direction in player-local space to cubemap face + u,v in [0,1]. */
inline void DirectionToCubemapUv(float dx, float dy, float dz, int& face, float& u, float& v)
{
    const float ax = std::fabs(dx);
    const float ay = std::fabs(dy);
    const float az = std::fabs(dz);
    if(ax >= ay && ax >= az)
    {
        if(dx >= 0.0f)
        {
            face = 0; // +X
            u = 0.5f + (-dz / ax) * 0.5f;
            v = 0.5f + (-dy / ax) * 0.5f;
        }
        else
        {
            face = 1; // -X
            u = 0.5f + (dz / ax) * 0.5f;
            v = 0.5f + (-dy / ax) * 0.5f;
        }
    }
    else if(ay >= ax && ay >= az)
    {
        if(dy >= 0.0f)
        {
            face = 2; // +Y
            u = 0.5f + (dx / ay) * 0.5f;
            v = 0.5f + (dz / ay) * 0.5f;
        }
        else
        {
            face = 3; // -Y
            u = 0.5f + (dx / ay) * 0.5f;
            v = 0.5f + (-dz / ay) * 0.5f;
        }
    }
    else
    {
        if(dz >= 0.0f)
        {
            face = 4; // +Z forward
            u = 0.5f + (dx / az) * 0.5f;
            v = 0.5f + (-dy / az) * 0.5f;
        }
        else
        {
            face = 5; // -Z back
            u = 0.5f + (-dx / az) * 0.5f;
            v = 0.5f + (-dy / az) * 0.5f;
        }
    }
    u = std::clamp(u, 0.0f, 1.0f);
    v = std::clamp(v, 0.0f, 1.0f);
}

/** Face texel center → unit direction (must match DirectionToCubemapUv). */
inline void CubemapUvToDirection(int face, float u, float v, float& dx, float& dy, float& dz)
{
    const float s = u * 2.0f - 1.0f;
    const float t = v * 2.0f - 1.0f;
    switch(face)
    {
        case 0: dx = 1.0f;  dy = -t;    dz = -s;    break; // +X
        case 1: dx = -1.0f; dy = -t;    dz = s;     break; // -X
        case 2: dx = s;     dy = 1.0f;  dz = t;     break; // +Y
        case 3: dx = s;     dy = -1.0f; dz = -t;    break; // -Y
        case 4: dx = s;     dy = -t;    dz = 1.0f;  break; // +Z
        case 5: dx = -s;    dy = -t;    dz = -1.0f; break; // -Z
        default: dx = 0.0f; dy = 0.0f; dz = 1.0f; break;
    }
    const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if(len > 1e-6f)
    {
        dx /= len;
        dy /= len;
        dz /= len;
    }
}

}

#endif
