// SPDX-License-Identifier: GPL-2.0-only
// Room evaluation taxonomy: how an effect uses 3D layout (not game probe tinting).

#ifndef SPATIALROOMTYPES_H
#define SPATIALROOMTYPES_H

#include <cstdint>

namespace SpatialRoom
{

/** How an effect interprets room coordinates (library / evaluator family). */
enum class SpatialRoomMode : int
{
    /** Distance/angle from effect origin; classic spatial stack behavior. */
    OriginField = 0,
    /** Fixed lights + occluders via SpatialLighting::ShadeLed. */
    SpatialLighting = 1,
    /** Pattern in abstract space projected to room (Color Wheel 3D, disco ball, …). */
    RoomMappedPattern = 2,
    /** Volume/SDF/particle aesthetic (future). */
    HologramVolume = 3,
    /** Game telemetry / compass layers (SpatialSamplers + game bridge). */
    RoomMap = 4,
    /** FFT/beat-driven room placement (future dedicated path). */
    AudioReactive = 5,
    /** Display planes / capture-driven surfaces. */
    SurfaceMedia = 6,
    /** Strip-order or zone-local (minimal room use). */
    DeviceStrip = 7,
};

/** Cost/quality preset for room features (occlusion, AO). */
enum class SpatialRoomDepthPreset : int
{
    Simple = 0,
    Standard = 1,
    Quality = 2,
};

enum SpatialRoomCapability : uint32_t
{
    CapNone = 0,
    /** Do not warp sample XYZ with stack anchor/scale (room-fixed emitters). */
    CapSkipSampleWarp = 1u << 0,
    /** Ray-test against planes / controllers / blockers. */
    CapUseOcclusion = 1u << 1,
    /** Six-axis AO probes in SpatialLighting::ShadeLed. */
    CapUseAmbientOcclusion = 1u << 2,
    /** Hint: prefer iterating mapped LEDs only (small setups). */
    CapPreferLedOnlyIteration = 1u << 3,
    /** Sample in room grid bounds, not world-space LED coords. */
    CapRoomGridCoordinates = 1u << 4,
};

struct SpatialRoomCapabilities
{
    uint32_t flags = CapNone;

    bool has(SpatialRoomCapability cap) const { return (flags & cap) != 0; }
    void set(SpatialRoomCapability cap, bool on = true)
    {
        if(on)
        {
            flags |= cap;
        }
        else
        {
            flags &= ~cap;
        }
    }
};

/** Per-frame flags shared by the effect render pass (tab-owned). */
struct SpatialRoomFrameContext
{
    std::uint64_t render_sequence = 0;
    SpatialRoomDepthPreset depth_preset = SpatialRoomDepthPreset::Standard;
    bool room_grid_overlay_pass = false;
};

} // namespace SpatialRoom

#endif
