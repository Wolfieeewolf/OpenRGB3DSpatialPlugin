// SPDX-License-Identifier: GPL-2.0-only

#ifndef GAMELIGHTINGBRIDGE_H
#define GAMELIGHTINGBRIDGE_H

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class OpenRGBPluginAPIInterface;

/** UDP listener for game wrapper shims (port 9877). One active source at a time. */
class GameLightingBridge
{
public:
    struct ZoneSample
    {
        std::uint32_t zone_id = 0;
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        /** Compass bitmask (N, NE, E, …, C). 0 = all sectors. */
        std::uint32_t loc = 0;
        /** Vertical layer bitmask; 0 or 1 = all layers. */
        std::uint32_t height = 0;
    };

    struct LightingSnapshot
    {
        bool has_frame = false;
        bool uniform_rgb = true;
        std::uint32_t sequence = 0;
        std::uint64_t timestamp_ms = 0;
        unsigned long long received_ms = 0;
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        std::vector<ZoneSample> zones;
        std::string source;
    };

    GameLightingBridge();
    ~GameLightingBridge();

    bool Register(OpenRGBPluginAPIInterface* api);
    void Unregister(OpenRGBPluginAPIInterface* api);

    static LightingSnapshot GetSnapshot();
    static void GetStats(unsigned int& packets_total,
                         unsigned int& packets_valid,
                         unsigned int& packets_error,
                         std::string& last_source);
    static std::uint64_t DataRevision();
    /** Milliseconds since last valid GW01 frame, or max value if none. */
    static unsigned long long SnapshotAgeMs();
    static bool IsConnected(unsigned long long stale_ms = 5000);

private:
    void StartUdpListener();
    void StopUdpListener();
    void UdpListenLoop();
    static bool ProcessIncomingFrame(const char* data, std::size_t size, std::string& out_source);

    std::atomic<bool> udp_running;
    std::thread udp_thread;
    int udp_socket_fd;

    struct Stats
    {
        unsigned int packets_total = 0;
        unsigned int packets_valid = 0;
        unsigned int packets_error = 0;
        std::string last_source;
    };

    static std::mutex stats_mutex;
    static Stats stats;
    static LightingSnapshot snapshot;
};

#endif
