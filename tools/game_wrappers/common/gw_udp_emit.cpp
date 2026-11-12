// SPDX-License-Identifier: GPL-2.0-only

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "gw_udp_emit.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace
{
constexpr char kMagic[4] = {'G', 'W', '0', '1'};
constexpr std::uint16_t kPort = 9877;
constexpr std::uint8_t kDeviceCount = 6;
constexpr std::uint8_t kFrameDeviceGrid = 2;
constexpr std::uint8_t kMaxRows = 16;
constexpr std::uint8_t kMaxCols = 32;
constexpr std::uint16_t kMaxCells = static_cast<std::uint16_t>(kMaxRows) * kMaxCols;
constexpr std::uint8_t kGridFlagActive = 1u;
constexpr unsigned long long kFlushIntervalMs = 15;

#pragma pack(push, 1)
struct DeviceGridHeader
{
    char magic[4];
    std::uint32_t sequence;
    std::uint64_t timestamp_ms;
    std::uint8_t frame_type;
    char source[24];
    std::uint8_t device_id;
    std::uint8_t rows;
    std::uint8_t cols;
    std::uint8_t flags;
};
#pragma pack(pop)

static_assert(sizeof(DeviceGridHeader) == 45, "grid header size");

struct DeviceGridState
{
    bool dirty = false;
    bool active = false;
    std::uint8_t rows = 0;
    std::uint8_t cols = 0;
    std::uint8_t rgb[kMaxCells * 3]{};
};

std::atomic<std::uint32_t> g_sequence{0};
std::mutex g_mutex;
int g_sock = -1;
bool g_wsa_started = false;

DeviceGridState g_grids[kDeviceCount]{};
char g_source[24]{};
unsigned long long g_last_flush_ms = 0;
bool g_dirty = false;

static unsigned long long NowMs()
{
    return (unsigned long long)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

static bool EnsureSocketUnlocked()
{
    if(g_sock >= 0)
    {
        return true;
    }
#ifdef _WIN32
    if(!g_wsa_started)
    {
        WSADATA wsa{};
        if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        {
            return false;
        }
        g_wsa_started = true;
    }
#endif
    g_sock = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    return g_sock >= 0;
}

static void ColorRefToRgb8(std::uint32_t color, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b)
{
    r = (std::uint8_t)(color & 0xFF);
    g = (std::uint8_t)((color >> 8) & 0xFF);
    b = (std::uint8_t)((color >> 16) & 0xFF);
}

static bool SendBytes(const char* data, int size)
{
    if(!EnsureSocketUnlocked())
    {
        return false;
    }
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(kPort);
    dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const int sent = sendto(g_sock, data, size, 0, reinterpret_cast<sockaddr*>(&dest), (socklen_t)sizeof(dest));
    return sent == size;
}

static bool SendDeviceGridUnlocked(unsigned idx, const DeviceGridState& st)
{
    if(!st.active || st.rows == 0 || st.cols == 0)
    {
        return true;
    }

    const unsigned cells = (unsigned)st.rows * (unsigned)st.cols;
    if(cells == 0 || cells > kMaxCells)
    {
        return false;
    }

    std::vector<char> packet(sizeof(DeviceGridHeader) + cells * 3u);
    DeviceGridHeader* hdr = reinterpret_cast<DeviceGridHeader*>(packet.data());
    std::memset(hdr, 0, sizeof(*hdr));
    std::memcpy(hdr->magic, kMagic, 4);
    hdr->sequence = ++g_sequence;
    hdr->timestamp_ms = NowMs();
    hdr->frame_type = kFrameDeviceGrid;
    std::memcpy(hdr->source, g_source, sizeof(hdr->source));
    hdr->device_id = (std::uint8_t)idx;
    hdr->rows = st.rows;
    hdr->cols = st.cols;
    hdr->flags = kGridFlagActive;
    std::memcpy(packet.data() + sizeof(DeviceGridHeader), st.rgb, cells * 3u);
    return SendBytes(packet.data(), (int)packet.size());
}

static void FlushDevicesUnlocked()
{
    if(!g_dirty)
    {
        return;
    }

    bool ok = true;
    for(unsigned i = 0; i < kDeviceCount; ++i)
    {
        if(!g_grids[i].dirty)
        {
            continue;
        }
        if(!SendDeviceGridUnlocked(i, g_grids[i]))
        {
            ok = false;
            continue;
        }
        g_grids[i].dirty = false;
    }

    if(ok)
    {
        g_last_flush_ms = NowMs();
        g_dirty = false;
    }
}

static void MaybeAutoFlushUnlocked()
{
    if(!g_dirty)
    {
        return;
    }
    const unsigned long long now = NowMs();
    if(g_last_flush_ms != 0 && (now - g_last_flush_ms) < kFlushIntervalMs)
    {
        return;
    }
    FlushDevicesUnlocked();
}

static void StoreGridUnlocked(unsigned idx,
                              const std::uint32_t* colors,
                              int rows,
                              int cols,
                              const char* source)
{
    if(idx >= kDeviceCount || !colors || rows <= 0 || cols <= 0)
    {
        return;
    }

    const int use_rows = std::min(rows, (int)kMaxRows);
    const int use_cols = std::min(cols, (int)kMaxCols);

    if(source)
    {
        std::memset(g_source, 0, sizeof(g_source));
        std::strncpy(g_source, source, sizeof(g_source) - 1);
    }

    DeviceGridState& st = g_grids[idx];
    st.active = true;
    st.rows = (std::uint8_t)use_rows;
    st.cols = (std::uint8_t)use_cols;
    std::memset(st.rgb, 0, sizeof(st.rgb));

    for(int row = 0; row < use_rows; ++row)
    {
        for(int col = 0; col < use_cols; ++col)
        {
            const std::uint32_t c = colors[row * cols + col];
            std::uint8_t r, g, b;
            ColorRefToRgb8(c, r, g, b);
            const unsigned off = ((unsigned)row * (unsigned)use_cols + (unsigned)col) * 3u;
            st.rgb[off + 0] = r;
            st.rgb[off + 1] = g;
            st.rgb[off + 2] = b;
        }
    }

    st.dirty = true;
    g_dirty = true;
    MaybeAutoFlushUnlocked();
}

} // namespace

namespace GwUdp
{

void SetDeviceColorRef(DeviceId id, std::uint32_t color, const char* source)
{
    const std::uint32_t one = color;
    std::lock_guard<std::mutex> lock(g_mutex);
    StoreGridUnlocked(static_cast<unsigned>(id), &one, 1, 1, source);
}

void SetDeviceGridColorRef(DeviceId id,
                           const std::uint32_t* colors,
                           int rows,
                           int cols,
                           const char* source)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if(!colors || rows <= 0 || cols <= 0)
    {
        const std::uint32_t zero = 0;
        StoreGridUnlocked(static_cast<unsigned>(id), &zero, 1, 1, source);
        return;
    }
    StoreGridUnlocked(static_cast<unsigned>(id), colors, rows, cols, source);
}

void FlushDevices(const char* source)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if(source)
    {
        std::memset(g_source, 0, sizeof(g_source));
        std::strncpy(g_source, source, sizeof(g_source) - 1);
    }
    FlushDevicesUnlocked();
}

void ClearDevices()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    for(unsigned i = 0; i < kDeviceCount; ++i)
    {
        g_grids[i] = DeviceGridState{};
    }
    g_dirty = false;
}

} // namespace GwUdp
