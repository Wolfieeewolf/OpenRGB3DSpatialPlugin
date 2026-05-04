// SPDX-License-Identifier: GPL-2.0-only

#ifndef GAMETELEMETRYBRIDGE_H
#define GAMETELEMETRYBRIDGE_H

#include <mutex>
#include <string>
#include <atomic>
#include <thread>
#include <array>
#include <vector>
#include <cstdint>

class ResourceManagerInterface;

class GameTelemetryBridge
{
public:
    struct PoseChannel
    {
        bool has_pose = false;
        float player_x = 0.0f;
        float player_y = 0.0f;
        float player_z = 0.0f;
        float forward_x = 0.0f;
        float forward_y = 0.0f;
        float forward_z = 1.0f;
        float up_x = 0.0f;
        float up_y = 1.0f;
        float up_z = 0.0f;
    };

    struct HealthChannel
    {
        bool has_health = false;
        float health = 100.0f;
        float health_max = 100.0f;
        float hearts = 10.0f;
        float hearts_max = 10.0f;
        float hunger = 20.0f;
        float hunger_max = 20.0f;
        float air = 300.0f;
        float air_max = 300.0f;
        bool has_item_durability = false;
        float item_durability = 0.0f;
        float item_durability_max = 1.0f;
    };

    struct VoxelFrameChannel
    {
        bool has_voxel_frame = false;
        unsigned int frame_id = 0;
        int size_x = 0;
        int size_y = 0;
        int size_z = 0;
        float origin_x = 0.0f;
        float origin_y = 0.0f;
        float origin_z = 0.0f;
        float voxel_size = 1.0f;
        std::vector<unsigned char> rgba;
        unsigned long long received_ms = 0;
    };

    struct TelemetrySnapshot
    {
        PoseChannel pose;
        HealthChannel health_state;
        VoxelFrameChannel voxel_frame;

        bool has_player_pose = false;
        float player_blocks_per_m = 1.0f;
        bool has_player_blocks_per_m = false;
        float player_x = 0.0f;
        float player_y = 0.0f;
        float player_z = 0.0f;
        float forward_x = 0.0f;
        float forward_y = 0.0f;
        float forward_z = 1.0f;
        float up_x = 0.0f;
        float up_y = 1.0f;
        float up_z = 0.0f;
        int camera_mode = 0;
        bool has_camera_mode = false;

        bool has_damage_event = false;
        float damage_amount = 0.0f;
        float damage_dir_x = 0.0f;
        float damage_dir_y = 0.0f;
        float damage_dir_z = 0.0f;
        unsigned long long damage_received_ms = 0;

        bool has_lightning_event = false;
        float lightning_strength = 0.0f;
        float lightning_dir_x = 0.0f;
        float lightning_dir_y = 1.0f;
        float lightning_dir_z = 0.0f;
        float lightning_dir_focus = 0.0f;
        unsigned long long lightning_received_ms = 0;

        bool has_health_state = false;
        float health = 100.0f;
        float health_max = 100.0f;
        float hearts = 10.0f;
        float hearts_max = 10.0f;
        float hunger = 20.0f;
        float hunger_max = 20.0f;
        float air = 300.0f;
        float air_max = 300.0f;
        bool has_item_durability = false;
        float item_durability = 0.0f;
        float item_durability_max = 1.0f;

        bool has_world_light = false;
        float world_light_x = 0.0f;
        float world_light_y = 0.0f;
        float world_light_z = 0.0f;
        float world_light_dir_x = 0.0f;
        float world_light_dir_y = 0.0f;
        float world_light_dir_z = 0.0f;
        float world_light_focus = 0.0f;
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
        bool has_layered_world_probes = false;
        int layered_probe_profile = 0;
        int layered_probe_layer_count = 0;
        int layered_probe_sector_count = 0;
        std::array<unsigned char, 4 * 9 * 3> layered_probe_rgb{};
        bool has_vanilla_biome_colors = false;
        unsigned char biome_sky_r = 128;
        unsigned char biome_sky_g = 180;
        unsigned char biome_sky_b = 255;
        unsigned char biome_fog_r = 200;
        unsigned char biome_fog_g = 220;
        unsigned char biome_fog_b = 255;
        unsigned char water_fog_r = 32;
        unsigned char water_fog_g = 64;
        unsigned char water_fog_b = 120;
        float water_submerge = 0.0f;
        float env_rain = 0.0f;
        float env_thunder = 0.0f;
        std::string dimension_id;
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

    static std::uint64_t TelemetryDataRevision();

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
