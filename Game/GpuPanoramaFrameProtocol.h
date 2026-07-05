// SPDX-License-Identifier: GPL-2.0-only
//
// GPU-rendered cubemap panorama (6 faces) from the Minecraft client framebuffer.

#ifndef GPUPANORAMAFRAMEPROTOCOL_H
#define GPUPANORAMAFRAMEPROTOCOL_H

#include <cstddef>
#include <cstdint>

namespace GpuPanoramaFrameProtocol
{

constexpr std::uint32_t kMagic = 0x50475055u; // 'PGPU'
constexpr std::uint16_t kVersion = 1;
constexpr std::uint32_t kHeaderBytes = 64;
constexpr std::uint32_t kShmTotalBytes = 8388608u; // 8 MiB
constexpr std::uint32_t kFlagLz4 = 1u << 0;
constexpr std::uint32_t kFaceCount = 6u;
constexpr std::uint16_t kDefaultFaceSize = 256u;

#pragma pack(push, 1)
struct FrameHeader
{
    std::uint32_t magic;
    std::uint16_t version;
    std::uint16_t header_bytes;
    std::uint32_t sequence;
    std::uint32_t frame_id;
    std::uint64_t timestamp_ms;
    float anchor_x;
    float anchor_y;
    float anchor_z;
    std::uint16_t face_w;
    std::uint16_t face_h;
    std::uint8_t face_count;
    std::uint8_t reserved0;
    std::uint16_t reserved1;
    std::uint32_t rgba_raw_size;
    std::uint32_t rgba_stored_size;
    std::uint32_t flags;
    std::uint8_t reserved2[8];
};
#pragma pack(pop)

static_assert(sizeof(FrameHeader) == kHeaderBytes, "GpuPanoramaFrameProtocol::FrameHeader size mismatch");

inline bool TryComputeRgbaBytes(int face_w, int face_h, int face_count, std::size_t& out_bytes)
{
    if(face_w <= 0 || face_h <= 0 || face_count <= 0)
    {
        return false;
    }
    const std::size_t uw = (std::size_t)face_w;
    const std::size_t uh = (std::size_t)face_h;
    const std::size_t uf = (std::size_t)face_count;
    if(uw > (SIZE_MAX / uh) || uw * uh > (SIZE_MAX / uf) || uw * uh * uf > (SIZE_MAX / 4u))
    {
        return false;
    }
    out_bytes = uw * uh * uf * 4u;
    return true;
}

}

#endif
