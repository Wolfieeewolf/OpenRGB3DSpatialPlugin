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

#include <algorithm>
#include <chrono>
#include <cmath>
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

static float ClampUnit(float v)
{
    return std::clamp(v, 0.0f, 1.0f);
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

    int reuse = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, (socklen_t)sizeof(reuse));

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
    constexpr std::size_t kGw01Size = 53u;

    std::lock_guard<std::mutex> guard(stats_mutex);
    stats.packets_total++;

    if(!GameLightingFormat::IsMagic(data, size) || size != kGw01Size)
    {
        stats.packets_error++;
        return false;
    }

    if(data[16] != static_cast<char>(GameLightingFormat::FrameType::UniformRgb))
    {
        stats.packets_error++;
        return false;
    }

    auto read_u32 = [&](std::size_t off) -> std::uint32_t {
        std::uint32_t v = 0;
        std::memcpy(&v, data + off, sizeof(v));
        return v;
    };
    auto read_u64 = [&](std::size_t off) -> std::uint64_t {
        std::uint64_t v = 0;
        std::memcpy(&v, data + off, sizeof(v));
        return v;
    };
    auto read_f32 = [&](std::size_t off) -> float {
        float v = 0.0f;
        std::memcpy(&v, data + off, sizeof(v));
        return v;
    };

    char src_buf[25]{};
    std::memcpy(src_buf, data + 17, 24);
    src_buf[24] = '\0';
    out_source = src_buf;

    auto finite_unit = [](float v) {
        if(!std::isfinite(v))
        {
            return 0.0f;
        }
        return ClampUnit(v);
    };

    snapshot.has_frame = true;
    snapshot.uniform_rgb = true;
    snapshot.sequence = read_u32(4);
    snapshot.timestamp_ms = read_u64(8);
    snapshot.received_ms = NowMs();
    snapshot.r = finite_unit(read_f32(41));
    snapshot.g = finite_unit(read_f32(45));
    snapshot.b = finite_unit(read_f32(49));
    snapshot.zones.clear();
    snapshot.source = out_source;

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
