// SPDX-License-Identifier: GPL-2.0-only

#ifndef GAMEWRAPPERCAPTURE_H
#define GAMEWRAPPERCAPTURE_H

#include "Game/GameLightingBridge.h"
#include "Game/GameLightingFormat.h"
#include "RGBController.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct GridContext3D;

namespace GameWrapperCapture
{

/** Multi-select destinations for one Chroma channel (bit N = enum value N). */
enum class TargetDevice : int
{
    Off = 0, /**< sentinel — not stored as a bit */
    Keyboard = 1,
    Mouse = 2,
    Mousepad = 3,
    Headset = 4,
    Keypad = 5,
    RoomLights = 6, /**< any OpenRGB controller */
    WholeRoom = 7,
    Desk = 8,
    Floor = 9,
    Ceiling = 10,
    Left = 11,
    Right = 12,
};

using TargetMask = std::uint32_t;

inline constexpr TargetMask TargetBit(TargetDevice t)
{
    if(t == TargetDevice::Off)
    {
        return 0u;
    }
    return 1u << static_cast<unsigned>(t);
}

inline constexpr TargetMask kAllDestinationBits =
    TargetBit(TargetDevice::Keyboard) | TargetBit(TargetDevice::Mouse) | TargetBit(TargetDevice::Mousepad) |
    TargetBit(TargetDevice::Headset) | TargetBit(TargetDevice::Keypad) | TargetBit(TargetDevice::RoomLights) |
    TargetBit(TargetDevice::WholeRoom) | TargetBit(TargetDevice::Desk) | TargetBit(TargetDevice::Floor) |
    TargetBit(TargetDevice::Ceiling) | TargetBit(TargetDevice::Left) | TargetBit(TargetDevice::Right);

struct DeviceMapping
{
    bool enabled = true;
    TargetMask targets = 0;

    bool HasTarget(TargetDevice t) const
    {
        return (targets & TargetBit(t)) != 0;
    }

    void SetTarget(TargetDevice t, bool on)
    {
        if(t == TargetDevice::Off)
        {
            if(!on)
            {
                targets = 0;
            }
            return;
        }
        if(on)
        {
            targets |= TargetBit(t);
        }
        else
        {
            targets &= ~TargetBit(t);
        }
    }

    void ClearTargets()
    {
        targets = 0;
    }

    bool HasAnyTarget() const
    {
        return (targets & kAllDestinationBits) != 0;
    }

    std::vector<TargetDevice> ListedTargets() const
    {
        std::vector<TargetDevice> out;
        for(int i = 1; i <= static_cast<int>(TargetDevice::Right); ++i)
        {
            const auto t = static_cast<TargetDevice>(i);
            if(HasTarget(t))
            {
                out.push_back(t);
            }
        }
        return out;
    }
};

using DeviceMapArray = std::array<DeviceMapping, GameLightingFormat::kDeviceCount>;

struct Settings
{
    bool max_blend = true; /**< overlapping destinations keep the brighter colour */
    DeviceMapArray devices{};
    std::string active_game; /**< shim source basename */
    std::map<std::string, DeviceMapArray> game_profiles;

    Settings()
    {
        ApplyIdentityMap(devices);
    }

    static void ApplyIdentityMap(DeviceMapArray& out)
    {
        /* Keyboard on → whole room; other channels off (avoid ChromaLink wash). */
        for(std::uint8_t i = 0; i < GameLightingFormat::kDeviceCount; ++i)
        {
            out[i] = DeviceMapping{};
            out[i].enabled = false;
            out[i].targets = TargetBit(TargetDevice::WholeRoom);
        }
        out[static_cast<size_t>(GameLightingFormat::DeviceId::Keyboard)].enabled = true;
    }
};

bool IsPeripheralTarget(TargetDevice target);
bool IsSpatialTarget(TargetDevice target);
bool ControllerMatchesTarget(device_type type, TargetDevice target);
bool SpatialTargetApplies(TargetDevice target, float x01, float y01);

std::string NormalizeGameKey(const std::string& source);

void SyncActiveGameProfile(Settings& settings,
                           const std::string& live_source,
                           const GameLightingBridge::LightingSnapshot* snap_for_new = nullptr);

void ApplySeenChannelsDefault(DeviceMapArray& out, const GameLightingBridge::LightingSnapshot& snap);

void ApplyPresetDefault(Settings& s);
void ApplyPresetPeripheralIdentity(Settings& s);
void ApplyPresetAllWholeRoom(Settings& s);

RGBColor RenderColor(const GameLightingBridge::LightingSnapshot& snap,
                     float grid_x,
                     float grid_y,
                     float grid_z,
                     float origin_x,
                     float origin_y,
                     float origin_z,
                     const GridContext3D& grid,
                     const Settings& settings);

} // namespace GameWrapperCapture

#endif
