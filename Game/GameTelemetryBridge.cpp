// SPDX-License-Identifier: GPL-2.0-only

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "GameTelemetryBridge.h"
#include "LogManager.h"

#include <nlohmann/json.hpp>
#include <vector>
#include <cstring>
#include <chrono>
#include <algorithm>
#include <cmath>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#endif

namespace
{
static const unsigned short kGameUdpPort = 9876;

static unsigned long long NowMs()
{
    return (unsigned long long)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static float LerpFloat(float a, float b, float t)
{
    return a + (b - a) * t;
}

static void CloseSocketFd(int& fd)
{
    if(fd < 0)
    {
        return;
    }
#ifdef _WIN32
    closesocket((SOCKET)fd);
#else
    close(fd);
#endif
    fd = -1;
}
}

std::mutex GameTelemetryBridge::stats_mutex;
GameTelemetryBridge::Stats GameTelemetryBridge::stats;
GameTelemetryBridge::TelemetrySnapshot GameTelemetryBridge::telemetry;

GameTelemetryBridge::GameTelemetryBridge()
{
    udp_running = false;
    udp_socket_fd = -1;
}

GameTelemetryBridge::~GameTelemetryBridge()
{
    StopUdpListener();
}

bool GameTelemetryBridge::Register(ResourceManagerInterface* rm)
{
    (void)rm;
    StartUdpListener();
    return true;
}

void GameTelemetryBridge::Unregister(ResourceManagerInterface* rm)
{
    (void)rm;
    StopUdpListener();
}

void GameTelemetryBridge::StartUdpListener()
{
    if(udp_running)
    {
        return;
    }

#ifdef _WIN32
    WSADATA wsa_data;
    if(WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        LOG_ERROR("[3DSpatial] game telemetry UDP: WinSock init failed");
        return;
    }
#endif

    int fd = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(fd < 0)
    {
        LOG_ERROR("[3DSpatial] game telemetry UDP: socket() failed");
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kGameUdpPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if(bind(fd, (sockaddr*)&addr, (socklen_t)sizeof(addr)) != 0)
    {
        LOG_ERROR("[3DSpatial] game telemetry UDP: bind failed on 127.0.0.1:%u", (unsigned int)kGameUdpPort);
        CloseSocketFd(fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    udp_socket_fd = fd;
    udp_running = true;
    udp_thread = std::thread(&GameTelemetryBridge::UdpListenLoop, this);
    LOG_INFO("[3DSpatial] game telemetry UDP listening on 127.0.0.1:%u", (unsigned int)kGameUdpPort);
}

void GameTelemetryBridge::StopUdpListener()
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

void GameTelemetryBridge::UdpListenLoop()
{
    /* world_light JSON with ~48 probes can exceed 8 KiB; truncate = parse fail or partial data. */
    std::vector<char> buf(65507);
    while(udp_running)
    {
        if(udp_socket_fd < 0)
        {
            break;
        }

        sockaddr_in src_addr;
        socklen_t src_len = (socklen_t)sizeof(src_addr);
        int n = (int)recvfrom(udp_socket_fd, buf.data(), (int)buf.size(), 0, (sockaddr*)&src_addr, &src_len);
        if(n <= 0)
        {
            if(!udp_running)
            {
                break;
            }
            continue;
        }

        std::string source;
        std::string type;
        bool valid = ProcessIncomingJson(buf.data(), (size_t)n, source, type);

        std::lock_guard<std::mutex> guard(stats_mutex);
        stats.packets_total++;
        if(valid)
        {
            stats.packets_valid++;
            stats.last_source = source;
            stats.last_type = type;
        }
        else
        {
            stats.packets_error++;
        }
    }
}

bool GameTelemetryBridge::ProcessIncomingJson(const char* data, size_t size, std::string& out_source, std::string& out_type)
{
    if(data == nullptr || size == 0)
    {
        return false;
    }

    try
    {
        nlohmann::json msg = nlohmann::json::parse(data, data + size);
        if(msg.is_object() &&
           msg.contains("version") && msg["version"].is_number_integer() &&
           msg.contains("type") && msg["type"].is_string() &&
           msg.contains("timestamp_ms") && msg["timestamp_ms"].is_number_integer() &&
           msg.contains("source") && msg["source"].is_string())
        {
            int version = msg["version"].get<int>();
            if(version == 1)
            {
                out_type = msg["type"].get<std::string>();
                out_source = msg["source"].get<std::string>();

                std::lock_guard<std::mutex> guard(stats_mutex);
                telemetry.last_source = out_source;
                telemetry.last_type = out_type;
                telemetry.last_event_ms = NowMs();
                if(out_type == "player_pose")
                {
                    if(msg.contains("x") && msg.contains("y") && msg.contains("z") &&
                       msg.contains("fx") && msg.contains("fy") && msg.contains("fz") &&
                       msg.contains("ux") && msg.contains("uy") && msg.contains("uz"))
                    {
                        float nx = msg["x"].get<float>();
                        float ny = msg["y"].get<float>();
                        float nz = msg["z"].get<float>();
                        float nfx = msg["fx"].get<float>();
                        float nfy = msg["fy"].get<float>();
                        float nfz = msg["fz"].get<float>();
                        float nux = msg["ux"].get<float>();
                        float nuy = msg["uy"].get<float>();
                        float nuz = msg["uz"].get<float>();
                        if(!telemetry.has_player_pose)
                        {
                            telemetry.player_x = nx;
                            telemetry.player_y = ny;
                            telemetry.player_z = nz;
                            telemetry.forward_x = nfx;
                            telemetry.forward_y = nfy;
                            telemetry.forward_z = nfz;
                            telemetry.up_x = nux;
                            telemetry.up_y = nuy;
                            telemetry.up_z = nuz;
                        }
                        else
                        {
                            const float alpha = 0.35f;
                            telemetry.player_x = LerpFloat(telemetry.player_x, nx, alpha);
                            telemetry.player_y = LerpFloat(telemetry.player_y, ny, alpha);
                            telemetry.player_z = LerpFloat(telemetry.player_z, nz, alpha);
                            telemetry.forward_x = LerpFloat(telemetry.forward_x, nfx, alpha);
                            telemetry.forward_y = LerpFloat(telemetry.forward_y, nfy, alpha);
                            telemetry.forward_z = LerpFloat(telemetry.forward_z, nfz, alpha);
                            telemetry.up_x = LerpFloat(telemetry.up_x, nux, alpha);
                            telemetry.up_y = LerpFloat(telemetry.up_y, nuy, alpha);
                            telemetry.up_z = LerpFloat(telemetry.up_z, nuz, alpha);
                        }
                        telemetry.has_player_pose = true;
                    }
                    if(msg.contains("camera_mode") && msg["camera_mode"].is_string())
                    {
                        std::string cm = msg["camera_mode"].get<std::string>();
                        if(cm == "first_person")
                        {
                            telemetry.camera_mode = 1;
                            telemetry.has_camera_mode = true;
                        }
                        else if(cm == "third_person")
                        {
                            telemetry.camera_mode = 2;
                            telemetry.has_camera_mode = true;
                        }
                    }
                }
                else if(out_type == "damage_event")
                {
                    telemetry.has_damage_event = true;
                    telemetry.damage_amount = msg.value("amount", 10.0f);
                    telemetry.damage_dir_x = msg.value("dir_x", 0.0f);
                    telemetry.damage_dir_y = msg.value("dir_y", 0.0f);
                    telemetry.damage_dir_z = msg.value("dir_z", 0.0f);
                    telemetry.damage_received_ms = NowMs();
                }
                else if(out_type == "health_state")
                {
                    telemetry.has_health_state = true;
                    telemetry.health = msg.value("health", 100.0f);
                    telemetry.health_max = (std::max)(1.0f, msg.value("health_max", 100.0f));
                }
                else if(out_type == "world_light")
                {
                    float wx = msg.value("x", 0.0f);
                    float wy = msg.value("y", 0.0f);
                    float wz = msg.value("z", 0.0f);
                    if(!telemetry.has_world_light)
                    {
                        telemetry.world_light_x = wx;
                        telemetry.world_light_y = wy;
                        telemetry.world_light_z = wz;
                    }
                    else
                    {
                        const float alpha = 0.45f;
                        telemetry.world_light_x = LerpFloat(telemetry.world_light_x, wx, alpha);
                        telemetry.world_light_y = LerpFloat(telemetry.world_light_y, wy, alpha);
                        telemetry.world_light_z = LerpFloat(telemetry.world_light_z, wz, alpha);
                    }
                    telemetry.has_world_light = true;
                    telemetry.world_light_r = (unsigned char)(std::max)(0, (std::min)(255, msg.value("r", 255)));
                    telemetry.world_light_g = (unsigned char)(std::max)(0, (std::min)(255, msg.value("g", 255)));
                    telemetry.world_light_b = (unsigned char)(std::max)(0, (std::min)(255, msg.value("b", 255)));
                    telemetry.world_light_intensity = (std::max)(0.0f, msg.value("intensity", 1.0f));
                    if(msg.contains("sky_r") && msg.contains("sky_g") && msg.contains("sky_b") &&
                       msg.contains("mid_r") && msg.contains("mid_g") && msg.contains("mid_b") &&
                       msg.contains("ground_r") && msg.contains("ground_g") && msg.contains("ground_b"))
                    {
                        telemetry.has_world_layers = true;
                        telemetry.world_sky_r = (unsigned char)(std::max)(0, (std::min)(255, msg.value("sky_r", 170)));
                        telemetry.world_sky_g = (unsigned char)(std::max)(0, (std::min)(255, msg.value("sky_g", 190)));
                        telemetry.world_sky_b = (unsigned char)(std::max)(0, (std::min)(255, msg.value("sky_b", 255)));
                        telemetry.world_mid_r = (unsigned char)(std::max)(0, (std::min)(255, msg.value("mid_r", 140)));
                        telemetry.world_mid_g = (unsigned char)(std::max)(0, (std::min)(255, msg.value("mid_g", 180)));
                        telemetry.world_mid_b = (unsigned char)(std::max)(0, (std::min)(255, msg.value("mid_b", 120)));
                        telemetry.world_ground_r = (unsigned char)(std::max)(0, (std::min)(255, msg.value("ground_r", 100)));
                        telemetry.world_ground_g = (unsigned char)(std::max)(0, (std::min)(255, msg.value("ground_g", 120)));
                        telemetry.world_ground_b = (unsigned char)(std::max)(0, (std::min)(255, msg.value("ground_b", 80)));
                    }
                    telemetry.has_world_probes = false;
                    telemetry.world_probe_count = 0;
                    telemetry.has_world_probe_cube = false;
                    telemetry.has_world_probe_compass = false;
                    for(int cf = 0; cf < 6; ++cf)
                    {
                        telemetry.world_probe_cube_r[cf] = 0.0f;
                        telemetry.world_probe_cube_g[cf] = 0.0f;
                        telemetry.world_probe_cube_b[cf] = 0.0f;
                    }
                    for(int cf = 0; cf < 9; ++cf)
                    {
                        telemetry.world_probe_compass_r[cf] = 0.0f;
                        telemetry.world_probe_compass_g[cf] = 0.0f;
                        telemetry.world_probe_compass_b[cf] = 0.0f;
                    }
                    /* is_number_integer() alone can miss number_unsigned / float from some encoders. */
                    if(msg.contains("probe_count") && msg["probe_count"].is_number())
                    {
                        int probe_count = std::clamp(static_cast<int>(std::lround(msg["probe_count"].get<double>())), 0, 64);
                        for(int i = 0; i < probe_count; i++)
                        {
                            const std::string p = "p" + std::to_string(i) + "_";
                            const std::string kdx = p + "dx";
                            const std::string kdy = p + "dy";
                            const std::string kdz = p + "dz";
                            const std::string kr = p + "r";
                            const std::string kg = p + "g";
                            const std::string kb = p + "b";
                            const std::string ki = p + "i";
                            telemetry.world_probe_dir_x[i] = msg.value(kdx, 0.0f);
                            telemetry.world_probe_dir_y[i] = msg.value(kdy, 0.0f);
                            telemetry.world_probe_dir_z[i] = msg.value(kdz, 0.0f);
                            telemetry.world_probe_r[i] = (unsigned char)(std::max)(0, (std::min)(255, msg.value(kr, 0)));
                            telemetry.world_probe_g[i] = (unsigned char)(std::max)(0, (std::min)(255, msg.value(kg, 0)));
                            telemetry.world_probe_b[i] = (unsigned char)(std::max)(0, (std::min)(255, msg.value(kb, 0)));
                            telemetry.world_probe_intensity[i] = (std::max)(0.0f, msg.value(ki, 0.0f));
                        }
                        telemetry.world_probe_count = probe_count;
                        telemetry.has_world_probes = probe_count > 0;

                        float fr[6] = {0}, fg[6] = {0}, fb[6] = {0}, fw[6] = {0};
                        for(int i = 0; i < probe_count; ++i)
                        {
                            float px = telemetry.world_probe_dir_x[i];
                            float py = telemetry.world_probe_dir_y[i];
                            float pz = telemetry.world_probe_dir_z[i];
                            const float plen2 = px * px + py * py + pz * pz;
                            if(plen2 < 1e-10f)
                            {
                                continue;
                            }
                            const float invl = 1.0f / std::sqrt(plen2);
                            px *= invl;
                            py *= invl;
                            pz *= invl;
                            const float pi = telemetry.world_probe_intensity[i];
                            if(pi <= 0.0f)
                            {
                                continue;
                            }
                            const float uxp = (std::max)(0.0f, px);
                            const float uxn = (std::max)(0.0f, -px);
                            const float uyp = (std::max)(0.0f, py);
                            const float uyn = (std::max)(0.0f, -py);
                            const float uzp = (std::max)(0.0f, pz);
                            const float uzn = (std::max)(0.0f, -pz);
                            const float uxp2 = uxp * uxp;
                            const float uxn2 = uxn * uxn;
                            const float uyp2 = uyp * uyp;
                            const float uyn2 = uyn * uyn;
                            const float uzp2 = uzp * uzp;
                            const float uzn2 = uzn * uzn;
                            const float s = uxp2 + uxn2 + uyp2 + uyn2 + uzp2 + uzn2;
                            if(s < 1e-10f)
                            {
                                continue;
                            }
                            const float invs_pi = pi / s;
                            const float rch = static_cast<float>(telemetry.world_probe_r[i]);
                            const float gch = static_cast<float>(telemetry.world_probe_g[i]);
                            const float bch = static_cast<float>(telemetry.world_probe_b[i]);
                            fr[0] += rch * uxp2 * invs_pi;
                            fg[0] += gch * uxp2 * invs_pi;
                            fb[0] += bch * uxp2 * invs_pi;
                            fw[0] += uxp2 * invs_pi;
                            fr[1] += rch * uxn2 * invs_pi;
                            fg[1] += gch * uxn2 * invs_pi;
                            fb[1] += bch * uxn2 * invs_pi;
                            fw[1] += uxn2 * invs_pi;
                            fr[2] += rch * uyp2 * invs_pi;
                            fg[2] += gch * uyp2 * invs_pi;
                            fb[2] += bch * uyp2 * invs_pi;
                            fw[2] += uyp2 * invs_pi;
                            fr[3] += rch * uyn2 * invs_pi;
                            fg[3] += gch * uyn2 * invs_pi;
                            fb[3] += bch * uyn2 * invs_pi;
                            fw[3] += uyn2 * invs_pi;
                            fr[4] += rch * uzp2 * invs_pi;
                            fg[4] += gch * uzp2 * invs_pi;
                            fb[4] += bch * uzp2 * invs_pi;
                            fw[4] += uzp2 * invs_pi;
                            fr[5] += rch * uzn2 * invs_pi;
                            fg[5] += gch * uzn2 * invs_pi;
                            fb[5] += bch * uzn2 * invs_pi;
                            fw[5] += uzn2 * invs_pi;
                        }
                        int nonempty = 0;
                        float sum_r = 0.0f;
                        float sum_g = 0.0f;
                        float sum_b = 0.0f;
                        for(int f = 0; f < 6; ++f)
                        {
                            if(fw[f] > 1e-8f)
                            {
                                telemetry.world_probe_cube_r[f] = fr[f] / fw[f];
                                telemetry.world_probe_cube_g[f] = fg[f] / fw[f];
                                telemetry.world_probe_cube_b[f] = fb[f] / fw[f];
                                sum_r += telemetry.world_probe_cube_r[f];
                                sum_g += telemetry.world_probe_cube_g[f];
                                sum_b += telemetry.world_probe_cube_b[f];
                                ++nonempty;
                            }
                            else
                            {
                                telemetry.world_probe_cube_r[f] = 0.0f;
                                telemetry.world_probe_cube_g[f] = 0.0f;
                                telemetry.world_probe_cube_b[f] = 0.0f;
                            }
                        }
                        if(nonempty > 0 && nonempty < 6)
                        {
                            const float invn = 1.0f / static_cast<float>(nonempty);
                            const float ar = sum_r * invn;
                            const float ag = sum_g * invn;
                            const float ab = sum_b * invn;
                            for(int f = 0; f < 6; ++f)
                            {
                                if(fw[f] <= 1e-8f)
                                {
                                    telemetry.world_probe_cube_r[f] = ar;
                                    telemetry.world_probe_cube_g[f] = ag;
                                    telemetry.world_probe_cube_b[f] = ab;
                                }
                            }
                        }
                        telemetry.has_world_probe_cube = (nonempty > 0);

                        /* Horizontal compass in player space: forward = N, right = E (telemetry up × forward corrected). */
                        float ux = telemetry.up_x;
                        float uy = telemetry.up_y;
                        float uz = telemetry.up_z;
                        float ulen = std::sqrt(ux * ux + uy * uy + uz * uz);
                        if(ulen < 1e-5f)
                        {
                            ux = 0.0f;
                            uy = 1.0f;
                            uz = 0.0f;
                            ulen = 1.0f;
                        }
                        else
                        {
                            ux /= ulen;
                            uy /= ulen;
                            uz /= ulen;
                        }
                        float fx = telemetry.forward_x;
                        float fy = telemetry.forward_y;
                        float fz = telemetry.forward_z;
                        float flen = std::sqrt(fx * fx + fy * fy + fz * fz);
                        if(flen < 1e-5f)
                        {
                            fx = 0.0f;
                            fy = 0.0f;
                            fz = 1.0f;
                            flen = 1.0f;
                        }
                        else
                        {
                            fx /= flen;
                            fy /= flen;
                            fz /= flen;
                        }
                        float fhx = fx - ux * (fx * ux + fy * uy + fz * uz);
                        float fhy = fy - uy * (fx * ux + fy * uy + fz * uz);
                        float fhz = fz - uz * (fx * ux + fy * uy + fz * uz);
                        float fhlen = std::sqrt(fhx * fhx + fhy * fhy + fhz * fhz);
                        if(fhlen < 1e-5f)
                        {
                            fhx = 1.0f;
                            fhy = 0.0f;
                            fhz = 0.0f;
                            fhlen = 1.0f;
                        }
                        else
                        {
                            fhx /= fhlen;
                            fhy /= fhlen;
                            fhz /= fhlen;
                        }
                        float rhx = uy * fhz - uz * fhy;
                        float rhy = uz * fhx - ux * fhz;
                        float rhz = ux * fhy - uy * fhx;
                        float rhlen = std::sqrt(rhx * rhx + rhy * rhy + rhz * rhz);
                        if(rhlen > 1e-5f)
                        {
                            rhx /= rhlen;
                            rhy /= rhlen;
                            rhz /= rhlen;
                        }
                        auto add_norm = [](float ax, float ay, float az, float bx, float by, float bz, float& ox, float& oy, float& oz) {
                            ox = ax + bx;
                            oy = ay + by;
                            oz = az + bz;
                            const float l = std::sqrt(ox * ox + oy * oy + oz * oz);
                            if(l > 1e-5f)
                            {
                                ox /= l;
                                oy /= l;
                                oz /= l;
                            }
                        };
                        float sx[8], sy[8], sz[8];
                        sx[0] = fhx;
                        sy[0] = fhy;
                        sz[0] = fhz;
                        sx[4] = -fhx;
                        sy[4] = -fhy;
                        sz[4] = -fhz;
                        sx[2] = rhx;
                        sy[2] = rhy;
                        sz[2] = rhz;
                        sx[6] = -rhx;
                        sy[6] = -rhy;
                        sz[6] = -rhz;
                        add_norm(fhx, fhy, fhz, rhx, rhy, rhz, sx[1], sy[1], sz[1]);
                        add_norm(-fhx, -fhy, -fhz, rhx, rhy, rhz, sx[3], sy[3], sz[3]);
                        add_norm(-fhx, -fhy, -fhz, -rhx, -rhy, -rhz, sx[5], sy[5], sz[5]);
                        add_norm(fhx, fhy, fhz, -rhx, -rhy, -rhz, sx[7], sy[7], sz[7]);

                        constexpr float k_compass_fold = 2.2f;
                        float cr[8] = {0}, cg[8] = {0}, cb[8] = {0}, cw[8] = {0};
                        for(int i = 0; i < probe_count; ++i)
                        {
                            float px = telemetry.world_probe_dir_x[i];
                            float py = telemetry.world_probe_dir_y[i];
                            float pz = telemetry.world_probe_dir_z[i];
                            const float pl2 = px * px + py * py + pz * pz;
                            if(pl2 < 1e-10f)
                            {
                                continue;
                            }
                            const float invl = 1.0f / std::sqrt(pl2);
                            px *= invl;
                            py *= invl;
                            pz *= invl;
                            const float pi = telemetry.world_probe_intensity[i];
                            if(pi <= 0.0f)
                            {
                                continue;
                            }
                            const float rch = static_cast<float>(telemetry.world_probe_r[i]);
                            const float gch = static_cast<float>(telemetry.world_probe_g[i]);
                            const float bch = static_cast<float>(telemetry.world_probe_b[i]);
                            for(int k = 0; k < 8; ++k)
                            {
                                const float dp = (std::max)(0.0f, px * sx[k] + py * sy[k] + pz * sz[k]);
                                const float w = std::pow(dp, k_compass_fold) * pi;
                                if(w <= 1e-10f)
                                {
                                    continue;
                                }
                                cw[k] += w;
                                cr[k] += rch * w;
                                cg[k] += gch * w;
                                cb[k] += bch * w;
                            }
                        }
                        int c_nonempty = 0;
                        float c_sumr = 0.0f;
                        float c_sumg = 0.0f;
                        float c_sumb = 0.0f;
                        float max_r = 0.0f;
                        float max_g = 0.0f;
                        float max_b = 0.0f;
                        for(int k = 0; k < 8; ++k)
                        {
                            if(cw[k] > 1e-8f)
                            {
                                telemetry.world_probe_compass_r[k] = cr[k] / cw[k];
                                telemetry.world_probe_compass_g[k] = cg[k] / cw[k];
                                telemetry.world_probe_compass_b[k] = cb[k] / cw[k];
                                c_sumr += telemetry.world_probe_compass_r[k];
                                c_sumg += telemetry.world_probe_compass_g[k];
                                c_sumb += telemetry.world_probe_compass_b[k];
                                max_r = (std::max)(max_r, telemetry.world_probe_compass_r[k]);
                                max_g = (std::max)(max_g, telemetry.world_probe_compass_g[k]);
                                max_b = (std::max)(max_b, telemetry.world_probe_compass_b[k]);
                                ++c_nonempty;
                            }
                            else
                            {
                                telemetry.world_probe_compass_r[k] = 0.0f;
                                telemetry.world_probe_compass_g[k] = 0.0f;
                                telemetry.world_probe_compass_b[k] = 0.0f;
                            }
                        }
                        if(c_nonempty > 0 && c_nonempty < 8)
                        {
                            const float invc = 1.0f / static_cast<float>(c_nonempty);
                            const float ar = c_sumr * invc;
                            const float ag = c_sumg * invc;
                            const float ab = c_sumb * invc;
                            for(int k = 0; k < 8; ++k)
                            {
                                if(cw[k] <= 1e-8f)
                                {
                                    telemetry.world_probe_compass_r[k] = ar;
                                    telemetry.world_probe_compass_g[k] = ag;
                                    telemetry.world_probe_compass_b[k] = ab;
                                }
                            }
                        }
                        if(c_nonempty > 0)
                        {
                            float mean_r = 0.0f;
                            float mean_g = 0.0f;
                            float mean_b = 0.0f;
                            max_r = 0.0f;
                            max_g = 0.0f;
                            max_b = 0.0f;
                            for(int k = 0; k < 8; ++k)
                            {
                                const float rr = telemetry.world_probe_compass_r[k];
                                const float gg = telemetry.world_probe_compass_g[k];
                                const float bb = telemetry.world_probe_compass_b[k];
                                mean_r += rr;
                                mean_g += gg;
                                mean_b += bb;
                                max_r = (std::max)(max_r, rr);
                                max_g = (std::max)(max_g, gg);
                                max_b = (std::max)(max_b, bb);
                            }
                            mean_r *= 0.125f;
                            mean_g *= 0.125f;
                            mean_b *= 0.125f;
                            constexpr float k_center_lerp = 0.42f;
                            telemetry.world_probe_compass_r[8] = max_r * (1.0f - k_center_lerp) + mean_r * k_center_lerp;
                            telemetry.world_probe_compass_g[8] = max_g * (1.0f - k_center_lerp) + mean_g * k_center_lerp;
                            telemetry.world_probe_compass_b[8] = max_b * (1.0f - k_center_lerp) + mean_b * k_center_lerp;
                        }
                        telemetry.has_world_probe_compass = (c_nonempty > 0);
                    }
                    telemetry.world_light_received_ms = NowMs();
                }
                return true;
            }
        }
    }
    catch(const std::exception&)
    {
    }

    return false;
}

GameTelemetryBridge::TelemetrySnapshot GameTelemetryBridge::GetTelemetrySnapshot()
{
    std::lock_guard<std::mutex> guard(stats_mutex);
    return telemetry;
}

void GameTelemetryBridge::GetStats(unsigned int& packets_total,
                             unsigned int& packets_valid,
                             unsigned int& packets_error,
                             std::string& last_source,
                             std::string& last_type)
{
    std::lock_guard<std::mutex> guard(stats_mutex);
    packets_total = stats.packets_total;
    packets_valid = stats.packets_valid;
    packets_error = stats.packets_error;
    last_source = stats.last_source;
    last_type = stats.last_type;
}

