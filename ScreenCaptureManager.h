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
    std::string     id;                 // Unique identifier for this monitor
    std::string     name;               // Display name (e.g., "\\.\DISPLAY1")
    std::string     device_name;        // Friendly name if available
    int             width;              // Native resolution width
    int             height;             // Native resolution height
    int             x;                  // Position in virtual screen space
    int             y;                  // Position in virtual screen space
    bool            is_primary;         // Is this the primary display?
    bool            is_available;       // Can we capture from this source?
};

/**
 * @brief A single captured frame with metadata
 */
struct CapturedFrame
{
    std::vector<uint8_t>    data;           // RGBA pixel data (downscaled)
    int                     width;          // Frame width
    int                     height;         // Frame height
    uint64_t                frame_id;       // Monotonic frame counter
    uint64_t                timestamp_ms;   // Capture timestamp
    bool                    valid;          // Is this frame valid?

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

    // Prevent copying
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
