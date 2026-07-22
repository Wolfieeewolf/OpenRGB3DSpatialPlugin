// SPDX-License-Identifier: GPL-2.0-only

#ifndef GAMETELEMETRYBRIDGE_H
#define GAMETELEMETRYBRIDGE_H

#include <mutex>
#include <string>
#include <atomic>
#include <thread>
#include <vector>
#include <cstdint>

#include "RoomSampleFrameProtocol.h"

#include <memory>

class OpenRGBPluginAPIInterface;

// UDP telemetry pose contract (type "player_pose"):
//   x,y,z     — player eye position in the game's native GameWorld space (not feet).
//   fx,fy,fz  — unit look / forward vector in the same space.
//   ux,uy,uz  — unit up vector in the same space (omit only if world Y is always up).
// The plugin derives a right-handed PlayerLocal basis (right = forward × up) and never
// assumes game +Z equals room +Z or player forward.

class GameTelemetryBridge
{
public:
    struct RoomSampleFrameChannel
    {
        bool has_frame = false;
        unsigned int frame_id = 0;
        unsigned int config_id = 0;
        std::uint32_t flags = 0;
        int size_x = 0;
        int size_y = 0;
        int size_z = 0;
        float room_min_x = 0.0f;
        float room_min_y = 0.0f;
        float room_min_z = 0.0f;
        float room_max_x = 0.0f;
        float room_max_y = 0.0f;
        float room_max_z = 0.0f;
        float effect_origin_x = 0.0f;
        float effect_origin_y = 0.0f;
        float effect_origin_z = 0.0f;
        float room_to_world_scale = 0.05f;
        float pos_offset_forward_blocks = 0.0f;
        float pos_offset_right_blocks = 0.0f;
        float pos_offset_up_blocks = 0.0f;
        std::shared_ptr<const std::vector<unsigned char>> rgba;
        unsigned long long received_ms = 0;
    };

    struct TelemetrySnapshot
    {
        RoomSampleFrameChannel room_sample;

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

        bool has_damage_event = false;
        float damage_amount = 0.0f;
        float damage_dir_x = 0.0f;
        float damage_dir_y = 0.0f;
        float damage_dir_z = 0.0f;
        unsigned long long damage_received_ms = 0;

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

        std::string last_source;
        std::string last_type;
        unsigned long long last_event_ms = 0;
    };

    GameTelemetryBridge();
    ~GameTelemetryBridge();

    bool Register(OpenRGBPluginAPIInterface* rm);
    void Unregister(OpenRGBPluginAPIInterface* rm);

    static void GetStats(unsigned int& packets_total,
                         unsigned int& packets_valid,
                         unsigned int& packets_error,
                         std::string& last_source,
                         std::string& last_type);
    static TelemetrySnapshot GetTelemetrySnapshot();

    static std::uint64_t TelemetryDataRevision();
    static void NotifyTelemetryDataUpdated();
    static void ApplyRoomSampleShmFrame(const RoomSampleFrameProtocol::FrameHeader& hdr,
                                        std::vector<unsigned char> rgba);

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
