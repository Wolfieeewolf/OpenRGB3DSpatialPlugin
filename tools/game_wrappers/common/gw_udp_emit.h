// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <cstdint>

namespace GwUdp
{

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

void SetDeviceColorRef(DeviceId id, std::uint32_t color, const char* source);

/** Full COLORREF grid (row-major) as GW01 DeviceGrid. */
void SetDeviceGridColorRef(DeviceId id,
                           const std::uint32_t* colors,
                           int rows,
                           int cols,
                           const char* source);

void FlushDevices(const char* source);
void ClearDevices();

} // namespace GwUdp
