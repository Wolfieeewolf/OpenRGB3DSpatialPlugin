/*---------------------------------------------------------*\
| ScreenCaptureManager.h                                    |
|                                                           |
|   Multi-monitor screen capture manager for ambilight     |
|                                                           |
|   Date: 2025-10-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

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

/**
 * @brief Information about an available capture source (monitor)
 */
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

/**
 * @brief Manages screen capture from multiple monitors
 *
 * Singleton class that handles:
 * - Enumerating available capture sources (monitors)
 * - Capturing frames from each source
 * - Downscaling and format conversion
 * - Thread-safe frame buffer access
 *
 * Platform-specific implementations via compile-time selection.
 */
class ScreenCaptureManager
{
public:
    /**
     * @brief Get the singleton instance
     */
    static ScreenCaptureManager& Instance();

    /**
     * @brief Initialize the capture system
     * @return True if initialization succeeded
     */
    bool Initialize();

    /**
     * @brief Shutdown and release all resources
     */
    void Shutdown();

    /**
     * @brief Check if the manager is initialized
     */
    bool IsInitialized() const { return initialized; }

    /**
     * @brief Re-enumerate capture sources (call if monitors change)
     */
    void RefreshSources();

    /**
     * @brief Get list of all available capture sources
     */
    std::vector<CaptureSourceInfo> GetAvailableSources() const;

    /**
     * @brief Start capturing from a specific source
     * @param source_id The unique ID of the capture source
     * @return True if capture started successfully
     */
    bool StartCapture(const std::string& source_id);

    /**
     * @brief Stop capturing from a specific source
     * @param source_id The unique ID of the capture source
     */
    void StopCapture(const std::string& source_id);

    /**
     * @brief Check if a source is currently being captured
     */
    bool IsCapturing(const std::string& source_id) const;

    /**
     * @brief Get the latest frame from a capture source
     * @param source_id The unique ID of the capture source
     * @return Shared pointer to the latest frame (nullptr if unavailable)
     *
     * Thread-safe. Returns a copy of the latest frame buffer.
     */
    std::shared_ptr<CapturedFrame> GetLatestFrame(const std::string& source_id) const;

    /**
     * @brief Set the target downscale resolution for all captures
     * @param width Target width (default: 480)
     * @param height Target height (default: 270)
     *
     * Applies to new captures. Existing captures need restart to apply.
     */
    void SetDownscaleResolution(int width, int height);

    /**
     * @brief Get current downscale resolution
     */
    void GetDownscaleResolution(int& width, int& height) const
    {
        width = target_width;
        height = target_height;
    }

    /**
     * @brief Set the target capture frame rate
     * @param fps Target FPS (default: 30, max: 60)
     */
    void SetTargetFPS(int fps);

    /**
     * @brief Get current target FPS
     */
    int GetTargetFPS() const { return target_fps; }

private:
    ScreenCaptureManager();
    ~ScreenCaptureManager();

    // Prevent copying
    ScreenCaptureManager(const ScreenCaptureManager&) = delete;
    ScreenCaptureManager& operator=(const ScreenCaptureManager&) = delete;

    /**
     * @brief Platform-specific initialization
     */
    bool InitializePlatform();

    /**
     * @brief Platform-specific shutdown
     */
    void ShutdownPlatform();

    /**
     * @brief Platform-specific source enumeration
     */
    void EnumerateSourcesPlatform();

    /**
     * @brief Platform-specific capture start
     */
    bool StartCapturePlatform(const std::string& source_id);

    /**
     * @brief Platform-specific capture stop
     */
    void StopCapturePlatform(const std::string& source_id);

    /**
     * @brief Capture thread function for a specific source
     */
    void CaptureThreadFunction(const std::string& source_id);

    /*---------------------------------------------------------*\
    | Private data                                             |
    \*---------------------------------------------------------*/
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
