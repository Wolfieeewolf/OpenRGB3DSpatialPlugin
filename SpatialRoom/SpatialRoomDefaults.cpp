// SPDX-License-Identifier: GPL-2.0-only

#include "SpatialRoomDefaults.h"

#include "SpatialLighting/SpatialLightingEngine.h"

namespace SpatialRoom
{

SpatialRoomCapabilities DefaultCapabilitiesForMode(SpatialRoomMode mode)
{
    SpatialRoomCapabilities caps{};
    switch(mode)
    {
    case SpatialRoomMode::SpatialLighting:
    case SpatialRoomMode::EmissiveRelay:
        caps.set(CapSkipSampleWarp);
        caps.set(CapRoomGridCoordinates);
        caps.set(CapUseOcclusion);
        caps.set(CapUseAmbientOcclusion);
        break;
    case SpatialRoomMode::RoomMappedPattern:
        caps.set(CapSkipSampleWarp);
        caps.set(CapRoomGridCoordinates);
        caps.set(CapPreferLedOnlyIteration);
        break;
    case SpatialRoomMode::HologramVolume:
        caps.set(CapRoomGridCoordinates);
        caps.set(CapPreferLedOnlyIteration);
        break;
    case SpatialRoomMode::RoomMap:
        caps.set(CapRoomGridCoordinates);
        break;
    case SpatialRoomMode::AudioReactive:
        caps.set(CapRoomGridCoordinates);
        caps.set(CapPreferLedOnlyIteration);
        break;
    case SpatialRoomMode::SurfaceMedia:
        caps.set(CapRoomGridCoordinates);
        break;
    case SpatialRoomMode::DeviceStrip:
        break;
    case SpatialRoomMode::OriginField:
    default:
        break;
    }
    return caps;
}

void ApplyDepthPreset(SpatialRoomCapabilities& caps, SpatialRoomDepthPreset preset)
{
    switch(preset)
    {
    case SpatialRoomDepthPreset::Simple:
        caps.set(CapUseOcclusion, false);
        caps.set(CapUseAmbientOcclusion, false);
        break;
    case SpatialRoomDepthPreset::Quality:
        caps.set(CapUseOcclusion, true);
        caps.set(CapUseAmbientOcclusion, true);
        break;
    case SpatialRoomDepthPreset::Standard:
    default:
        caps.set(CapUseOcclusion, true);
        caps.set(CapUseAmbientOcclusion, true);
        break;
    }
}

void ApplyDepthPresetToShadeSettings(SpatialLighting::ShadeSettings& shade, SpatialRoomDepthPreset preset)
{
    switch(preset)
    {
    case SpatialRoomDepthPreset::Simple:
        shade.use_occlusion = false;
        shade.use_ambient_occlusion = false;
        break;
    case SpatialRoomDepthPreset::Quality:
        shade.use_occlusion = true;
        shade.use_ambient_occlusion = true;
        break;
    case SpatialRoomDepthPreset::Standard:
    default:
        shade.use_occlusion = true;
        shade.use_ambient_occlusion = true;
        break;
    }
}

const char* LibraryGroupForMode(SpatialRoomMode mode)
{
    switch(mode)
    {
    case SpatialRoomMode::SpatialLighting:
        return "Spatial · Lighting";
    case SpatialRoomMode::EmissiveRelay:
        return "Spatial · Relay";
    case SpatialRoomMode::RoomMappedPattern:
        return "Spatial · Mapped";
    case SpatialRoomMode::HologramVolume:
        return "Spatial · Volume";
    case SpatialRoomMode::RoomMap:
        return "Game · Room";
    case SpatialRoomMode::AudioReactive:
        return "Spatial · Audio";
    case SpatialRoomMode::SurfaceMedia:
        return "Media";
    case SpatialRoomMode::DeviceStrip:
        return "Device";
    case SpatialRoomMode::OriginField:
    default:
        return "Spatial";
    }
}

bool ModeUsesSpatialLightingEngine(SpatialRoomMode mode)
{
    return mode == SpatialRoomMode::SpatialLighting || mode == SpatialRoomMode::EmissiveRelay;
}

} // namespace SpatialRoom
