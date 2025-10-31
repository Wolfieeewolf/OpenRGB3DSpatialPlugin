/*---------------------------------------------------------*\
| ScreenCaptureManager.cpp                                  |
|                                                           |
|   Multi-monitor screen capture manager for ambilight     |
|                                                           |
|   Date: 2025-10-23                                        |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "ScreenCaptureManager.h"
#include "LogManager.h"
#include <chrono>
#include <algorithm>
#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QImage>

#ifdef _WIN32
    #include <Windows.h>

    // Forward declaration for Qt internal function
    extern QPixmap qt_pixmapFromWinHBITMAP(HBITMAP bitmap, int format = 0);
#endif

/*---------------------------------------------------------*\
| Helper to generate unique source IDs                     |
\*---------------------------------------------------------*/
static std::string GenerateSourceID(int screen_index)
{
    return "screen_" + std::to_string(screen_index);
}

/*---------------------------------------------------------*\
| Singleton access                                         |
\*---------------------------------------------------------*/
ScreenCaptureManager& ScreenCaptureManager::Instance()
{
    static ScreenCaptureManager instance;
    return instance;
}

/*---------------------------------------------------------*\
| Constructor / Destructor                                 |
\*---------------------------------------------------------*/
ScreenCaptureManager::ScreenCaptureManager()
    : initialized(false)
    , target_width(320)
    , target_height(180)
    , target_fps(30)
{
}

ScreenCaptureManager::~ScreenCaptureManager()
{
    Shutdown();
}

/*---------------------------------------------------------*\
| Initialize                                               |
\*---------------------------------------------------------*/
bool ScreenCaptureManager::Initialize()
{
    if (initialized.load())
    {
        return true;
    }

    if (!InitializePlatform())
    {
        return false;
    }

    EnumerateSourcesPlatform();
    initialized.store(true);
    return true;
}

/*---------------------------------------------------------*\
| Shutdown                                                 |
\*---------------------------------------------------------*/
void ScreenCaptureManager::Shutdown()
{
    if (!initialized.load())
    {
        return;
    }

    // Stop all capture threads
    {
        std::lock_guard<std::mutex> lock(threads_mutex);
        for (std::map<std::string, std::atomic<bool>>::iterator it = capture_active.begin();
             it != capture_active.end();
             ++it)
        {
            it->second.store(false);
        }
    }

    // Wait for threads to finish
    {
        std::lock_guard<std::mutex> lock(threads_mutex);
        for (std::map<std::string, std::thread>::iterator it = capture_threads.begin();
             it != capture_threads.end();
             ++it)
        {
            if (it->second.joinable())
            {
                it->second.join();
            }
        }
        capture_threads.clear();
        capture_active.clear();
    }

    ShutdownPlatform();

    {
        std::lock_guard<std::mutex> lock(sources_mutex);
        sources.clear();
    }

    {
        std::lock_guard<std::mutex> lock(frames_mutex);
        latest_frames.clear();
    }

    initialized.store(false);
}

/*---------------------------------------------------------*\
| Refresh sources                                          |
\*---------------------------------------------------------*/
void ScreenCaptureManager::RefreshSources()
{
    std::lock_guard<std::mutex> lock(sources_mutex);
    sources.clear();
    EnumerateSourcesPlatform();
}

/*---------------------------------------------------------*\
| Get available sources                                    |
\*---------------------------------------------------------*/
std::vector<CaptureSourceInfo> ScreenCaptureManager::GetAvailableSources() const
{
    std::lock_guard<std::mutex> lock(sources_mutex);
    std::vector<CaptureSourceInfo> result;
    result.reserve(sources.size());

    for (std::map<std::string, CaptureSourceInfo>::const_iterator it = sources.begin();
         it != sources.end();
         ++it)
    {
        result.push_back(it->second);
    }

    return result;
}

/*---------------------------------------------------------*\
| Start capture                                            |
\*---------------------------------------------------------*/
bool ScreenCaptureManager::StartCapture(const std::string& source_id)
{
    // Check if source exists
    {
        std::lock_guard<std::mutex> lock(sources_mutex);
        if (sources.find(source_id) == sources.end())
        {
            return false;
        }
    }

    // Check if already capturing
    {
        std::lock_guard<std::mutex> lock(threads_mutex);
        if (capture_active.find(source_id) != capture_active.end() &&
            capture_active[source_id].load())
        {
            return true; // Already running
        }
    }

    // Start platform-specific capture
    if (!StartCapturePlatform(source_id))
    {
        LOG_WARNING("[ScreenCapture] StartCapturePlatform failed for '%s'", source_id.c_str());
        return false;
    }

    // Launch capture thread
    {
        std::lock_guard<std::mutex> lock(threads_mutex);
        capture_active[source_id].store(true);
        capture_threads[source_id] = std::thread(&ScreenCaptureManager::CaptureThreadFunction, this, source_id);
    }

    return true;
}

