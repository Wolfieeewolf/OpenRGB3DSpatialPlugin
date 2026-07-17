// SPDX-License-Identifier: GPL-2.0-only

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "RoomSampleConfigPublisher.h"
#include "RoomSampleFrameProtocol.h"
#include "RoomSampleShmPaths.h"
#include "Effects3D/Games/Minecraft/MinecraftGameSettings.h"
#include "SpatialEffect3D.h"
#include "PluginLog.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <cstddef>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
static unsigned long long NowMs()
{
    return (unsigned long long)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

#ifdef _WIN32
static bool WriteConfigFile(const RoomSampleFrameProtocol::ConfigHeader& hdr)
{
    const std::wstring path = RoomSampleShmPaths::ConfigFilePathW();
    const std::wstring dir = RoomSampleShmPaths::BaseDirW();
    CreateDirectoryW(dir.c_str(), nullptr);

    HANDLE file = CreateFileW(path.c_str(),
                              GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr,
                              CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if(file == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD written = 0;
    const BOOL ok = WriteFile(file, &hdr, (DWORD)sizeof(hdr), &written, nullptr);
    CloseHandle(file);
    return ok && written == sizeof(hdr);
}
#endif

static std::uint32_t HashConfig(const RoomSampleFrameProtocol::ConfigHeader& h)
{
    std::uint32_t hash = 2166136261u;
    const auto* bytes = reinterpret_cast<const unsigned char*>(&h);
    for(std::size_t i = offsetof(RoomSampleFrameProtocol::ConfigHeader, config_id);
        i < sizeof(RoomSampleFrameProtocol::ConfigHeader);
        ++i)
    {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}
}

namespace RoomSampleConfigPublisher
{
static RoomSampleFrameProtocol::ConfigHeader g_last_config{};
static bool g_has_last_config = false;

void PublishIfNeeded(const GridContext3D& grid,
                     const MinecraftGame::Settings& settings,
                     float effect_origin_x,
                     float effect_origin_y,
                     float effect_origin_z,
                     float room_to_world_scale)
{
#ifndef _WIN32
    (void)grid;
    (void)settings;
    (void)effect_origin_x;
    (void)effect_origin_y;
    (void)effect_origin_z;
    (void)room_to_world_scale;
    return;
#else
    static std::uint32_t last_hash = 0;
    static unsigned long long last_write_ms = 0;
    const unsigned long long now = NowMs();

    const float span_x = std::max(1e-3f, grid.max_x - grid.min_x);
    const float span_y = std::max(1e-3f, grid.max_y - grid.min_y);
    const float span_z = std::max(1e-3f, grid.max_z - grid.min_z);

    int nx = 0;
    int ny = 0;
    int nz = 0;

    // Maximum cells the user permits (ceiling, not a target).
    const std::size_t max_cells = (std::size_t)std::clamp(
        settings.room_vr_sample_target_cells, 4096, (int)RoomSampleFrameProtocol::kMaxCells);

    // Primary: derive grid so each cell maps to exactly one Minecraft block at the current scale.
    // This gives the minimum cell count for full per-block accuracy and keeps updates fast.
    const float clamped_scale = std::clamp(room_to_world_scale, 0.005f, 0.80f);
    const int nx_mc = std::clamp((int)std::ceil(span_x * clamped_scale) + 1, 2, 512);
    const int ny_mc = std::clamp((int)std::ceil(span_y * clamped_scale) + 1, 2, 384);
    const int nz_mc = std::clamp((int)std::ceil(span_z * clamped_scale) + 1, 2, 512);
    const std::size_t mc_cells = (std::size_t)nx_mc * (std::size_t)ny_mc * (std::size_t)nz_mc;

    if(mc_cells <= max_cells)
    {
        // Block-matched grid fits within the budget: use it directly.
        nx = nx_mc;
        ny = ny_mc;
        nz = nz_mc;
    }
    else
    {
        // Grid exceeds the max-cells ceiling: fall back to proportional distribution within budget.
        RoomSampleFrameProtocol::ComputeGridDimensions(span_x, span_y, span_z, nx, ny, nz, max_cells);
    }

    RoomSampleFrameProtocol::ConfigHeader hdr{};
    hdr.magic = RoomSampleFrameProtocol::kConfigMagic;
    hdr.version = RoomSampleFrameProtocol::kVersion;
    hdr.header_bytes = RoomSampleFrameProtocol::kConfigHeaderBytes;
    hdr.sequence = 2;
    hdr.config_id = 0;
    hdr.flags = RoomSampleFrameProtocol::kFlagEnabled;
    hdr.target_cells = (std::uint32_t)std::min(mc_cells, max_cells);
    hdr.size_x = nx;
    hdr.size_y = ny;
    hdr.size_z = nz;
    hdr.room_min_x = grid.min_x;
    hdr.room_min_y = grid.min_y;
    hdr.room_min_z = grid.min_z;
    hdr.room_max_x = grid.max_x;
    hdr.room_max_y = grid.max_y;
    hdr.room_max_z = grid.max_z;
    hdr.effect_origin_x = effect_origin_x;
    hdr.effect_origin_y = effect_origin_y;
    hdr.effect_origin_z = effect_origin_z;
    hdr.room_to_world_scale = std::clamp(room_to_world_scale, 0.005f, 0.80f);
    hdr.heading_offset_deg = settings.room_vr_heading_offset_deg;
    hdr.pos_offset_forward_blocks = settings.room_vr_pos_offset_forward_blocks;
    hdr.pos_offset_right_blocks = settings.room_vr_pos_offset_right_blocks;
    hdr.pos_offset_up_blocks = settings.room_vr_pos_offset_up_blocks;

    const std::uint32_t hash = HashConfig(hdr);
    if(hash == last_hash && now - last_write_ms < 1000ULL)
    {
        return;
    }

    const bool config_changed = hash != last_hash;

    hdr.config_id = hash;
    hdr.sequence = 1;
    if(!WriteConfigFile(hdr))
    {
        return;
    }
    hdr.sequence = 2;
    WriteConfigFile(hdr);

    g_last_config = hdr;
    g_has_last_config = true;

    if(config_changed)
    {
        LOG_INFO("[3DSpatial] room config published (%dx%dx%d, config id %u, scale %.4f)",
                 nx,
                 ny,
                 nz,
                 hdr.config_id,
                 hdr.room_to_world_scale);
    }

    last_hash = hash;
    last_write_ms = now;
#endif
}

bool GetLastPublishedConfig(RoomSampleFrameProtocol::ConfigHeader& out)
{
    if(!g_has_last_config)
    {
        return false;
    }
    out = g_last_config;
    return (out.flags & RoomSampleFrameProtocol::kFlagEnabled) != 0;
}

void Disable()
{
#ifdef _WIN32
    RoomSampleFrameProtocol::ConfigHeader hdr{};
    hdr.magic = RoomSampleFrameProtocol::kConfigMagic;
    hdr.version = RoomSampleFrameProtocol::kVersion;
    hdr.header_bytes = RoomSampleFrameProtocol::kConfigHeaderBytes;
    hdr.config_id = HashConfig(hdr);
    hdr.sequence = 1;
    hdr.flags = 0;
    WriteConfigFile(hdr);
    g_has_last_config = false;
#endif
}

}
