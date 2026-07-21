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
#include <mutex>
#include <unordered_set>
#include <vector>

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

static std::mutex g_mu;
static bool g_has_publish_room_grid = false;
static GridContext3D g_publish_room_grid(0, 1, 0, 1, 0, 1, 10.0f);
static std::vector<float> g_frame_led_xyz;
static std::vector<std::uint32_t> g_last_important_cells;

static void BuildImportantCells(const RoomSampleFrameProtocol::ConfigHeader& hdr,
                                const std::vector<float>& led_xyz,
                                std::vector<std::uint32_t>& out)
{
    out.clear();
    if(led_xyz.size() < 3 || hdr.size_x <= 0 || hdr.size_y <= 0 || hdr.size_z <= 0)
    {
        return;
    }

    const float span_x = std::max(1e-6f, hdr.room_max_x - hdr.room_min_x);
    const float span_y = std::max(1e-6f, hdr.room_max_y - hdr.room_min_y);
    const float span_z = std::max(1e-6f, hdr.room_max_z - hdr.room_min_z);
    const int nx = hdr.size_x;
    const int ny = hdr.size_y;
    const int nz = hdr.size_z;

    std::unordered_set<std::uint32_t> unique;
    unique.reserve(std::min<std::size_t>(led_xyz.size() / 3u * 8u, RoomSampleFrameProtocol::kMaxImportantCells));

    auto mark = [&](int ix, int iy, int iz) {
        if(ix < 0 || iy < 0 || iz < 0 || ix >= nx || iy >= ny || iz >= nz)
        {
            return;
        }
        const std::uint32_t flat =
            (std::uint32_t)(((std::size_t)ix * (std::size_t)ny + (std::size_t)iy) * (std::size_t)nz + (std::size_t)iz);
        unique.insert(flat);
    };

    // Same continuous mapping as RoomSampleMapping::TrilinearSample.
    for(std::size_t i = 0; i + 2 < led_xyz.size(); i += 3)
    {
        const float gx =
            std::clamp((led_xyz[i] - hdr.room_min_x) / span_x, 0.0f, 1.0f) * (float)nx - 0.5f;
        const float gy =
            std::clamp((led_xyz[i + 1] - hdr.room_min_y) / span_y, 0.0f, 1.0f) * (float)ny - 0.5f;
        const float gz =
            std::clamp((led_xyz[i + 2] - hdr.room_min_z) / span_z, 0.0f, 1.0f) * (float)nz - 0.5f;

        const int ix0 = std::clamp((int)std::floor(gx), 0, nx - 1);
        const int iy0 = std::clamp((int)std::floor(gy), 0, ny - 1);
        const int iz0 = std::clamp((int)std::floor(gz), 0, nz - 1);
        const int ix1 = std::min(ix0 + 1, nx - 1);
        const int iy1 = std::min(iy0 + 1, ny - 1);
        const int iz1 = std::min(iz0 + 1, nz - 1);

        // Only the exact trilinear corner set — extra halo was filling empty room with sky
        // that then bled onto neighbouring LEDs.
        mark(ix0, iy0, iz0);
        mark(ix1, iy0, iz0);
        mark(ix0, iy1, iz0);
        mark(ix1, iy1, iz0);
        mark(ix0, iy0, iz1);
        mark(ix1, iy0, iz1);
        mark(ix0, iy1, iz1);
        mark(ix1, iy1, iz1);
        if(unique.size() >= RoomSampleFrameProtocol::kMaxImportantCells)
        {
            goto done_mark;
        }
    }
done_mark:
    out.assign(unique.begin(), unique.end());
    std::sort(out.begin(), out.end());
}

#ifdef _WIN32
static bool WriteConfigFile(const RoomSampleFrameProtocol::ConfigHeader& hdr,
                            const std::vector<std::uint32_t>& important_cells)
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
    BOOL ok = WriteFile(file, &hdr, (DWORD)sizeof(hdr), &written, nullptr);
    if(!ok || written != sizeof(hdr))
    {
        CloseHandle(file);
        return false;
    }

    if(!important_cells.empty())
    {
        const DWORD payload_bytes = (DWORD)(important_cells.size() * sizeof(std::uint32_t));
        written = 0;
        ok = WriteFile(file, important_cells.data(), payload_bytes, &written, nullptr);
        if(!ok || written != payload_bytes)
        {
            CloseHandle(file);
            return false;
        }
    }

    CloseHandle(file);
    return true;
}
#endif

static std::uint32_t HashConfig(const RoomSampleFrameProtocol::ConfigHeader& h,
                                const std::vector<std::uint32_t>& important_cells)
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
    for(std::uint32_t cell : important_cells)
    {
        hash ^= cell & 0xFFu;
        hash *= 16777619u;
        hash ^= (cell >> 8) & 0xFFu;
        hash *= 16777619u;
        hash ^= (cell >> 16) & 0xFFu;
        hash *= 16777619u;
        hash ^= (cell >> 24) & 0xFFu;
        hash *= 16777619u;
    }
    return hash;
}
}

namespace RoomSampleConfigPublisher
{
static RoomSampleFrameProtocol::ConfigHeader g_last_config{};
static bool g_has_last_config = false;

void SetPublishRoomGrid(const GridContext3D& room_grid)
{
    std::lock_guard<std::mutex> lock(g_mu);
    g_publish_room_grid = room_grid;
    g_has_publish_room_grid = true;
}

void SetFrameLedRoomPositions(const float* xyz_triplets, std::size_t triplet_count)
{
    std::lock_guard<std::mutex> lock(g_mu);
    g_frame_led_xyz.clear();
    if(xyz_triplets == nullptr || triplet_count == 0)
    {
        return;
    }
    const std::size_t max_triplets = RoomSampleFrameProtocol::kMaxImportantCells;
    const std::size_t n = std::min(triplet_count, max_triplets);
    g_frame_led_xyz.assign(xyz_triplets, xyz_triplets + n * 3u);
}

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

