// SPDX-License-Identifier: GPL-2.0-only

#ifndef SCREENCAPTUREMANAGER_H
#define SCREENCAPTUREMANAGER_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <thread>
#include <cstdint>

struct CaptureSourceInfo
{
    std::string     id;
    std::string     name;
    std::string     device_name;
    int             width;
    int             height;
    int             x;
    int             y;
    bool            is_primary;
    bool            is_available;
};

/**
 * @brief A single captured frame with metadata
 */
struct CapturedFrame
{
    std::vector<uint8_t>    data;
    int                     width;
    int                     height;
    uint64_t                frame_id;
    uint64_t                timestamp_ms;
    bool                    valid;

    CapturedFrame()
        : width(0), height(0), frame_id(0), timestamp_ms(0), valid(false)
    {}
};

class ScreenCaptureManager
{
public:
    static ScreenCaptureManager& Instance();
    bool Initialize();
    void Shutdown();
    bool IsInitialized() const { return initialized; }
    void RefreshSources();
    std::vector<CaptureSourceInfo> GetAvailableSources() const;
    bool StartCapture(const std::string& source_id);
    void StopCapture(const std::string& source_id);
    bool IsCapturing(const std::string& source_id) const;
    std::shared_ptr<CapturedFrame> GetLatestFrame(const std::string& source_id) const;
    void SetDownscaleResolution(int width, int height);
    void GetDownscaleResolution(int& width, int& height) const
    {
        width = target_width;
        height = target_height;
    }
    void SetTargetFPS(int fps);
    int GetTargetFPS() const { return target_fps; }

private:
    ScreenCaptureManager();
    ~ScreenCaptureManager();

    ScreenCaptureManager(const ScreenCaptureManager&) = delete;
    ScreenCaptureManager& operator=(const ScreenCaptureManager&) = delete;

    bool InitializePlatform();
    void ShutdownPlatform();
    void EnumerateSourcesPlatform();
    bool StartCapturePlatform(const std::string& source_id);
    void StopCapturePlatform(const std::string& source_id);
    void CaptureThreadFunction(const std::string& source_id);
    std::atomic<bool>                       initialized;
    std::atomic<int>                        target_width;
    std::atomic<int>                        target_height;
    std::atomic<int>                        target_fps;

    mutable std::mutex                      sources_mutex;
    std::map<std::string, CaptureSourceInfo> sources;

    mutable std::mutex                      frames_mutex;
    std::map<std::string, std::shared_ptr<CapturedFrame>> latest_frames;

    mutable std::mutex                      threads_mutex;
    std::map<std::string, std::thread>      capture_threads;
    std::map<std::string, std::atomic<bool>> capture_active;
};

#endif // SCREENCAPTUREMANAGER_H
