// SPDX-License-Identifier: GPL-2.0-only

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "GpuPanoramaFrameShmReader.h"
#include "GpuPanoramaFrameProtocol.h"
#include "GpuPanoramaShmPaths.h"
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
       file_size.QuadPart < (LONGLONG)GpuPanoramaFrameProtocol::kHeaderBytes)
    {
        CloseHandle(file);
        return false;
    }

    const DWORD to_read = (DWORD)std::min<std::int64_t>(file_size.QuadPart,
                                                        (std::int64_t)GpuPanoramaFrameProtocol::kShmTotalBytes);
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

static bool ReadFileHeader(const std::wstring& path, GpuPanoramaFrameProtocol::FrameHeader& hdr_out)
{
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

    DWORD read_total = 0;
    GpuPanoramaFrameProtocol::FrameHeader hdr{};
    const BOOL ok = ReadFile(file, &hdr, (DWORD)sizeof(hdr), &read_total, nullptr);
    CloseHandle(file);
    if(!ok || read_total != sizeof(hdr))
    {
        return false;
    }

    hdr_out = hdr;
    return hdr.magic == GpuPanoramaFrameProtocol::kMagic && hdr.version == GpuPanoramaFrameProtocol::kVersion &&
           hdr.header_bytes == GpuPanoramaFrameProtocol::kHeaderBytes;
}

static bool ReadSnapshot(const unsigned char* base,
                         std::size_t view_size,
                         GpuPanoramaFrameProtocol::FrameHeader& hdr_out,
                         std::vector<unsigned char>& rgba_out)
{
    if(base == nullptr || view_size < GpuPanoramaFrameProtocol::kHeaderBytes)
    {
        return false;
    }

    GpuPanoramaFrameProtocol::FrameHeader hdr{};
    std::uint32_t seq1 = 0;
    std::uint32_t seq2 = 0;
    bool hdr_ok = false;

    for(int attempt = 0; attempt < 6; ++attempt)
    {
        seq1 = *reinterpret_cast<const std::uint32_t*>(base + offsetof(GpuPanoramaFrameProtocol::FrameHeader, sequence));
        if(seq1 & 1u)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        std::memcpy(&hdr, base, sizeof(GpuPanoramaFrameProtocol::FrameHeader));
        seq2 = *reinterpret_cast<const std::uint32_t*>(base + offsetof(GpuPanoramaFrameProtocol::FrameHeader, sequence));
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

    if(hdr.magic != GpuPanoramaFrameProtocol::kMagic || hdr.version != GpuPanoramaFrameProtocol::kVersion ||
       hdr.header_bytes != GpuPanoramaFrameProtocol::kHeaderBytes)
    {
        return false;
    }

    std::size_t raw_bytes = 0;
    if(!GpuPanoramaFrameProtocol::TryComputeRgbaBytes(hdr.face_w, hdr.face_h, hdr.face_count, raw_bytes) ||
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

    if((hdr.flags & GpuPanoramaFrameProtocol::kFlagLz4) != 0)
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

GpuPanoramaFrameShmReader::GpuPanoramaFrameShmReader() = default;

GpuPanoramaFrameShmReader::~GpuPanoramaFrameShmReader()
{
    Stop();
}

void GpuPanoramaFrameShmReader::Start()
{
#ifdef _WIN32
    if(running_.exchange(true))
    {
        return;
    }
    thread_ = std::thread(&GpuPanoramaFrameShmReader::PollLoop, this);
#endif
}

void GpuPanoramaFrameShmReader::Stop()
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

bool GpuPanoramaFrameShmReader::TryApplyLatest()
{
#ifdef _WIN32
    static thread_local std::uint32_t last_applied_frame_id = 0;
    static thread_local bool logged_success = false;
    static thread_local ULONGLONG last_file_time = 0;

    const std::wstring path = GpuPanoramaShmPaths::FrameFilePathW();

    WIN32_FILE_ATTRIBUTE_DATA attrs{};
    if(!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attrs))
    {
        return false;
    }
    const ULONGLONG file_time =
        ((ULONGLONG)attrs.ftLastWriteTime.dwHighDateTime << 32) | attrs.ftLastWriteTime.dwLowDateTime;

    GpuPanoramaFrameProtocol::FrameHeader header_peek{};
    if(!ReadFileHeader(path, header_peek))
    {
        return false;
    }

    if(file_time == last_file_time && header_peek.frame_id == last_applied_frame_id)
    {
        return false;
    }

    std::vector<unsigned char> file_bytes;
    if(!ReadFileBytes(path, file_bytes))
    {
        return false;
    }

    GpuPanoramaFrameProtocol::FrameHeader hdr{};
    std::vector<unsigned char> rgba;
    if(!ReadSnapshot(file_bytes.data(), file_bytes.size(), hdr, rgba))
    {
        return false;
    }

    if(hdr.frame_id == last_applied_frame_id)
    {
        last_file_time = file_time;
        return false;
    }

    if(!logged_success)
    {
        logged_success = true;
        LOG_INFO("[3DSpatial] GPU panorama SHM received (id %u, %u faces %ux%u)",
                 hdr.frame_id,
                 hdr.face_count,
                 hdr.face_w,
                 hdr.face_h);
    }

    last_applied_frame_id = hdr.frame_id;
    last_file_time = file_time;
    GameTelemetryBridge::ApplyGpuPanoramaShmFrame(hdr, std::move(rgba));
    GameTelemetryBridge::NotifyTelemetryDataUpdated();
    return true;
#else
    return false;
#endif
}

void GpuPanoramaFrameShmReader::PollLoop()
{
#ifdef _WIN32
    while(running_.load())
    {
        TryApplyLatest();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
#endif
}
