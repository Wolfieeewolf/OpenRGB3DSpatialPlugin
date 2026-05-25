// SPDX-License-Identifier: GPL-2.0-only

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "GameLightingBridge.h"
#include "GameLightingFormat.h"
#include "LogManager.h"

#include <algorithm>
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

bool GameLightingBridge::Register(ResourceManagerInterface* rm)
{
    (void)rm;
    StartUdpListener();
    return true;
}

void GameLightingBridge::Unregister(ResourceManagerInterface* rm)
{
    (void)rm;
    StopUdpListener();
}

void GameLightingBridge::StartUdpListener()
{
    if(udp_running)
    {
        return;
    }

#ifdef _WIN32
    WSADATA wsa_data;
    if(WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        LOG_ERROR("[3DSpatial] game lighting UDP: WinSock init failed");
        return;
    }
#endif

    int fd = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(fd < 0)
    {
#ifdef _WIN32
        LOG_ERROR("[3DSpatial] game lighting UDP: socket() failed (WSA error %d)", (int)WSAGetLastError());
        WSACleanup();
#else
        LOG_ERROR("[3DSpatial] game lighting UDP: socket() failed (errno %d)", errno);
#endif
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
        WSACleanup();
#else
        LOG_ERROR("[3DSpatial] game lighting UDP: bind failed on 127.0.0.1:%u (errno %d)",
                  (unsigned int)GameLightingFormat::kListenPort,
                  errno);
#endif
        CloseSocketFd(fd);
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
    CloseSocketFd(udp_socket_fd);

    if(udp_thread.joinable())
    {
        udp_thread.join();
    }

#ifdef _WIN32
    WSACleanup();
#endif
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
        bool ok = false;
        {
            std::lock_guard<std::mutex> guard(stats_mutex);
            stats.packets_total++;
            ok = ProcessIncomingFrame(buf.data(), static_cast<std::size_t>(n), source);
            if(ok)
            {
                stats.packets_valid++;
                stats.last_source = source;
            }
            else
            {
                stats.packets_error++;
            }
        }
        if(ok)
        {
            g_lighting_data_revision.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

bool GameLightingBridge::ProcessIncomingFrame(const char* data, std::size_t size, std::string& out_source)
{
    if(!GameLightingFormat::IsMagic(data, size))
    {
        return false;
    }

    if(size < GameLightingFormat::kUniformRgbFrameSize)
    {
        return false;
    }

    GameLightingFormat::UniformRgbFrame frame{};
    std::memcpy(&frame, data, GameLightingFormat::kUniformRgbFrameSize);

    if(frame.frame_type != static_cast<std::uint8_t>(GameLightingFormat::FrameType::UniformRgb))
    {
        return false;
    }

    char src_buf[sizeof(frame.source) + 1]{};
    std::memcpy(src_buf, frame.source, sizeof(frame.source));
    out_source = src_buf;

    {
        std::lock_guard<std::mutex> guard(stats_mutex);
        snapshot.has_frame = true;
        snapshot.uniform_rgb = true;
        snapshot.sequence = frame.sequence;
        snapshot.timestamp_ms = frame.timestamp_ms;
        snapshot.received_ms = NowMs();
        snapshot.r = ClampUnit(frame.r);
        snapshot.g = ClampUnit(frame.g);
        snapshot.b = ClampUnit(frame.b);
        snapshot.zones.clear();
        snapshot.source = out_source;
    }

    return true;
}

GameLightingBridge::LightingSnapshot GameLightingBridge::GetSnapshot()
{
    std::lock_guard<std::mutex> guard(stats_mutex);
    return snapshot;
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

bool GameLightingBridge::IsConnected(unsigned long long stale_ms)
{
    std::lock_guard<std::mutex> guard(stats_mutex);
    if(!snapshot.has_frame)
    {
        return false;
    }
    const unsigned long long now = NowMs();
    return now - snapshot.received_ms <= stale_ms;
}
