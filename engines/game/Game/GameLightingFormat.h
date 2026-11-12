// SPDX-License-Identifier: GPL-2.0-only
// Game-wrapper UDP frames → GameLightingBridge (127.0.0.1:9877).
// Keep tools/game_wrappers/common/gw_udp_emit.* (shared by every vendor shim) in sync with this layout.

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
    DeviceGrid = 2, /**< RGB888 matrix; value 2 is on-wire stable */
};

enum class DeviceId : std::uint8_t
{
    Keyboard = 0,
    Mouse = 1,
    Mousepad = 2,
    Headset = 3,
    Keypad = 4,
    ChromaLink = 5,
    Count = 6,
};

inline constexpr std::uint8_t kDeviceCount = static_cast<std::uint8_t>(DeviceId::Count);

/** Keyboard 6×22; mouse CUSTOM2 is 9×7 — keep headroom for future devices. */
inline constexpr std::uint8_t kMaxGridRows = 16;
inline constexpr std::uint8_t kMaxGridCols = 32;
inline constexpr std::uint16_t kMaxGridCells = static_cast<std::uint16_t>(kMaxGridRows) * kMaxGridCols;

inline constexpr std::uint8_t kDeviceGridFlagActive = 1u;

#pragma pack(push, 1)

/**
 * Full matrix for one device. Payload is rows*cols RGB triplets (R,G,B bytes)
 * immediately after this header. Max rows*cols = kMaxGridCells.
 */
struct DeviceGridHeader
{
    char magic[4];
    std::uint32_t sequence;
    std::uint64_t timestamp_ms;
    std::uint8_t frame_type; /**< FrameType::DeviceGrid */
    char source[24];
    std::uint8_t device_id; /**< DeviceId */
    std::uint8_t rows;
    std::uint8_t cols;
    std::uint8_t flags; /**< kDeviceGridFlagActive */
};

#pragma pack(pop)

inline constexpr std::size_t kDeviceGridHeaderSize = sizeof(DeviceGridHeader);
inline constexpr std::size_t kDeviceGridMaxFrameSize = kDeviceGridHeaderSize + (std::size_t)kMaxGridCells * 3u;

static_assert(kDeviceGridHeaderSize == 45, "DeviceGridHeader size");

inline bool IsMagic(const char* data, std::size_t size)
{
    return size >= 4 && data[0] == kMagic[0] && data[1] == kMagic[1] && data[2] == kMagic[2] &&
           data[3] == kMagic[3];
}

inline const char* DeviceName(DeviceId id)
{
    switch(id)
    {
        case DeviceId::Keyboard: return "Keyboard";
        case DeviceId::Mouse: return "Mouse";
        case DeviceId::Mousepad: return "Mousepad";
        case DeviceId::Headset: return "Headset";
        case DeviceId::Keypad: return "Keypad";
        case DeviceId::ChromaLink: return "ChromaLink";
        default: return "Unknown";
    }
}

} // namespace GameLightingFormat

#endif
