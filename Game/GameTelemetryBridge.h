// SPDX-License-Identifier: GPL-2.0-only

#ifndef GAMETELEMETRYBRIDGE_H
#define GAMETELEMETRYBRIDGE_H

#include <mutex>
#include <string>
#include <atomic>
#include <thread>

class ResourceManagerInterface;

/** UDP JSON listener (127.0.0.1:9876, protocol v1); shared by all Game-category effects. */
class GameTelemetryBridge
{
public:
    struct TelemetrySnapshot
    {
        bool has_player_pose = false;
        float player_x = 0.0f;
        float player_y = 0.0f;
        float player_z = 0.0f;
        float forward_x = 0.0f;
        float forward_y = 0.0f;
        float forward_z = 1.0f;
        float up_x = 0.0f;
        float up_y = 1.0f;
        float up_z = 0.0f;
        /* camera_mode: 0 = not sent (adapter should set); 1 = first_person; 2 = third_person */
        int camera_mode = 0;
        bool has_camera_mode = false;

        bool has_damage_event = false;
        float damage_amount = 0.0f;
        float damage_dir_x = 0.0f;
        float damage_dir_y = 0.0f;
        float damage_dir_z = 0.0f;
        unsigned long long damage_received_ms = 0;

        bool has_health_state = false;
        float health = 100.0f;
        float health_max = 100.0f;

        bool has_world_light = false;
        float world_light_x = 0.0f;
        float world_light_y = 0.0f;
        float world_light_z = 0.0f;
        unsigned char world_light_r = 255;
        unsigned char world_light_g = 255;
        unsigned char world_light_b = 255;
        float world_light_intensity = 1.0f;
        unsigned long long world_light_received_ms = 0;
        bool has_world_layers = false;
        unsigned char world_sky_r = 170;
        unsigned char world_sky_g = 190;
        unsigned char world_sky_b = 255;
        unsigned char world_mid_r = 140;
        unsigned char world_mid_g = 180;
        unsigned char world_mid_b = 120;
        unsigned char world_ground_r = 100;
        unsigned char world_ground_g = 120;
        unsigned char world_ground_b = 80;
        bool has_world_probes = false;
        int world_probe_count = 0;
        float world_probe_dir_x[64] = {0};
        float world_probe_dir_y[64] = {0};
        float world_probe_dir_z[64] = {0};
        unsigned char world_probe_r[64] = {0};
        unsigned char world_probe_g[64] = {0};
        unsigned char world_probe_b[64] = {0};
        float world_probe_intensity[64] = {0};
        /** Player-local six-face colors (+X,-X,+Y,-Y,+Z,-Z): probes folded onto cube faces (amBX / multi-ambilight style). */
        bool has_world_probe_cube = false;
        float world_probe_cube_r[6] = {0};
        float world_probe_cube_g[6] = {0};
        float world_probe_cube_b[6] = {0};
        /** Horizontal compass (player forward = N): N,NE,E,SE,S,SW,W,NW then [8]=center (smooth max+mean). Soft spherical lobes from probes. */
        bool has_world_probe_compass = false;
        float world_probe_compass_r[9] = {0};
        float world_probe_compass_g[9] = {0};
        float world_probe_compass_b[9] = {0};

        std::string last_source;
        std::string last_type;
        unsigned long long last_event_ms = 0;
    };

    GameTelemetryBridge();
    ~GameTelemetryBridge();

    bool Register(ResourceManagerInterface* rm);
    void Unregister(ResourceManagerInterface* rm);

    static void GetStats(unsigned int& packets_total,
                         unsigned int& packets_valid,
                         unsigned int& packets_error,
                         std::string& last_source,
                         std::string& last_type);
    static TelemetrySnapshot GetTelemetrySnapshot();

private:
    void StartUdpListener();
    void StopUdpListener();
    void UdpListenLoop();
    static bool ProcessIncomingJson(const char* data, size_t size, std::string& out_source, std::string& out_type);

    std::atomic<bool> udp_running;
    std::thread udp_thread;
    int udp_socket_fd;

    struct Stats
    {
        unsigned int packets_total = 0;
        unsigned int packets_valid = 0;
        unsigned int packets_error = 0;
        std::string  last_source;
        std::string  last_type;
    };

    static std::mutex stats_mutex;
    static Stats stats;
    static TelemetrySnapshot telemetry;
};

#endif
