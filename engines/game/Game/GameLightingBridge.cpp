// SPDX-License-Identifier: GPL-2.0-only

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "GameLightingBridge.h"
#include "GameLightingFormat.h"
#include "WinsockLifetime.h"
#include "PluginLog.h"

#include <chrono>
#include <cstring>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#endif

namespace
{
std::atomic<std::uint64_t> g_lighting_data_revision{0};

static unsigned long long NowMs()
{
    return (unsigned long long)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

static void CloseSocketFd(int& fd)
{
    if(fd < 0)
    {
        return;
    }
#ifdef _WIN32
    closesocket(fd);
#else
    close(fd);
#endif
    fd = -1;
}
} // namespace

std::mutex GameLightingBridge::stats_mutex;
GameLightingBridge::Stats GameLightingBridge::stats;
GameLightingBridge::LightingSnapshot GameLightingBridge::snapshot;

GameLightingBridge::GameLightingBridge()
    : udp_running(false)
    , udp_socket_fd(-1)
{
}

GameLightingBridge::~GameLightingBridge()
{
    StopUdpListener();
}

bool GameLightingBridge::Register(OpenRGBPluginAPIInterface* api)
{
    (void)api;
    StartUdpListener();
    return true;
}

void GameLightingBridge::Unregister(OpenRGBPluginAPIInterface* api)
{
    (void)api;
    StopUdpListener();
}

void GameLightingBridge::StartUdpListener()
{
    if(udp_running)
    {
        return;
    }

    if(!WinsockLifetime::Acquire())
    {
        LOG_ERROR("[3DSpatial] game lighting UDP: WinSock init failed");
        return;
    }

    int fd = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(fd < 0)
    {
#ifdef _WIN32
        LOG_ERROR("[3DSpatial] game lighting UDP: socket() failed (WSA error %d)", (int)WSAGetLastError());
#else
        LOG_ERROR("[3DSpatial] game lighting UDP: socket() failed (errno %d)", errno);
#endif
        WinsockLifetime::Release();
        return;
    }

    /* Do not SO_REUSEADDR — a second binder on Windows can steal GW01 datagrams. */

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(GameLightingFormat::kListenPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if(bind(fd, reinterpret_cast<sockaddr*>(&addr), static_cast<socklen_t>(sizeof(addr))) != 0)
    {
#ifdef _WIN32
        LOG_ERROR("[3DSpatial] game lighting UDP: bind failed on 127.0.0.1:%u (WSA error %d)",
                  (unsigned int)GameLightingFormat::kListenPort,
                  (int)WSAGetLastError());
#else
        LOG_ERROR("[3DSpatial] game lighting UDP: bind failed on 127.0.0.1:%u (errno %d)",
                  (unsigned int)GameLightingFormat::kListenPort,
                  errno);
#endif
        CloseSocketFd(fd);
        WinsockLifetime::Release();
        return;
    }

    udp_socket_fd = fd;
    udp_running = true;
    udp_thread = std::thread(&GameLightingBridge::UdpListenLoop, this);
    LOG_INFO("[3DSpatial] game wrapper lighting UDP listening on 127.0.0.1:%u",
             (unsigned int)GameLightingFormat::kListenPort);
}

void GameLightingBridge::StopUdpListener()
{
    if(!udp_running)
    {
        return;
    }

    udp_running = false;
#ifdef _WIN32
    if(udp_socket_fd >= 0)
    {
        shutdown(udp_socket_fd, SD_BOTH);
    }
#endif
    CloseSocketFd(udp_socket_fd);

    if(udp_thread.joinable())
    {
        udp_thread.join();
    }

    WinsockLifetime::Release();
}

void GameLightingBridge::UdpListenLoop()
{
    std::vector<char> buf(4096);
    while(udp_running)
    {
        if(udp_socket_fd < 0)
        {
            break;
        }

        sockaddr_in src_addr{};
        socklen_t src_len = (socklen_t)sizeof(src_addr);
        int n = static_cast<int>(recvfrom(udp_socket_fd,
                                          buf.data(),
                                          static_cast<int>(buf.size()),
                                          0,
                                          reinterpret_cast<sockaddr*>(&src_addr),
                                          &src_len));
        if(n <= 0)
        {
            continue;
        }

        std::string source;
        const bool ok = ProcessIncomingFrame(buf.data(), static_cast<std::size_t>(n), source);
        if(ok)
        {
            g_lighting_data_revision.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

bool GameLightingBridge::ProcessIncomingFrame(const char* data, std::size_t size, std::string& out_source)
{
    std::lock_guard<std::mutex> guard(stats_mutex);
    stats.packets_total++;

    if(!GameLightingFormat::IsMagic(data, size) || size < GameLightingFormat::kDeviceGridHeaderSize)
    {
        stats.packets_error++;
        return false;
    }

    const auto frame_type = static_cast<GameLightingFormat::FrameType>(static_cast<std::uint8_t>(data[16]));
    if(frame_type != GameLightingFormat::FrameType::DeviceGrid)
    {
        stats.packets_error++;
        return false;
    }

    char src_buf[25]{};
    std::memcpy(src_buf, data + 17, 24);
    src_buf[24] = '\0';
    out_source = src_buf;

    GameLightingFormat::DeviceGridHeader hdr{};
    std::memcpy(&hdr, data, sizeof(hdr));
    if(hdr.device_id >= GameLightingFormat::kDeviceCount || hdr.rows == 0 || hdr.cols == 0)
    {
        stats.packets_error++;
        return false;
    }
    if(hdr.rows > GameLightingFormat::kMaxGridRows || hdr.cols > GameLightingFormat::kMaxGridCols)
    {
        stats.packets_error++;
        return false;
    }

    const std::size_t cells = (std::size_t)hdr.rows * (std::size_t)hdr.cols;
    const std::size_t need = GameLightingFormat::kDeviceGridHeaderSize + cells * 3u;
    if(size < need || cells > GameLightingFormat::kMaxGridCells)
    {
        stats.packets_error++;
        return false;
    }

    DeviceSample& out = snapshot.devices[hdr.device_id];
    out.active = (hdr.flags & GameLightingFormat::kDeviceGridFlagActive) != 0;
    out.has_grid = out.active;
    out.grid_rows = hdr.rows;
    out.grid_cols = hdr.cols;
    out.grid_rgb.fill(0);
    std::memcpy(out.grid_rgb.data(), data + GameLightingFormat::kDeviceGridHeaderSize, cells * 3u);

    double ar = 0.0, ag = 0.0, ab = 0.0;
    int an = 0;
    for(std::size_t i = 0; i < cells; ++i)
    {
        const std::uint8_t r = out.grid_rgb[i * 3u + 0];
        const std::uint8_t g = out.grid_rgb[i * 3u + 1];
        const std::uint8_t b = out.grid_rgb[i * 3u + 2];
        if(r == 0 && g == 0 && b == 0)
        {
            continue;
        }
        ar += r;
        ag += g;
        ab += b;
        ++an;
    }
    if(an > 0)
    {
        out.r = (float)(ar / (255.0 * (double)an));
        out.g = (float)(ag / (255.0 * (double)an));
        out.b = (float)(ab / (255.0 * (double)an));
    }
    else
    {
        out.r = out.g = out.b = 0.0f;
    }

    snapshot.has_frame = true;
    snapshot.has_devices = true;
    snapshot.sequence = hdr.sequence;
    snapshot.timestamp_ms = hdr.timestamp_ms;
    snapshot.received_ms = NowMs();
    snapshot.source = out_source;
    snapshot.r = out.r;
    snapshot.g = out.g;
    snapshot.b = out.b;

    stats.packets_valid++;
    stats.last_source = out_source;
    return true;
}

GameLightingBridge::LightingSnapshot GameLightingBridge::GetSnapshot()
{
    std::lock_guard<std::mutex> guard(stats_mutex);
    LightingSnapshot out = snapshot;
    if(out.has_frame && snapshot.received_ms != 0)
    {
        const unsigned long long now = NowMs();
        const unsigned long long age =
            (now < snapshot.received_ms) ? 0ull : (now - snapshot.received_ms);
        if(age > 5000ull)
        {
            out.has_frame = false;
            out.has_devices = false;
        }
    }
    return out;
}

std::uint64_t GameLightingBridge::DataRevision()
{
    return g_lighting_data_revision.load(std::memory_order_relaxed);
}

void GameLightingBridge::GetStats(unsigned int& packets_total,
                                  unsigned int& packets_valid,
                                  unsigned int& packets_error,
                                  std::string& last_source)
{
    std::lock_guard<std::mutex> guard(stats_mutex);
    packets_total = stats.packets_total;
    packets_valid = stats.packets_valid;
    packets_error = stats.packets_error;
    last_source = stats.last_source;
}

unsigned long long GameLightingBridge::SnapshotAgeMs()
{
    std::lock_guard<std::mutex> guard(stats_mutex);
    if(!snapshot.has_frame || snapshot.received_ms == 0)
    {
        return ~0ull;
    }
    const unsigned long long now = NowMs();
    if(now < snapshot.received_ms)
    {
        return 0;
    }
    return now - snapshot.received_ms;
}

bool GameLightingBridge::IsConnected(unsigned long long stale_ms)
{
    const unsigned long long age = SnapshotAgeMs();
    return age != ~0ull && age <= stale_ms;
}
