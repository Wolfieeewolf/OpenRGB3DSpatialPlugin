// SPDX-License-Identifier: GPL-2.0-only

#ifndef GAMELIGHTINGBRIDGE_H
#define GAMELIGHTINGBRIDGE_H

#include "GameLightingFormat.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <array>

class OpenRGBPluginAPIInterface;

/** UDP listener for universal game-wrapper shims (port 9877). */
class GameLightingBridge
{
public:
    struct DeviceSample
    {
        bool active = false;
        bool has_grid = false; /**< full Chroma matrix present */
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        std::uint8_t grid_rows = 0;
        std::uint8_t grid_cols = 0;
        /** Row-major RGB888, size grid_rows*grid_cols*3 (capped at kMaxGridCells). */
        std::array<std::uint8_t, GameLightingFormat::kMaxGridCells * 3u> grid_rgb{};
    };

    struct LightingSnapshot
    {
        bool has_frame = false;
        bool has_devices = false;
        std::uint32_t sequence = 0;
        std::uint64_t timestamp_ms = 0;
        unsigned long long received_ms = 0;
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        std::array<DeviceSample, GameLightingFormat::kDeviceCount> devices{};
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
