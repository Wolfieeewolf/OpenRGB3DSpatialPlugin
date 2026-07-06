// SPDX-License-Identifier: GPL-2.0-only

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "RoomSampleFrameShmReader.h"
#include "RoomSampleFrameProtocol.h"
#include "RoomSampleShmPaths.h"
#include "GameTelemetryBridge.h"
#include "LogManager.h"

#include "lz4/lz4.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstddef>
#include <thread>
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

#ifdef _WIN32
static bool ReadFileBytes(const std::wstring& path, std::vector<unsigned char>& out_bytes)
{
    out_bytes.clear();
    HANDLE file = CreateFileW(path.c_str(),
                              GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if(file == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    LARGE_INTEGER file_size{};
    if(!GetFileSizeEx(file, &file_size) ||
       file_size.QuadPart < (LONGLONG)RoomSampleFrameProtocol::kFrameHeaderBytes)
    {
        CloseHandle(file);
        return false;
    }

    const DWORD to_read =
        (DWORD)std::min<std::int64_t>(file_size.QuadPart, (std::int64_t)RoomSampleFrameProtocol::kShmTotalBytes);
    out_bytes.resize(to_read);
    DWORD read_total = 0;
    while(read_total < to_read)
    {
        DWORD chunk = 0;
        if(!ReadFile(file, out_bytes.data() + read_total, to_read - read_total, &chunk, nullptr) || chunk == 0)
        {
            CloseHandle(file);
            out_bytes.clear();
            return false;
        }
        read_total += chunk;
    }
    CloseHandle(file);
    return true;
}

static bool ReadSnapshot(const unsigned char* base,
                         std::size_t view_size,
                         RoomSampleFrameProtocol::FrameHeader& hdr_out,
                         std::vector<unsigned char>& rgba_out)
{
    if(base == nullptr || view_size < RoomSampleFrameProtocol::kFrameHeaderBytes)
    {
        return false;
    }

    RoomSampleFrameProtocol::FrameHeader hdr{};
    std::uint32_t seq1 = 0;
    std::uint32_t seq2 = 0;
    bool hdr_ok = false;

    for(int attempt = 0; attempt < 6; ++attempt)
    {
        seq1 = *reinterpret_cast<const std::uint32_t*>(base + offsetof(RoomSampleFrameProtocol::FrameHeader, sequence));
        if(seq1 & 1u)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        std::memcpy(&hdr, base, sizeof(RoomSampleFrameProtocol::FrameHeader));
        seq2 = *reinterpret_cast<const std::uint32_t*>(base + offsetof(RoomSampleFrameProtocol::FrameHeader, sequence));
        if(seq1 == seq2 && !(seq2 & 1u))
        {
            hdr_ok = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if(!hdr_ok)
    {
        return false;
    }

    hdr_out = hdr;

    if(hdr.magic != RoomSampleFrameProtocol::kFrameMagic || hdr.version != RoomSampleFrameProtocol::kVersion ||
       hdr.header_bytes != RoomSampleFrameProtocol::kFrameHeaderBytes)
    {
        return false;
    }

    std::size_t raw_bytes = 0;
    if(!RoomSampleFrameProtocol::TryComputeRgbaBytes(hdr.size_x, hdr.size_y, hdr.size_z, raw_bytes) ||
       raw_bytes != hdr.rgba_raw_size || raw_bytes == 0)
    {
        return false;
    }

    const std::size_t payload_off = hdr.header_bytes;
    if(hdr.rgba_stored_size == 0 || payload_off + hdr.rgba_stored_size > view_size)
    {
        return false;
    }

    const unsigned char* payload = base + payload_off;
    rgba_out.resize(raw_bytes);

    if((hdr.flags & RoomSampleFrameProtocol::kFlagLz4) != 0)
    {
        const int decoded = LZ4_decompress_safe(reinterpret_cast<const char*>(payload),
                                                reinterpret_cast<char*>(rgba_out.data()),
                                                (int)hdr.rgba_stored_size,
                                                (int)raw_bytes);
        if(decoded != (int)raw_bytes)
        {
            return false;
        }
    }
    else
    {
        if(hdr.rgba_stored_size != raw_bytes)
        {
            return false;
        }
        std::memcpy(rgba_out.data(), payload, raw_bytes);
    }

    return true;
}
#endif
}

RoomSampleFrameShmReader::RoomSampleFrameShmReader() = default;

RoomSampleFrameShmReader::~RoomSampleFrameShmReader()
{
    Stop();
}

void RoomSampleFrameShmReader::Start()
{
#ifdef _WIN32
    if(running_.exchange(true))
    {
        return;
    }
    thread_ = std::thread(&RoomSampleFrameShmReader::PollLoop, this);
#endif
}

void RoomSampleFrameShmReader::Stop()
{
    if(!running_.exchange(false))
    {
        return;
    }
    if(thread_.joinable())
    {
        thread_.join();
    }
}

bool RoomSampleFrameShmReader::TryApplyLatest()
{
#ifdef _WIN32
    static thread_local std::uint32_t last_applied_frame_id = 0;
    static thread_local int last_size_x = -1;
    static thread_local int last_size_y = -1;
    static thread_local int last_size_z = -1;
    static thread_local unsigned long long last_fail_log_ms = 0;
    static thread_local unsigned long long last_missing_log_ms = 0;

    const std::wstring path = RoomSampleShmPaths::FrameFilePathW();
    std::vector<unsigned char> file_bytes;
    if(!ReadFileBytes(path, file_bytes))
    {
        const unsigned long long now = NowMs();
        if(now - last_missing_log_ms > 15000ULL)
        {
            last_missing_log_ms = now;
            LOG_INFO("[3DSpatial] waiting for room sample SHM file from Minecraft mod");
        }
        return false;
    }

    RoomSampleFrameProtocol::FrameHeader hdr{};
    std::vector<unsigned char> rgba;
    if(!ReadSnapshot(file_bytes.data(), file_bytes.size(), hdr, rgba))
    {
        const unsigned long long now = NowMs();
        if(now - last_fail_log_ms > 8000ULL)
        {
            last_fail_log_ms = now;
            LOG_WARNING("[3DSpatial] room sample SHM parse failed");
        }
        return false;
    }

    if(hdr.frame_id == last_applied_frame_id)
    {
        return false;
    }

    const bool first = last_size_x < 0;
    const bool grid_changed = hdr.size_x != last_size_x || hdr.size_y != last_size_y || hdr.size_z != last_size_z;
    if(first)
    {
        LOG_INFO("[3DSpatial] room sample SHM frame received (id %u, %dx%dx%d)",
                 hdr.frame_id,
                 hdr.size_x,
                 hdr.size_y,
                 hdr.size_z);
    }
    else if(grid_changed)
    {
        LOG_INFO("[3DSpatial] room sample SHM grid updated: %dx%dx%d -> %dx%dx%d (frame id %u)",
                 last_size_x,
                 last_size_y,
                 last_size_z,
                 hdr.size_x,
                 hdr.size_y,
                 hdr.size_z,
                 hdr.frame_id);
    }

    last_size_x = hdr.size_x;
    last_size_y = hdr.size_y;
    last_size_z = hdr.size_z;
    last_applied_frame_id = hdr.frame_id;
    GameTelemetryBridge::ApplyRoomSampleShmFrame(hdr, std::move(rgba));
    GameTelemetryBridge::NotifyTelemetryDataUpdated();
    return true;
#else
    return false;
#endif
}

void RoomSampleFrameShmReader::PollLoop()
{
#ifdef _WIN32
    while(running_.load())
    {
        TryApplyLatest();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
#endif
}
