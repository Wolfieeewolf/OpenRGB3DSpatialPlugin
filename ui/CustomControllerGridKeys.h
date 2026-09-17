// SPDX-License-Identifier: GPL-2.0-only

#ifndef CUSTOMCONTROLLERGRIDKEYS_H
#define CUSTOMCONTROLLERGRIDKEYS_H

#include <cstdint>

/** Packed (x, y, z) grid cell key for custom controller layers. */
inline uint64_t GridCellKey3D(int x, int y, int z)
{
    return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 42)
           | (static_cast<uint64_t>(static_cast<uint32_t>(y)) << 21)
           | static_cast<uint64_t>(static_cast<uint32_t>(z));
}

inline void DecodeGridCellKey3D(uint64_t key, int* out_x, int* out_y, int* out_z)
{
    if(out_x)
    {
        *out_x = static_cast<int>(static_cast<uint32_t>(key >> 42));
    }
    if(out_y)
    {
        *out_y = static_cast<int>(static_cast<uint32_t>(key >> 21) & 0x1FFFFFu);
    }
    if(out_z)
    {
        *out_z = static_cast<int>(static_cast<uint32_t>(key & 0x1FFFFFu));
    }
}

#endif
