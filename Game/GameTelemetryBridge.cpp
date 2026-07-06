// SPDX-License-Identifier: GPL-2.0-only

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "GameTelemetryBridge.h"
#include "RoomSampleFrameShmReader.h"
#include "GpuPanoramaFrameShmReader.h"
#include "RoomSampleConfigPublisher.h"
#include "RoomSampleFrameProtocol.h"
#include "GpuPanoramaFrameProtocol.h"
#include "LogManager.h"
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

static SpatialCoordinateSpaces::GameWorldConvention ParseWorldConvention(const nlohmann::json& msg)
{
    if(msg.contains("world_convention"))
    {
        if(msg["world_convention"].is_string())
        {
            const std::string s = msg["world_convention"].get<std::string>();
            if(s == "unity" || s == "left_handed_y_up")
            {
                return SpatialCoordinateSpaces::GameWorldConvention::LeftHandedYUpUnity;
            }
            if(s == "unreal" || s == "left_handed_z_up")
            {
                return SpatialCoordinateSpaces::GameWorldConvention::LeftHandedZUpUnreal;
            }
        }
        else if(msg["world_convention"].is_number_integer())
        {
            const int v = msg["world_convention"].get<int>();
            if(v >= 0 && v <= 2)
            {
                return static_cast<SpatialCoordinateSpaces::GameWorldConvention>(v);
            }
        }
    }
    return SpatialCoordinateSpaces::GameWorldConvention::RightHandedYUp;
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
        telemetry.pose.has_pose = true;
        telemetry.pose.player_x = telemetry.player_x;
        telemetry.pose.player_y = telemetry.player_y;
        telemetry.pose.player_z = telemetry.player_z;
        telemetry.pose.forward_x = telemetry.forward_x;
        telemetry.pose.forward_y = telemetry.forward_y;
        telemetry.pose.forward_z = telemetry.forward_z;
        telemetry.pose.up_x = telemetry.up_x;
        telemetry.pose.up_y = telemetry.up_y;
        telemetry.pose.up_z = telemetry.up_z;
        telemetry.pose.world_convention = ParseWorldConvention(msg);
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

    telemetry.health_state.has_health = true;
    telemetry.health_state.health = telemetry.health;
    telemetry.health_state.health_max = telemetry.health_max;
    telemetry.health_state.hearts = telemetry.hearts;
    telemetry.health_state.hearts_max = telemetry.hearts_max;
    telemetry.health_state.hunger = telemetry.hunger;
    telemetry.health_state.hunger_max = telemetry.hunger_max;
    telemetry.health_state.air = telemetry.air;
    telemetry.health_state.air_max = telemetry.air_max;
    telemetry.health_state.has_item_durability = telemetry.has_item_durability;
    telemetry.health_state.item_durability = telemetry.item_durability;
    telemetry.health_state.item_durability_max = telemetry.item_durability_max;
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
static GpuPanoramaFrameShmReader g_gpu_panorama_shm_reader;

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
    g_room_sample_shm_reader.Start();
    g_gpu_panorama_shm_reader.Start();
    return true;
}

void GameTelemetryBridge::Unregister(ResourceManagerInterface* rm)
{
    (void)rm;
    g_room_sample_shm_reader.Stop();
    g_gpu_panorama_shm_reader.Stop();
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
                bool apply_gpu_panorama_shm = false;

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
                else if(out_type == "gpu_panorama_shm_notify")
                {
                    apply_gpu_panorama_shm = true;
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
                    if(msg.contains("probe_layer_count") && msg.contains("probe_sector_count") &&
                       msg.contains("probe_rgb") && msg["probe_rgb"].is_array())
                    {
                        const int layer_count = msg.value("probe_layer_count", 0);
                        const int sector_count = msg.value("probe_sector_count", 0);
                        const nlohmann::json& probe_rgb = msg["probe_rgb"];
                        const int expected = layer_count * sector_count * 3;
                        if((layer_count == 3 || layer_count == 4) &&
                           sector_count == 9 &&
                           expected > 0 &&
                           (int)probe_rgb.size() >= expected)
                        {
                            telemetry.has_layered_world_probes = true;
                            telemetry.layered_probe_profile = msg.value("probe_layer_profile", layer_count);
                            telemetry.layered_probe_layer_count = layer_count;
                            telemetry.layered_probe_sector_count = sector_count;
                            telemetry.layered_probe_rgb.fill(0);
                            const int cap = (int)telemetry.layered_probe_rgb.size();
                            const int copy_n = (std::min)(expected, cap);
                            for(int i = 0; i < copy_n; i++)
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
                            telemetry.layered_probe_profile = 0;
                            telemetry.layered_probe_layer_count = 0;
                            telemetry.layered_probe_sector_count = 0;
                        }
                    }
                    else
                    {
                        telemetry.has_layered_world_probes = false;
                        telemetry.layered_probe_profile = 0;
                        telemetry.layered_probe_layer_count = 0;
                        telemetry.layered_probe_sector_count = 0;
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
                g_telemetry_data_revision.fetch_add(1, std::memory_order_relaxed);
                }

                if(apply_room_sample_shm)
                {
                    RoomSampleFrameShmReader::TryApplyLatest();
                }
                if(apply_gpu_panorama_shm)
                {
                    GpuPanoramaFrameShmReader::TryApplyLatest();
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
    telemetry.room_sample.transport = 1;
}

void GameTelemetryBridge::ApplyGpuPanoramaShmFrame(const GpuPanoramaFrameProtocol::FrameHeader& hdr,
                                                   std::vector<unsigned char> rgba)
{
    if(rgba.empty())
    {
        return;
    }

    std::lock_guard<std::mutex> guard(stats_mutex);
    telemetry.gpu_panorama.has_frame = true;
    telemetry.gpu_panorama.frame_id = hdr.frame_id;
    telemetry.gpu_panorama.face_w = hdr.face_w;
    telemetry.gpu_panorama.face_h = hdr.face_h;
    telemetry.gpu_panorama.face_count = hdr.face_count;
    telemetry.gpu_panorama.anchor_x = hdr.anchor_x;
    telemetry.gpu_panorama.anchor_y = hdr.anchor_y;
    telemetry.gpu_panorama.anchor_z = hdr.anchor_z;
    telemetry.gpu_panorama.rgba = std::make_shared<const std::vector<unsigned char>>(std::move(rgba));
    telemetry.gpu_panorama.received_ms =
        (hdr.timestamp_ms > 0) ? hdr.timestamp_ms
                               : (unsigned long long)std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now().time_since_epoch())
                                     .count();
    telemetry.gpu_panorama.transport = 1;
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