/*---------------------------------------------------------*\
| Stop capture                                             |
\*---------------------------------------------------------*/
void ScreenCaptureManager::StopCapture(const std::string& source_id)
{
    // Signal thread to stop
    {
        std::lock_guard<std::mutex> lock(threads_mutex);
        std::map<std::string, std::atomic<bool>>::iterator active_it = capture_active.find(source_id);
        if (active_it != capture_active.end())
        {
            active_it->second.store(false);
        }
    }

    // Wait for thread to finish
    {
        std::lock_guard<std::mutex> lock(threads_mutex);
        std::map<std::string, std::thread>::iterator thread_it = capture_threads.find(source_id);
        if (thread_it != capture_threads.end() && thread_it->second.joinable())
        {
            thread_it->second.join();
            capture_threads.erase(thread_it);
        }
        capture_active.erase(source_id);
    }

    StopCapturePlatform(source_id);
}

/*---------------------------------------------------------*\
| Check if capturing                                       |
\*---------------------------------------------------------*/
bool ScreenCaptureManager::IsCapturing(const std::string& source_id) const
{
    std::lock_guard<std::mutex> lock(threads_mutex);
    std::map<std::string, std::atomic<bool>>::const_iterator it = capture_active.find(source_id);
    return (it != capture_active.end() && it->second.load());
}

/*---------------------------------------------------------*\
| Get latest frame                                         |
\*---------------------------------------------------------*/
std::shared_ptr<CapturedFrame> ScreenCaptureManager::GetLatestFrame(const std::string& source_id) const
{
    std::lock_guard<std::mutex> lock(frames_mutex);
    std::map<std::string, std::shared_ptr<CapturedFrame>>::const_iterator it = latest_frames.find(source_id);
    if (it != latest_frames.end())
    {
        return it->second;
    }
    return nullptr;
}

/*---------------------------------------------------------*\
| Set downscale resolution                                 |
\*---------------------------------------------------------*/
void ScreenCaptureManager::SetDownscaleResolution(int width, int height)
{
    target_width.store((std::max)(32, (std::min)(width, 3840)));
    target_height.store((std::max)(32, (std::min)(height, 2160)));
}

/*---------------------------------------------------------*\
| Set target FPS                                           |
\*---------------------------------------------------------*/
void ScreenCaptureManager::SetTargetFPS(int fps)
{
    target_fps.store((std::max)(1, (std::min)(fps, 60)));
}

/*---------------------------------------------------------*\
| Platform-specific implementations - Windows              |
\*---------------------------------------------------------*/
#ifdef _WIN32

bool ScreenCaptureManager::InitializePlatform()
{
    // Platform data will be created per-capture-source
    // Nothing to initialize globally for DXGI
    return true;
}

void ScreenCaptureManager::ShutdownPlatform()
{
    // Per-source cleanup handled in StopCapturePlatform
}

void ScreenCaptureManager::EnumerateSourcesPlatform()
{
    QList<QScreen*> screens = QGuiApplication::screens();

    for (int i = 0; i < screens.size(); i++)
    {
        QScreen* screen = screens[i];
        if (!screen) continue;

        CaptureSourceInfo info;
        info.id = GenerateSourceID(i);
        info.name = screen->name().toStdString();
        info.device_name = screen->model().toStdString();

        QRect geometry = screen->geometry();
        info.width = geometry.width();
        info.height = geometry.height();
        info.x = geometry.x();
        info.y = geometry.y();

        // Primary screen is at the top-left of the virtual desktop (usually 0,0)
        info.is_primary = (screen == QGuiApplication::primaryScreen());
        info.is_available = true;

        sources[info.id] = info;
    }
}

bool ScreenCaptureManager::StartCapturePlatform(const std::string& source_id)
{
    // Extract screen index from source_id (format: "screen_N")
    size_t underscore_pos = source_id.find('_');
    if (underscore_pos == std::string::npos)
    {
        LOG_WARNING("[ScreenCapture] Invalid source_id format in StartCapturePlatform: '%s'", source_id.c_str());
        return false;
    }

    int screen_index = std::stoi(source_id.substr(underscore_pos + 1));

    QList<QScreen*> screens = QGuiApplication::screens();
    if (screen_index < 0 || screen_index >= screens.size())
    {
        LOG_WARNING("[ScreenCapture] Invalid screen index %d (total screens: %d)", screen_index, screens.size());
        return false;
    }

    // No platform-specific initialization needed - will be done in capture thread
    return true;
}

void ScreenCaptureManager::StopCapturePlatform(const std::string& /*source_id*/)
{
    // Cleanup will be done in capture thread when it exits
    // Platform-specific resources are managed per-thread
}

