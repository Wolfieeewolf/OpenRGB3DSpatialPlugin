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

/** Cubemap texels covering each mapped LED direction (+ bilinear 2×2). */
static void BuildImportantCubemapTexels(const RoomSampleFrameProtocol::ConfigHeader& hdr,
                                        const std::vector<float>& led_xyz,
                                        std::vector<std::uint32_t>& out)
{
    out.clear();
    if(led_xyz.size() < 3
       || !RoomSampleFrameProtocol::IsCubemapLayout(hdr.size_x, hdr.size_y, hdr.size_z, hdr.flags))
    {
        return;
    }

    const int face_size = hdr.size_x;
    const float scale = std::clamp(hdr.room_to_world_scale, 0.005f, 0.80f);
    const float eff_ox = hdr.effect_origin_x;
    const float eff_oy = hdr.effect_origin_y;
    const float eff_oz = hdr.effect_origin_z;

    std::unordered_set<std::uint32_t> unique;
    unique.reserve(std::min<std::size_t>(led_xyz.size() / 3u * 4u, RoomSampleFrameProtocol::kMaxImportantCells));

    auto mark = [&](int face, int iu, int iv) {
        if(face < 0 || face >= RoomSampleFrameProtocol::kCubemapFaceCount || iu < 0 || iv < 0
           || iu >= face_size || iv >= face_size)
        {
            return;
        }
        const std::uint32_t flat =
            (std::uint32_t)(((std::size_t)iu * (std::size_t)face_size + (std::size_t)iv)
                                * (std::size_t)RoomSampleFrameProtocol::kCubemapFaceCount
                            + (std::size_t)face);
        unique.insert(flat);
    };

    for(std::size_t i = 0; i + 2 < led_xyz.size(); i += 3)
    {
        float dx = (led_xyz[i] - eff_ox) * scale;
        float dy = (led_xyz[i + 1] - eff_oy) * scale;
        float dz = (eff_oz - led_xyz[i + 2]) * scale;
        const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
        if(len <= 1e-5f)
        {
            continue;
        }
        dx /= len;
        dy /= len;
        dz /= len;

        int face = 0;
        float u = 0.0f;
        float v = 0.0f;
        RoomSampleFrameProtocol::DirectionToCubemapUv(dx, dy, dz, face, u, v);

        const float gu = u * (float)face_size - 0.5f;
        const float gv = v * (float)face_size - 0.5f;
        // One nearest texel per LED — bilinear neighbourhood caused high-res flashing.
        const int iu = std::clamp((int)std::lround(gu), 0, face_size - 1);
        const int iv = std::clamp((int)std::lround(gv), 0, face_size - 1);
        mark(face, iu, iv);

        if(unique.size() >= RoomSampleFrameProtocol::kMaxImportantCells)
        {
            break;
        }
    }

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

    const int face = std::clamp(settings.room_ambilight_cubemap_face_size,
                                32,
                                RoomSampleFrameProtocol::kCubemapFaceSizeMax);
    const int nx = face;
    const int ny = face;
    const int nz = RoomSampleFrameProtocol::kCubemapFaceCount;

    RoomSampleFrameProtocol::ConfigHeader hdr{};
    hdr.magic = RoomSampleFrameProtocol::kConfigMagic;
    hdr.version = RoomSampleFrameProtocol::kVersion;
    hdr.header_bytes = RoomSampleFrameProtocol::kConfigHeaderBytes;
    hdr.sequence = 2;
    hdr.config_id = 0;
    hdr.flags = RoomSampleFrameProtocol::kFlagEnabled | RoomSampleFrameProtocol::kFlagCubemap;
    if(settings.room_ambilight_sky_enabled)
    {
        hdr.flags |= RoomSampleFrameProtocol::kFlagSkyEnabled;
    }
    hdr.target_cells = (std::uint32_t)((std::size_t)nx * (std::size_t)ny * (std::size_t)nz);
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
    // Alignment floats kept in the SHM header for layout stability; always identity.
    hdr.heading_offset_deg = 0.0f;
    hdr.pos_offset_forward_blocks = 0.0f;
    hdr.pos_offset_right_blocks = 0.0f;
    hdr.pos_offset_up_blocks = 0.0f;

    const std::uint32_t uv_dim =
        (std::uint32_t)RoomSampleFrameProtocol::SnapUvTextureDim(settings.room_ambilight_texture_uv_dim);
    std::memcpy(hdr.reserved + RoomSampleFrameProtocol::kReservedUvMaxDimOffset, &uv_dim, sizeof(uv_dim));

    std::vector<std::uint32_t> important_cells;
    BuildImportantCubemapTexels(hdr, led_xyz, important_cells);
    if(!important_cells.empty())
    {
        hdr.flags |= RoomSampleFrameProtocol::kFlagImportantCells;
        const std::uint32_t count = (std::uint32_t)important_cells.size();
        std::memcpy(hdr.reserved + RoomSampleFrameProtocol::kReservedImportantCountOffset,
                    &count,
                    sizeof(count));
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
        LOG_INFO("[3DSpatial] room config published (cubemap %dx%d x6, UV %u, config id %u, scale %.4f, "
                 "room spans %.1f/%.1f/%.1f, mapped LED texels %zu)",
                 nx,
                 ny,
                 uv_dim,
                 hdr.config_id,
                 hdr.room_to_world_scale,
                 span_x,
                 span_y,
                 span_z,
                 g_last_important_cells.size());
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
