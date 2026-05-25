// SPDX-License-Identifier: GPL-2.0-only
// Binary frames from tools/game_wrappers/* shims → plugin listener (UDP 9877).

#ifndef GAMELIGHTINGFORMAT_H
#define GAMELIGHTINGFORMAT_H

#include <cstddef>
#include <cstdint>

namespace GameLightingFormat
{

inline constexpr std::uint16_t kListenPort = 9877;
inline constexpr char kMagic[4] = {'G', 'W', '0', '1'};

enum class FrameType : std::uint8_t
{
    UniformRgb = 0,
};

#pragma pack(push, 1)

/** v0 — one RGB applied to the full capture grid (typical Chroma “all devices”). */
struct UniformRgbFrame
{
    char magic[4];
    std::uint32_t sequence;
    std::uint64_t timestamp_ms;
    std::uint8_t frame_type;
    char source[24];
    float r;
    float g;
    float b;
};

#pragma pack(pop)

inline constexpr std::size_t kUniformRgbFrameSize = sizeof(UniformRgbFrame);

inline bool IsMagic(const char* data, std::size_t size)
{
    return size >= 4 && data[0] == kMagic[0] && data[1] == kMagic[1] && data[2] == kMagic[2] &&
           data[3] == kMagic[3];
}

} // namespace GameLightingFormat

#endif