/*---------------------------------------------------------*\
| Helper function to capture screen using GDI BitBlt       |
\*---------------------------------------------------------*/
static QPixmap GrabScreen(QScreen* screen)
{
    if (!screen) return QPixmap();

    const QRect screen_geometry = screen->geometry();
    int width = screen_geometry.width();
    int height = screen_geometry.height();
    int xIn = screen_geometry.x();
    int yIn = screen_geometry.y();

    // Get desktop window
    HWND hwnd = GetDesktopWindow();

    // Create and setup bitmap
    HDC display_dc = GetDC(nullptr);
    HDC bitmap_dc = CreateCompatibleDC(display_dc);
    HBITMAP bitmap = CreateCompatibleBitmap(display_dc, width, height);
    HGDIOBJ null_bitmap = SelectObject(bitmap_dc, bitmap);

    // Copy screen data using BitBlt
    HDC window_dc = GetDC(hwnd);
    BitBlt(bitmap_dc, 0, 0, width, height, window_dc, xIn, yIn, SRCCOPY);

    // Clean up all but bitmap
    ReleaseDC(hwnd, window_dc);
    SelectObject(bitmap_dc, null_bitmap);
    DeleteDC(bitmap_dc);

    // Convert to QPixmap
    const QPixmap pixmap = qt_pixmapFromWinHBITMAP(bitmap);
    DeleteObject(bitmap);
    ReleaseDC(nullptr, display_dc);

    return pixmap;
}

void ScreenCaptureManager::CaptureThreadFunction(const std::string& source_id)
{
    // Extract screen index from source_id (format: "screen_N")
    size_t underscore_pos = source_id.find('_');
    if (underscore_pos == std::string::npos)
    {
        LOG_WARNING("[ScreenCapture] Invalid source_id format: '%s'", source_id.c_str());
        return;
    }

    int screen_index = std::stoi(source_id.substr(underscore_pos + 1));

    // Get the QScreen object
    QList<QScreen*> screens = QGuiApplication::screens();
    if (screen_index < 0 || screen_index >= screens.size())
    {
        LOG_WARNING("[ScreenCapture] Invalid screen index %d", screen_index);
        return;
    }

    QScreen* screen = screens[screen_index];
    if (!screen)
    {
        LOG_WARNING("[ScreenCapture] Screen %d is null", screen_index);
        return;
    }

    // Capture loop
    uint64_t frame_counter = 0;
    const int target_frame_time_ms = 1000 / target_fps.load();

    while (capture_active[source_id].load())
    {
        std::chrono::steady_clock::time_point frame_start = std::chrono::steady_clock::now();

        // Check if screen is still valid
        if (!screen)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // Capture the screen using GDI BitBlt
        QPixmap pixmap = GrabScreen(screen);
        if (pixmap.isNull())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // Convert to QImage and downscale
        QImage image = pixmap.toImage();
        int target_w = target_width.load();
        int target_h = target_height.load();

        // Downscale if needed
        if (image.width() != target_w || image.height() != target_h)
        {
            image = image.scaled(target_w, target_h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }

        // Convert to RGBA format
        image = image.convertToFormat(QImage::Format_RGBA8888);

        // Create frame buffer
        std::shared_ptr<CapturedFrame> frame = std::make_shared<CapturedFrame>();
        frame->width = image.width();
        frame->height = image.height();
        frame->data.resize(frame->width * frame->height * 4);

        // Copy pixel data
        memcpy(frame->data.data(), image.constBits(), frame->data.size());

        frame->frame_id = frame_counter++;
        frame->timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        frame->valid = true;

        // Store frame in buffer
        {
            std::lock_guard<std::mutex> lock(frames_mutex);
            latest_frames[source_id] = frame;
        }

        // Frame rate limiting
        std::chrono::steady_clock::time_point frame_end = std::chrono::steady_clock::now();
        int elapsed = (int)std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start).count();
        int sleep_time = target_frame_time_ms - elapsed;

        // Sleep at least 2ms to prevent CPU spinning
        if (sleep_time < 2)
        {
            sleep_time = 2;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
    }

}

#else
/*---------------------------------------------------------*\
| Platform-specific implementations - Linux / Other        |
\*---------------------------------------------------------*/

bool ScreenCaptureManager::InitializePlatform()
{
    // Linux capture is not implemented yet
    return false;
}

void ScreenCaptureManager::ShutdownPlatform()
{
}

void ScreenCaptureManager::EnumerateSourcesPlatform()
{
}

bool ScreenCaptureManager::StartCapturePlatform(const std::string& source_id)
{
    return false;
}

void ScreenCaptureManager::StopCapturePlatform(const std::string& source_id)
{
}

void ScreenCaptureManager::CaptureThreadFunction(const std::string& source_id)
{
}

#endif
