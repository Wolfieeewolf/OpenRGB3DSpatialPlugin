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
    /* Start listener immediately so game UDP works even if Register() were skipped. */
    StartUdpListener();
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
#ifdef _WIN32
        LOG_ERROR("[3DSpatial] game telemetry UDP: socket() failed (WSA error %d)", (int)WSAGetLastError());
        WSACleanup();
#else
        LOG_ERROR("[3DSpatial] game telemetry UDP: socket() failed (errno %d)", errno);
#endif
        return;
    }

    int reuse = 1;
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, (socklen_t)sizeof(reuse)) != 0)
    {
        LOG_ERROR("[3DSpatial] game telemetry UDP: setsockopt(SO_REUSEADDR) failed");
    }

    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kGameUdpPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if(bind(fd, (sockaddr*)&addr, (socklen_t)sizeof(addr)) != 0)
    {
#ifdef _WIN32
        LOG_ERROR("[3DSpatial] game telemetry UDP: bind failed on 127.0.0.1:%u (WSA error %d, port busy or blocked?)",
                  (unsigned int)kGameUdpPort,
                  (int)WSAGetLastError());
        WSACleanup();
#else
        LOG_ERROR("[3DSpatial] game telemetry UDP: bind failed on 127.0.0.1:%u (errno %d, port busy?)",
                  (unsigned int)kGameUdpPort,
                  errno);
#endif
        CloseSocketFd(fd);
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
    /* world_light JSON payload is small; keep max UDP packet size anyway. */
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
        /* Accept any JSON number for version/timestamp (some encoders use float or unsigned). */
        if(msg.is_object() &&
           msg.contains("version") && msg["version"].is_number() &&
           msg.contains("type") && msg["type"].is_string() &&
           msg.contains("timestamp_ms") && msg["timestamp_ms"].is_number() &&
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
                    if(msg.contains("blocks_per_m") && msg["blocks_per_m"].is_number())
                    {
                        telemetry.player_blocks_per_m = (std::max)(0.05f, msg["blocks_per_m"].get<float>());
                        telemetry.has_player_blocks_per_m = true;
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
                else if(out_type == "lightning_event")
                {
                    telemetry.has_lightning_event = true;
                    telemetry.lightning_strength = std::clamp(msg.value("strength", 1.0f), 0.0f, 2.0f);
                    telemetry.lightning_received_ms = NowMs();
                }
                else if(out_type == "health_state")
                {
                    telemetry.has_health_state = true;
                    telemetry.health = msg.value("health", 100.0f);
                    telemetry.health_max = (std::max)(1.0f, msg.value("health_max", 100.0f));
                    telemetry.hunger = msg.value("hunger", 20.0f);
                    telemetry.hunger_max = (std::max)(1.0f, msg.value("hunger_max", 20.0f));
                    telemetry.air = msg.value("air", 300.0f);
                    telemetry.air_max = (std::max)(1.0f, msg.value("air_max", 300.0f));
                    telemetry.has_item_durability = msg.value("item_durability_valid", false);
                    telemetry.item_durability = msg.value("item_durability", 0.0f);
                    telemetry.item_durability_max = (std::max)(1.0f, msg.value("item_durability_max", 1.0f));
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
                    telemetry.world_light_dir_x = msg.value("dir_x", 0.0f);
                    telemetry.world_light_dir_y = msg.value("dir_y", 0.0f);
                    telemetry.world_light_dir_z = msg.value("dir_z", 0.0f);
                    telemetry.world_light_focus = std::clamp(msg.value("dir_focus", 0.0f), 0.0f, 1.0f);
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
                    if(msg.contains("biome_sky_r") && msg.contains("biome_sky_g") && msg.contains("biome_sky_b") &&
                       msg.contains("biome_fog_r") && msg.contains("biome_fog_g") && msg.contains("biome_fog_b") &&
                       msg.contains("water_fog_r") && msg.contains("water_fog_g") && msg.contains("water_fog_b"))
                    {
                        telemetry.has_vanilla_biome_colors = true;
                        telemetry.biome_sky_r = (unsigned char)(std::max)(0, (std::min)(255, msg.value("biome_sky_r", 128)));
                        telemetry.biome_sky_g = (unsigned char)(std::max)(0, (std::min)(255, msg.value("biome_sky_g", 180)));
                        telemetry.biome_sky_b = (unsigned char)(std::max)(0, (std::min)(255, msg.value("biome_sky_b", 255)));
                        telemetry.biome_fog_r = (unsigned char)(std::max)(0, (std::min)(255, msg.value("biome_fog_r", 200)));
                        telemetry.biome_fog_g = (unsigned char)(std::max)(0, (std::min)(255, msg.value("biome_fog_g", 220)));
                        telemetry.biome_fog_b = (unsigned char)(std::max)(0, (std::min)(255, msg.value("biome_fog_b", 255)));
                        telemetry.water_fog_r = (unsigned char)(std::max)(0, (std::min)(255, msg.value("water_fog_r", 32)));
                        telemetry.water_fog_g = (unsigned char)(std::max)(0, (std::min)(255, msg.value("water_fog_g", 64)));
                        telemetry.water_fog_b = (unsigned char)(std::max)(0, (std::min)(255, msg.value("water_fog_b", 120)));
                    }
                    else
                    {
                        telemetry.has_vanilla_biome_colors = false;
                    }
                    if(msg.contains("env_rain") && msg["env_rain"].is_number())
                    {
                        telemetry.env_rain = (std::max)(0.0f, msg["env_rain"].get<float>());
                    }
                    if(msg.contains("env_thunder") && msg["env_thunder"].is_number())
                    {
                        telemetry.env_thunder = (std::max)(0.0f, msg["env_thunder"].get<float>());
                    }
                    if(msg.contains("water_submerge") && msg["water_submerge"].is_number())
                    {
                        telemetry.water_submerge = std::clamp(msg["water_submerge"].get<float>(), 0.0f, 1.0f);
                    }
                    if(msg.contains("dimension") && msg["dimension"].is_string())
                    {
                        telemetry.dimension_id = msg["dimension"].get<std::string>();
                    }
                    telemetry.world_light_received_ms = NowMs();
                }
                return true;
            }
        }
    }
    catch(const std::exception& ex)
    {
        static unsigned long long last_parse_log_ms = 0;
        const unsigned long long now = NowMs();
        if(now - last_parse_log_ms > 4000ULL)
        {
            last_parse_log_ms = now;
            LOG_ERROR("[3DSpatial] game telemetry JSON error: %s (size %zu)", ex.what(), size);
        }
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

