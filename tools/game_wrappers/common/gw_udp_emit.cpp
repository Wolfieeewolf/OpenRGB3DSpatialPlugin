// SPDX-License-Identifier: GPL-2.0-only

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "gw_udp_emit.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>

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

#pragma pack(push, 1)
struct UniformRgbFrame
{
    char magic[4];
    std::uint32_t sequence;
    std::uint64_t timestamp_ms;
    std::uint8_t frame_type;
    char source[24];
    float r;
    float g;
    float b;
};
#pragma pack(pop)

std::atomic<std::uint32_t> g_sequence{0};
std::mutex g_sock_mutex;
int g_sock = -1;
bool g_wsa_started = false;

static std::uint64_t NowMs()
{
    return (std::uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

static bool EnsureSocket()
{
    std::lock_guard<std::mutex> lock(g_sock_mutex);
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

static void ColorRefToRgb(std::uint32_t color, float& r, float& g, float& b)
{
    const float rf = (float)(color & 0xFF) / 255.0f;
    const float gf = (float)((color >> 8) & 0xFF) / 255.0f;
    const float bf = (float)((color >> 16) & 0xFF) / 255.0f;
    r = rf;
    g = gf;
    b = bf;
}
} // namespace

namespace GwUdp
{

bool EmitUniformRgb(float r, float g, float b, const char* source)
{
    if(!EnsureSocket())
    {
        return false;
    }

    UniformRgbFrame frame{};
    std::memcpy(frame.magic, kMagic, 4);
    frame.sequence = ++g_sequence;
    frame.timestamp_ms = NowMs();
    frame.frame_type = 0;
    if(source)
    {
        std::strncpy(frame.source, source, sizeof(frame.source) - 1);
    }
    frame.r = r;
    frame.g = g;
    frame.b = b;

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(kPort);
    dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    std::lock_guard<std::mutex> lock(g_sock_mutex);
    const int sent = sendto(g_sock,
                            reinterpret_cast<const char*>(&frame),
                            (int)sizeof(frame),
                            0,
                            reinterpret_cast<sockaddr*>(&dest),
                            (socklen_t)sizeof(dest));
    return sent == (int)sizeof(frame);
}

void EmitFromColorRef(std::uint32_t c1, std::uint32_t c2, const char* source)
{
    float r1, g1, b1, r2, g2, b2;
    ColorRefToRgb(c1, r1, g1, b1);
    ColorRefToRgb(c2, r2, g2, b2);
    EmitUniformRgb((r1 + r2) * 0.5f, (g1 + g2) * 0.5f, (b1 + b2) * 0.5f, source);
}

} // namespace GwUdp
