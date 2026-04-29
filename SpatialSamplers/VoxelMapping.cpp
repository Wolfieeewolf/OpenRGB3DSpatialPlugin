// SPDX-License-Identifier: GPL-2.0-only

#include "VoxelMapping.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace VoxelMapping
{
namespace
{
/** Room layout: Y up; horizontal framing uses yaw only (matches compass / voxel tint). */
static void BuildHorizontalVoxelBasis(float look_x,
                                      float look_y,
                                      float look_z,
                                      float heading_offset_deg,
                                      VoxelRoomCore::Basis& basis)
{
    float ux = 0.0f;
    float uy = 1.0f;
    float uz = 0.0f;
    float lx = look_x;
    float ly = look_y;
    float lz = look_z;
    float ll = std::sqrt(lx * lx + ly * ly + lz * lz);
    if(ll <= 1e-5f)
    {
        lx = 0.0f;
        ly = 0.0f;
        lz = 1.0f;
    }
    else
    {
        lx /= ll;
        ly /= ll;
        lz /= ll;
    }
    const float horiz = lx * ux + ly * uy + lz * uz;
    float fx = lx - horiz * ux;
    float fy = ly - horiz * uy;
    float fz = lz - horiz * uz;
    float fl = std::sqrt(fx * fx + fy * fy + fz * fz);
    if(fl <= 1e-5f)
    {
        fx = 0.0f;
        fy = 0.0f;
        fz = 1.0f;
    }
    else
    {
        fx /= fl;
        fy /= fl;
        fz /= fl;
    }
    float rx = fy * uz - fz * uy;
    float ry = fz * ux - fx * uz;
    float rz = fx * uy - fy * ux;
    float rl = std::sqrt(rx * rx + ry * ry + rz * rz);
    if(rl <= 1e-5f)
    {
        rx = 1.0f;
        ry = 0.0f;
        rz = 0.0f;
    }
    else
    {
        rx /= rl;
        ry /= rl;
        rz /= rl;
    }

    const float yaw = heading_offset_deg * 0.01745329251f;
    if(std::fabs(yaw) > 1e-5f)
    {
        const float c = std::cos(yaw);
        const float s = std::sin(yaw);
        const float fx2 = fx * c + rx * s;
        const float fy2 = fy * c + ry * s;
        const float fz2 = fz * c + rz * s;
        const float rx2 = rx * c - fx * s;
        const float ry2 = ry * c - fy * s;
        const float rz2 = rz * c - fz * s;
        fx = fx2;
        fy = fy2;
        fz = fz2;
        rx = rx2;
        ry = ry2;
        rz = rz2;
    }

    basis.forward_x = fx;
    basis.forward_y = fy;
    basis.forward_z = fz;
    basis.up_x = ux;
    basis.up_y = uy;
    basis.up_z = uz;
    basis.valid = true;
}
} // namespace

RGBColor SampleAtRoomGrid(const GameTelemetryBridge::TelemetrySnapshot& telemetry,
                          float heading_offset_deg,
                          float room_to_world_scale,
                          float alpha_cutoff,
                          float grid_x,
                          float grid_y,
                          float grid_z,
                          float origin_x,
                          float origin_y,
                          float origin_z,
                          bool* out_got_room_sample)
{
    thread_local VoxelRoomCore::VoxelGrid tl_grid;
    thread_local std::uint64_t tl_voxel_key = 0;

    if(out_got_room_sample)
    {
        *out_got_room_sample = false;
    }

    if(!telemetry.voxel_frame.has_voxel_frame)
    {
        return (RGBColor)0;
    }

    const auto& vf = telemetry.voxel_frame;
    const std::uint64_t vkey =
        (std::uint64_t)vf.frame_id << 32 | (std::uint32_t)(vf.received_ms & 0xFFFFFFFFu);

    if(vkey != tl_voxel_key || tl_grid.size_x != vf.size_x || tl_grid.size_y != vf.size_y ||
       tl_grid.size_z != vf.size_z)
    {
        tl_grid.valid = true;
        tl_grid.size_x = vf.size_x;
        tl_grid.size_y = vf.size_y;
        tl_grid.size_z = vf.size_z;
        tl_grid.min_x = vf.origin_x;
        tl_grid.min_y = vf.origin_y;
        tl_grid.min_z = vf.origin_z;
        tl_grid.voxel_size = std::max(1e-4f, vf.voxel_size);
        tl_grid.rgba = vf.rgba;
        tl_voxel_key = vkey;
    }

    if(!tl_grid.valid || tl_grid.size_x <= 1 || tl_grid.size_y <= 1 || tl_grid.size_z <= 1)
    {
        return (RGBColor)0;
    }

    VoxelRoomCore::Basis basis{};
    if(telemetry.has_player_pose)
    {
        BuildHorizontalVoxelBasis(telemetry.forward_x,
                                  telemetry.forward_y,
                                  telemetry.forward_z,
                                  heading_offset_deg,
                                  basis);
    }
    else
    {
        BuildHorizontalVoxelBasis(0.0f, 0.0f, 1.0f, heading_offset_deg, basis);
    }

    const float ax = telemetry.has_player_pose ? telemetry.player_x : 0.0f;
    const float ay = telemetry.has_player_pose ? telemetry.player_y : 0.0f;
    const float az = telemetry.has_player_pose ? telemetry.player_z : 0.0f;

    VoxelRoomCore::RoomSamplePoint sp{};
    sp.room_x = grid_x;
    sp.room_y = grid_y;
    sp.room_z = grid_z;
    sp.origin_x = origin_x;
    sp.origin_y = origin_y;
    sp.origin_z = origin_z;

    VoxelRoomCore::MapperSettings ms;
    ms.room_to_world_scale = std::clamp(room_to_world_scale, 0.02f, 0.80f);
    ms.alpha_cutoff = std::max(1e-4f, alpha_cutoff);

    bool used = false;
    RGBColor c = VoxelRoomCore::ComputeRoomMappedVoxelColor(tl_grid, basis, sp, ax, ay, az, ms, &used);
    if(!used)
    {
        return (RGBColor)0;
    }
    if(out_got_room_sample)
    {
        *out_got_room_sample = true;
    }
    return c;
}

} // namespace VoxelMapping
