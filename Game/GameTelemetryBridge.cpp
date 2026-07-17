// SPDX-License-Identifier: GPL-2.0-only

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "GameTelemetryBridge.h"
#include "RoomSampleFrameShmReader.h"
#include "RoomSampleConfigPublisher.h"
#include "RoomSampleFrameProtocol.h"
#include "PluginLog.h"
#include <vector>
#include <chrono>
#include <algorithm>
#include <climits>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>

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

std::atomic<std::uint64_t> g_telemetry_data_revision{0};

static unsigned long long NowMs()
{
    return (unsigned long long)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static float LerpFloat(float a, float b, float t)
{
    return a + (b - a) * t;
}

static void ApplyPlayerPoseEvent(GameTelemetryBridge::TelemetrySnapshot& telemetry, const nlohmann::json& msg)
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
}

static void ApplyHealthStateEvent(GameTelemetryBridge::TelemetrySnapshot& telemetry, const nlohmann::json& msg)
{
    telemetry.has_health_state = true;
    telemetry.health = msg.value("health", 100.0f);
    telemetry.health_max = (std::max)(1.0f, msg.value("health_max", 100.0f));
    const float hp_per_heart = (std::max)(0.01f, msg.value("hp_per_heart", 2.0f));
    if(msg.contains("hearts") && msg["hearts"].is_number() && msg.contains("hearts_max") && msg["hearts_max"].is_number())
    {
        telemetry.hearts = msg.value("hearts", 0.0f);
        telemetry.hearts_max = (std::max)(1e-3f, msg.value("hearts_max", 1.0f));
    }
    else
    {
        telemetry.hearts = telemetry.health / hp_per_heart;
        telemetry.hearts_max = (std::max)(1e-3f, telemetry.health_max / hp_per_heart);
    }
    telemetry.hunger = msg.value("hunger", 20.0f);
    telemetry.hunger_max = (std::max)(1.0f, msg.value("hunger_max", 20.0f));
    telemetry.air = msg.value("air", 300.0f);
    telemetry.air_max = (std::max)(1.0f, msg.value("air_max", 300.0f));
    telemetry.has_item_durability = msg.value("item_durability_valid", false);
    telemetry.item_durability = msg.value("item_durability", 0.0f);
    telemetry.item_durability_max = (std::max)(1.0f, msg.value("item_durability_max", 1.0f));
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

static RoomSampleFrameShmReader g_room_sample_shm_reader;

GameTelemetryBridge::GameTelemetryBridge()
{
    udp_running = false;
    udp_socket_fd = -1;
}

GameTelemetryBridge::~GameTelemetryBridge()
{
    StopUdpListener();
}

bool GameTelemetryBridge::Register(OpenRGBPluginAPIInterface* rm)
{
    (void)rm;
    StartUdpListener();
    g_room_sample_shm_reader.Start();
    return true;
}

void GameTelemetryBridge::Unregister(OpenRGBPluginAPIInterface* rm)
{
    (void)rm;
    g_room_sample_shm_reader.Stop();
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

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kGameUdpPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if(bind(fd,
            reinterpret_cast<sockaddr*>(&addr),
            static_cast<socklen_t>(sizeof(addr))) != 0)
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
    std::vector<char> buf(65507);
    while(udp_running)
    {
        if(udp_socket_fd < 0)
        {
            break;
        }

        sockaddr_in src_addr;
        socklen_t src_len = (socklen_t)sizeof(src_addr);
        int n = static_cast<int>(
            recvfrom(udp_socket_fd,
                     buf.data(),
                     static_cast<int>(buf.size()),
                     0,
                     reinterpret_cast<sockaddr*>(&src_addr),
                     &src_len));
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

                bool apply_room_sample_shm = false;

                {
                std::lock_guard<std::mutex> guard(stats_mutex);
                telemetry.last_source = out_source;
                telemetry.last_type = out_type;
                telemetry.last_event_ms = NowMs();
                if(out_type == "player_pose")
                {
                    ApplyPlayerPoseEvent(telemetry, msg);
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
                    telemetry.lightning_dir_x = msg.value("dir_x", 0.0f);
                    telemetry.lightning_dir_y = msg.value("dir_y", 1.0f);
                    telemetry.lightning_dir_z = msg.value("dir_z", 0.0f);
                    telemetry.lightning_dir_focus = std::clamp(msg.value("dir_focus", 0.0f), 0.0f, 1.0f);
                    telemetry.lightning_received_ms = NowMs();
                }
                else if(out_type == "health_state")
                {
                    ApplyHealthStateEvent(telemetry, msg);
                }
                else if(out_type == "room_sample_shm_notify")
                {
                    // Deferred: TryApplyLatest locks stats_mutex again.
                    apply_room_sample_shm = true;
                }
                else if(out_type == "world_light")
                {
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
                    if(msg.contains("probe_rgb") && msg["probe_rgb"].is_array())
                    {
                        const nlohmann::json& probe_rgb = msg["probe_rgb"];
                        constexpr int kExpectedProbeRgb = 4 * 9 * 3;
                        if((int)probe_rgb.size() >= kExpectedProbeRgb)
                        {
                            telemetry.has_layered_world_probes = true;
                            telemetry.layered_probe_rgb.fill(0);
                            for(int i = 0; i < kExpectedProbeRgb; i++)
                            {
                                int v = 0;
                                if(probe_rgb[(size_t)i].is_number_integer())
                                {
                                    v = probe_rgb[(size_t)i].get<int>();
                                }
                                else if(probe_rgb[(size_t)i].is_number_float())
                                {
                                    v = (int)probe_rgb[(size_t)i].get<float>();
                                }
                                telemetry.layered_probe_rgb[(size_t)i] = (unsigned char)(std::max)(0, (std::min)(255, v));
                            }
                        }
                        else
                        {
                            telemetry.has_layered_world_probes = false;
                        }
                    }
                    else
                    {
                        telemetry.has_layered_world_probes = false;
                    }
                }
                g_telemetry_data_revision.fetch_add(1, std::memory_order_relaxed);
                }

                if(apply_room_sample_shm)
                {
                    RoomSampleFrameShmReader::TryApplyLatest();
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

std::uint64_t GameTelemetryBridge::TelemetryDataRevision()
{
    return g_telemetry_data_revision.load(std::memory_order_relaxed);
}

void GameTelemetryBridge::NotifyTelemetryDataUpdated()
{
    g_telemetry_data_revision.fetch_add(1, std::memory_order_relaxed);
}

void GameTelemetryBridge::ApplyRoomSampleShmFrame(const RoomSampleFrameProtocol::FrameHeader& hdr,
                                                  std::vector<unsigned char> rgba)
{
    RoomSampleFrameProtocol::ConfigHeader cfg{};
    if(!RoomSampleConfigPublisher::GetLastPublishedConfig(cfg))
    {
        return;
    }
    if(cfg.config_id != 0 && hdr.config_id != 0 && cfg.config_id != hdr.config_id)
    {
        return;
    }

    std::lock_guard<std::mutex> guard(stats_mutex);
    telemetry.room_sample.has_frame = true;
    telemetry.room_sample.frame_id = hdr.frame_id;
    telemetry.room_sample.config_id = hdr.config_id;
    telemetry.room_sample.size_x = hdr.size_x;
    telemetry.room_sample.size_y = hdr.size_y;
    telemetry.room_sample.size_z = hdr.size_z;
    telemetry.room_sample.room_min_x = cfg.room_min_x;
    telemetry.room_sample.room_min_y = cfg.room_min_y;
    telemetry.room_sample.room_min_z = cfg.room_min_z;
    telemetry.room_sample.room_max_x = cfg.room_max_x;
    telemetry.room_sample.room_max_y = cfg.room_max_y;
    telemetry.room_sample.room_max_z = cfg.room_max_z;
    telemetry.room_sample.rgba = std::make_shared<const std::vector<unsigned char>>(std::move(rgba));
    telemetry.room_sample.received_ms =
        (hdr.timestamp_ms > 0) ? hdr.timestamp_ms
                               : (unsigned long long)std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now().time_since_epoch())
                                     .count();
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