    GridContext3D publish_grid = grid;
    std::vector<float> led_xyz;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        if(g_has_publish_room_grid)
        {
            // Always size/map against the global room grid, never a zone-local effect grid.
            publish_grid = g_publish_room_grid;
        }
        led_xyz = g_frame_led_xyz;
    }

    const float span_x = std::max(1e-3f, publish_grid.max_x - publish_grid.min_x);
    const float span_y = std::max(1e-3f, publish_grid.max_y - publish_grid.min_y);
    const float span_z = std::max(1e-3f, publish_grid.max_z - publish_grid.min_z);

    int nx = 0;
    int ny = 0;
    int nz = 0;

    const std::size_t max_cells = (std::size_t)std::clamp(
        settings.room_vr_sample_target_cells, 4096, (int)RoomSampleFrameProtocol::kMaxCells);

    const float clamped_scale = std::clamp(room_to_world_scale, 0.005f, 0.80f);
    const int nx_mc = std::clamp((int)std::ceil(span_x * clamped_scale) + 1, 2, 512);
    const int ny_mc = std::clamp((int)std::ceil(span_y * clamped_scale) + 1, 2, 384);
    const int nz_mc = std::clamp((int)std::ceil(span_z * clamped_scale) + 1, 2, 512);
    const std::size_t mc_cells = (std::size_t)nx_mc * (std::size_t)ny_mc * (std::size_t)nz_mc;

    if(mc_cells <= max_cells)
    {
        nx = nx_mc;
        ny = ny_mc;
        nz = nz_mc;
    }
    else
    {
        RoomSampleFrameProtocol::ComputeGridDimensions(span_x, span_y, span_z, nx, ny, nz, max_cells);
    }

    RoomSampleFrameProtocol::ConfigHeader hdr{};
    hdr.magic = RoomSampleFrameProtocol::kConfigMagic;
    hdr.version = RoomSampleFrameProtocol::kVersion;
    hdr.header_bytes = RoomSampleFrameProtocol::kConfigHeaderBytes;
    hdr.sequence = 2;
    hdr.config_id = 0;
    hdr.flags = RoomSampleFrameProtocol::kFlagEnabled;
    if(settings.room_vr_sky_enabled)
    {
        hdr.flags |= RoomSampleFrameProtocol::kFlagSkyEnabled;
    }
    hdr.target_cells = (std::uint32_t)std::min(mc_cells, max_cells);
    hdr.size_x = nx;
    hdr.size_y = ny;
    hdr.size_z = nz;
    hdr.room_min_x = publish_grid.min_x;
    hdr.room_min_y = publish_grid.min_y;
    hdr.room_min_z = publish_grid.min_z;
    hdr.room_max_x = publish_grid.max_x;
    hdr.room_max_y = publish_grid.max_y;
    hdr.room_max_z = publish_grid.max_z;
    hdr.effect_origin_x = effect_origin_x;
    hdr.effect_origin_y = effect_origin_y;
    hdr.effect_origin_z = effect_origin_z;
    hdr.room_to_world_scale = std::clamp(room_to_world_scale, 0.005f, 0.80f);
    hdr.heading_offset_deg = settings.room_vr_heading_offset_deg;
    hdr.pos_offset_forward_blocks = settings.room_vr_pos_offset_forward_blocks;
    hdr.pos_offset_right_blocks = settings.room_vr_pos_offset_right_blocks;
    hdr.pos_offset_up_blocks = settings.room_vr_pos_offset_up_blocks;

    std::vector<std::uint32_t> important_cells;
    BuildImportantCells(hdr, led_xyz, important_cells);
    if(!important_cells.empty())
    {
        hdr.flags |= RoomSampleFrameProtocol::kFlagImportantCells;
        const std::uint32_t count = (std::uint32_t)important_cells.size();
        std::memcpy(hdr.reserved, &count, sizeof(count));
    }

    const std::uint32_t hash = HashConfig(hdr, important_cells);
    const unsigned long long refresh_ms = important_cells.empty() ? 1000ULL : 250ULL;
    if(hash == last_hash && now - last_write_ms < refresh_ms)
    {
        return;
    }

    const bool config_changed = hash != last_hash;

    hdr.config_id = hash;
    hdr.sequence = 1;
    if(!WriteConfigFile(hdr, important_cells))
    {
        return;
    }
    hdr.sequence = 2;
    WriteConfigFile(hdr, important_cells);

    g_last_config = hdr;
    g_has_last_config = true;
    g_last_important_cells = std::move(important_cells);

    if(config_changed)
    {
        LOG_INFO("[3DSpatial] room config published (%dx%dx%d, config id %u, scale %.4f, important cells %zu, leds %zu)",
                 nx,
                 ny,
                 nz,
                 hdr.config_id,
                 hdr.room_to_world_scale,
                 g_last_important_cells.size(),
                 led_xyz.size() / 3u);
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

const std::vector<std::uint32_t>& GetLastImportantCells()
{
    return g_last_important_cells;
}

void Disable()
{
#ifdef _WIN32
    RoomSampleFrameProtocol::ConfigHeader hdr{};
    hdr.magic = RoomSampleFrameProtocol::kConfigMagic;
    hdr.version = RoomSampleFrameProtocol::kVersion;
    hdr.header_bytes = RoomSampleFrameProtocol::kConfigHeaderBytes;
    hdr.config_id = HashConfig(hdr, {});
    hdr.sequence = 1;
    hdr.flags = 0;
    WriteConfigFile(hdr, {});
    g_has_last_config = false;
    g_last_important_cells.clear();
#endif
}

}
