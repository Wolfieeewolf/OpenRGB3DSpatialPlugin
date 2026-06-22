// SPDX-License-Identifier: GPL-2.0-only

#include "VoxelMapping.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>

namespace VoxelMapping
{
namespace
{
static unsigned long long NowMs()
{
    return (unsigned long long)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

static float SmoothStep01(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static RGBColor LerpRgbColor(RGBColor a, RGBColor b, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    const int ar = (int)(a & 0xFF);
    const int ag = (int)((a >> 8) & 0xFF);
    const int ab = (int)((a >> 16) & 0xFF);
    const int br = (int)(b & 0xFF);
    const int bg = (int)((b >> 8) & 0xFF);
    const int bb = (int)((b >> 16) & 0xFF);
    const int r = (int)std::lround(ar + (br - ar) * t);
    const int g = (int)std::lround(ag + (bg - ag) * t);
    const int bl = (int)std::lround(ab + (bb - ab) * t);
    return (RGBColor)(((bl & 0xFF) << 16) | ((g & 0xFF) << 8) | (r & 0xFF));
}

static void CopyVoxelFrameToGrid(const GameTelemetryBridge::VoxelFrameChannel& vf, VoxelRoomCore::VoxelGrid& grid)
{
    grid.valid = true;
    grid.size_x = vf.size_x;
    grid.size_y = vf.size_y;
    grid.size_z = vf.size_z;
    grid.min_x = vf.origin_x;
    grid.min_y = vf.origin_y;
    grid.min_z = vf.origin_z;
    grid.voxel_size = std::max(1e-4f, vf.voxel_size);
    grid.rgba = vf.rgba;
}

static bool GridTopologyMatches(const VoxelRoomCore::VoxelGrid& a, const VoxelRoomCore::VoxelGrid& b)
{
    return a.valid && b.valid && a.size_x == b.size_x && a.size_y == b.size_y && a.size_z == b.size_z &&
           a.rgba.size() == b.rgba.size();
}
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
}

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
                          bool* out_got_room_sample,
                          bool nearest_sample,
                          float pos_offset_forward_blocks,
                          float pos_offset_right_blocks,
                          float pos_offset_up_blocks)
{
    thread_local VoxelRoomCore::VoxelGrid tl_grid;
    thread_local VoxelRoomCore::VoxelGrid tl_grid_prev;
    thread_local std::uint64_t tl_voxel_key = 0;
    thread_local unsigned long long tl_frame_ms = 0;
    thread_local float tl_frame_interval_ms = 200.0f;
    thread_local bool tl_has_prev_frame = false;

    if(out_got_room_sample)
    {
        *out_got_room_sample = false;
    }

    if(!telemetry.voxel_frame.has_voxel_frame)
    {
        return (RGBColor)0;
    }

    const GameTelemetryBridge::VoxelFrameChannel& vf = telemetry.voxel_frame;
    const std::uint64_t vkey =
        (std::uint64_t)vf.frame_id << 32 | (std::uint32_t)(vf.received_ms & 0xFFFFFFFFu);

    if(vkey != tl_voxel_key || tl_grid.size_x != vf.size_x || tl_grid.size_y != vf.size_y ||
       tl_grid.size_z != vf.size_z)
    {
        if(tl_grid.valid && !tl_grid.rgba.empty() && tl_grid.size_x == vf.size_x && tl_grid.size_y == vf.size_y &&
           tl_grid.size_z == vf.size_z)
        {
            tl_grid_prev = tl_grid;
            tl_has_prev_frame = true;
        }

        if(tl_frame_ms > 0 && vf.received_ms > tl_frame_ms)
        {
            const float dt = (float)(vf.received_ms - tl_frame_ms);
            tl_frame_interval_ms = std::clamp(0.72f * tl_frame_interval_ms + 0.28f * dt, 80.0f, 450.0f);
        }
        tl_frame_ms = vf.received_ms;

        CopyVoxelFrameToGrid(vf, tl_grid);
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

    const float ax = vf.has_anchor_position ? vf.anchor_x
                                            : (telemetry.has_player_pose ? telemetry.player_x : 0.0f);
    const float ay = vf.has_anchor_position ? vf.anchor_y
                                            : (telemetry.has_player_pose ? telemetry.player_y : 0.0f);
    const float az = vf.has_anchor_position ? vf.anchor_z
                                            : (telemetry.has_player_pose ? telemetry.player_z : 0.0f);

    const float scale = std::clamp(room_to_world_scale, 0.005f, 0.80f);
    const float grid_units_per_block = 1.0f / scale;

    VoxelRoomCore::RoomSamplePoint sp{};
    sp.room_x = grid_x;
    sp.room_y = grid_y;
    sp.room_z = grid_z;
    // See SpatialCoordinateSpaces.h — offsets shift the room origin in fixed RoomGrid axes.
    sp.origin_x = origin_x - pos_offset_right_blocks * grid_units_per_block;
    sp.origin_y = origin_y - pos_offset_up_blocks * grid_units_per_block;
    sp.origin_z = origin_z + pos_offset_forward_blocks * grid_units_per_block;

    VoxelRoomCore::MapperSettings ms;
    ms.room_to_world_scale = scale;
    ms.alpha_cutoff = std::max(1e-4f, alpha_cutoff);
    ms.nearest_sample = nearest_sample;

    bool used_cur = false;
    RGBColor c_cur =
        VoxelRoomCore::ComputeRoomMappedVoxelColor(tl_grid, basis, sp, ax, ay, az, ms, &used_cur);
    if(!used_cur)
    {
        return (RGBColor)0;
    }

    float blend_t = 1.0f;
    if(tl_has_prev_frame && GridTopologyMatches(tl_grid, tl_grid_prev) && tl_frame_ms > 0)
    {
        const unsigned long long now_ms = NowMs();
        const float elapsed = (float)((now_ms > tl_frame_ms) ? (now_ms - tl_frame_ms) : 0ULL);
        blend_t = SmoothStep01(elapsed / std::max(40.0f, tl_frame_interval_ms));
    }

    RGBColor out = c_cur;
    if(blend_t < 0.999f)
    {
        bool used_prev = false;
        const RGBColor c_prev =
            VoxelRoomCore::ComputeRoomMappedVoxelColor(tl_grid_prev, basis, sp, ax, ay, az, ms, &used_prev);
        if(used_prev)
        {
            out = LerpRgbColor(c_prev, c_cur, blend_t);
        }
    }

    if(out_got_room_sample)
    {
        *out_got_room_sample = true;
    }
    return out;
}

}
