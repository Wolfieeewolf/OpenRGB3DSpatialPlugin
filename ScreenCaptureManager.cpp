// SPDX-License-Identifier: GPL-2.0-only

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
    #include <d3d11.h>
    #include <dxgi1_2.h>

    #pragma comment(lib, "d3d11.lib")
    #pragma comment(lib, "dxgi.lib")
    extern QPixmap qt_pixmapFromWinHBITMAP(HBITMAP bitmap, int format = 0);
#endif

ScreenCaptureManager& ScreenCaptureManager::Instance()
{
    static ScreenCaptureManager instance;
    return instance;
}

ScreenCaptureManager::ScreenCaptureManager()
    : initialized(false)
    , target_width(480)
    , target_height(270)
    , target_fps(30)
{
}

ScreenCaptureManager::~ScreenCaptureManager()
{
    Shutdown();
}

bool ScreenCaptureManager::Initialize()
{
    if(initialized.load())
    {
        return true;
    }

    if(!InitializePlatform())
    {
        return false;
    }

    EnumerateSourcesPlatform();
    initialized.store(true);
    return true;
}

void ScreenCaptureManager::Shutdown()
{
    if(!initialized.load())
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(threads_mutex);
        for(std::map<std::string, std::atomic<bool>>::iterator it = capture_active.begin();
             it != capture_active.end();
             ++it)
        {
            it->second.store(false);
        }
    }

    {
        std::lock_guard<std::mutex> lock(threads_mutex);
        for(std::map<std::string, std::thread>::iterator it = capture_threads.begin();
             it != capture_threads.end();
             ++it)
        {
            if(it->second.joinable())
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

void ScreenCaptureManager::RefreshSources()
{
    std::lock_guard<std::mutex> lock(sources_mutex);
    sources.clear();
    EnumerateSourcesPlatform();
}

std::vector<CaptureSourceInfo> ScreenCaptureManager::GetAvailableSources() const
{
    std::lock_guard<std::mutex> lock(sources_mutex);
    std::vector<CaptureSourceInfo> result;
    result.reserve(sources.size());

    for(std::map<std::string, CaptureSourceInfo>::const_iterator it = sources.begin();
         it != sources.end();
         ++it)
    {
        result.push_back(it->second);
    }

    return result;
}

bool ScreenCaptureManager::StartCapture(const std::string& source_id)
{
    {
        std::lock_guard<std::mutex> lock(sources_mutex);
        if(sources.find(source_id) == sources.end())
        {
            return false;
        }
    }

    {
        std::lock_guard<std::mutex> lock(threads_mutex);
        if(capture_active.find(source_id) != capture_active.end() &&
            capture_active[source_id].load())
        {
            return true;
        }
    }

    if(!StartCapturePlatform(source_id))
    {
        LOG_WARNING("[ScreenCapture] StartCapturePlatform failed for '%s'", source_id.c_str());
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(threads_mutex);
        capture_active[source_id].store(true);
        capture_threads[source_id] = std::thread(&ScreenCaptureManager::CaptureThreadFunction, this, source_id);
    }

    return true;
}

void ScreenCaptureManager::StopCapture(const std::string& source_id)
{
    {
        std::lock_guard<std::mutex> lock(threads_mutex);
        std::map<std::string, std::atomic<bool>>::iterator active_it = capture_active.find(source_id);
        if(active_it != capture_active.end())
        {
            active_it->second.store(false);
        }
    }

    {
        std::lock_guard<std::mutex> lock(threads_mutex);
        std::map<std::string, std::thread>::iterator thread_it = capture_threads.find(source_id);
        if(thread_it != capture_threads.end() && thread_it->second.joinable())
        {
            thread_it->second.join();
            capture_threads.erase(thread_it);
        }
        capture_active.erase(source_id);
    }

    StopCapturePlatform(source_id);
}

bool ScreenCaptureManager::IsCapturing(const std::string& source_id) const
{
    std::lock_guard<std::mutex> lock(threads_mutex);
    std::map<std::string, std::atomic<bool>>::const_iterator it = capture_active.find(source_id);
    return (it != capture_active.end() && it->second.load());
}

std::shared_ptr<CapturedFrame> ScreenCaptureManager::GetLatestFrame(const std::string& source_id) const
{
    std::lock_guard<std::mutex> lock(frames_mutex);
    std::map<std::string, std::shared_ptr<CapturedFrame>>::const_iterator it = latest_frames.find(source_id);
    if(it != latest_frames.end())
    {
        return it->second;
    }
    return nullptr;
}

void ScreenCaptureManager::SetDownscaleResolution(int width, int height)
{
    target_width.store((std::max)(32, (std::min)(width, 3840)));
    target_height.store((std::max)(32, (std::min)(height, 2160)));
}

void ScreenCaptureManager::SetTargetFPS(int fps)
{
    target_fps.store((std::max)(1, (std::min)(fps, 60)));
}

#ifdef _WIN32

bool ScreenCaptureManager::InitializePlatform()
{
    return true;
}

void ScreenCaptureManager::ShutdownPlatform()
{
}

void ScreenCaptureManager::EnumerateSourcesPlatform()
{
    QList<QScreen*> screens = QGuiApplication::screens();

    for(int i = 0; i < screens.size(); i++)
    {
        QScreen* screen = screens[i];
        if(!screen) continue;

        CaptureSourceInfo info;
        info.id = "screen_" + std::to_string(i);
        info.name = screen->name().toStdString();
        info.device_name = screen->model().toStdString();

        QRect geometry = screen->geometry();
        info.width = geometry.width();
        info.height = geometry.height();
        info.x = geometry.x();
        info.y = geometry.y();

        info.is_primary = (screen == QGuiApplication::primaryScreen());
        info.is_available = true;

        sources[info.id] = info;
    }
}

bool ScreenCaptureManager::StartCapturePlatform(const std::string& source_id)
{
    size_t underscore_pos = source_id.find('_');
    if(underscore_pos == std::string::npos)
    {
        LOG_WARNING("[ScreenCapture] Invalid source_id format in StartCapturePlatform: '%s'", source_id.c_str());
        return false;
    }

    int screen_index = std::stoi(source_id.substr(underscore_pos + 1));

    QList<QScreen*> screens = QGuiApplication::screens();
    if(screen_index < 0 || screen_index >= screens.size())
    {
        LOG_WARNING("[ScreenCapture] Invalid screen index %d (total screens: %d)", screen_index, screens.size());
        return false;
    }

    return true;
}

void ScreenCaptureManager::StopCapturePlatform(const std::string& /*source_id*/)
{
}

static QPixmap GrabScreen(QScreen* screen)
{
    if(!screen) return QPixmap();

    const QRect screen_geometry = screen->geometry();
    int width = screen_geometry.width();
    int height = screen_geometry.height();
    int xIn = screen_geometry.x();
    int yIn = screen_geometry.y();

    HWND hwnd = GetDesktopWindow();
    HDC display_dc = GetDC(nullptr);
    HDC bitmap_dc = CreateCompatibleDC(display_dc);
    HBITMAP bitmap = CreateCompatibleBitmap(display_dc, width, height);
    HGDIOBJ null_bitmap = SelectObject(bitmap_dc, bitmap);

    HDC window_dc = GetDC(hwnd);
    BitBlt(bitmap_dc, 0, 0, width, height, window_dc, xIn, yIn, SRCCOPY);

    ReleaseDC(hwnd, window_dc);
    SelectObject(bitmap_dc, null_bitmap);
    DeleteDC(bitmap_dc);

    const QPixmap pixmap = qt_pixmapFromWinHBITMAP(bitmap);
    DeleteObject(bitmap);
    ReleaseDC(nullptr, display_dc);

    return pixmap;
}

struct DXGICaptureState
{
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGIOutputDuplication* duplication = nullptr;
    ID3D11Texture2D* staging_texture = nullptr;
    UINT width = 0;
    UINT height = 0;

    void Release()
    {
        if(staging_texture) { staging_texture->Release(); staging_texture = nullptr; }
        if(duplication) { duplication->Release(); duplication = nullptr; }
        if(context) { context->Release(); context = nullptr; }
        if(device) { device->Release(); device = nullptr; }
        width = 0;
        height = 0;
    }

    bool IsValid() const { return device && context && duplication && staging_texture && width > 0 && height > 0; }
};

static bool TryCreateDXGIDuplication(int screen_x, int screen_y, int screen_width, int screen_height, DXGICaptureState& out)
{
    out.Release();
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL feature_level;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &device, &feature_level, &context);
    if(FAILED(hr) || !device || !context)
        return false;

    IDXGIDevice* dxgi_device = nullptr;
    hr = device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgi_device);
    if(FAILED(hr) || !dxgi_device)
    {
        context->Release();
        device->Release();
        return false;
    }

    IDXGIAdapter* adapter = nullptr;
    hr = dxgi_device->GetAdapter(&adapter);
    dxgi_device->Release();
    if(FAILED(hr) || !adapter)
    {
        context->Release();
        device->Release();
        return false;
    }

    IDXGIOutput* output = nullptr;
    IDXGIOutput1* output1 = nullptr;
    IDXGIOutputDuplication* duplication = nullptr;
    ID3D11Texture2D* staging_texture = nullptr;
    UINT out_width = 0, out_height = 0;

    for(UINT i = 0; adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND; i++)
    {
        if(!output) continue;
        DXGI_OUTPUT_DESC desc;
        if(FAILED(output->GetDesc(&desc)))
        {
            output->Release();
            continue;
        }
        RECT& r = desc.DesktopCoordinates;
        int left = r.left, top = r.top, right = r.right, bottom = r.bottom;
        if(left == screen_x && top == screen_y &&
            (right - left) == screen_width && (bottom - top) == screen_height)
        {
            hr = output->QueryInterface(__uuidof(IDXGIOutput1), (void**)&output1);
            output->Release();
            if(FAILED(hr) || !output1) break;

            hr = output1->DuplicateOutput(device, &duplication);
            output1->Release();
            if(FAILED(hr) || !duplication)
            {
                context->Release();
                device->Release();
                return false;
            }

            DXGI_OUTDUPL_DESC dup_desc;
            duplication->GetDesc(&dup_desc);
            out_width = dup_desc.ModeDesc.Width;
            out_height = dup_desc.ModeDesc.Height;

            D3D11_TEXTURE2D_DESC tex_desc = {};
            tex_desc.Width = out_width;
            tex_desc.Height = out_height;
            tex_desc.MipLevels = 1;
            tex_desc.ArraySize = 1;
            tex_desc.Format = dup_desc.ModeDesc.Format;
            tex_desc.SampleDesc.Count = 1;
            tex_desc.SampleDesc.Quality = 0;
            tex_desc.Usage = D3D11_USAGE_STAGING;
            tex_desc.BindFlags = 0;
            tex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            tex_desc.MiscFlags = 0;
            hr = device->CreateTexture2D(&tex_desc, nullptr, &staging_texture);
            if(FAILED(hr) || !staging_texture)
            {
                duplication->Release();
                context->Release();
                device->Release();
                return false;
            }
            out.device = device;
            out.context = context;
            out.duplication = duplication;
            out.staging_texture = staging_texture;
            out.width = out_width;
            out.height = out_height;
            adapter->Release();
            return true;
        }
        output->Release();
    }
    adapter->Release();
    context->Release();
    device->Release();
    return false;
}

void ScreenCaptureManager::CaptureThreadFunction(const std::string& source_id)
{
    std::atomic<bool>* active_flag = nullptr;
    {
        std::lock_guard<std::mutex> lock(threads_mutex);
        std::map<std::string, std::atomic<bool>>::iterator active_it = capture_active.find(source_id);
        if(active_it == capture_active.end())
        {
            LOG_WARNING("[ScreenCapture] Active flag not found for '%s'", source_id.c_str());
            return;
        }
        active_flag = &(active_it->second);
    }

    size_t underscore_pos = source_id.find('_');
    if(underscore_pos == std::string::npos)
    {
        LOG_WARNING("[ScreenCapture] Invalid source_id format: '%s'", source_id.c_str());
        return;
    }
    int screen_index = std::stoi(source_id.substr(underscore_pos + 1));

    QList<QScreen*> screens = QGuiApplication::screens();
    QScreen* screen = (screen_index >= 0 && screen_index < screens.size()) ? screens[screen_index] : nullptr;
    if(!screen)
    {
        LOG_WARNING("[ScreenCapture] Screen %d unavailable", screen_index);
        return;
    }

    QRect geometry = screen->geometry();
    DXGICaptureState dxgi_state;
    bool use_dxgi = TryCreateDXGIDuplication(geometry.x(), geometry.y(), geometry.width(), geometry.height(), dxgi_state);
    if(!use_dxgi)
    {
        LOG_INFO("[ScreenCapture] Using GDI for screen %d (DXGI unavailable or failed)", screen_index);
    }

    uint64_t frame_counter = 0;
    const int target_frame_time_ms = 1000 / target_fps.load();
    bool logged_screen_unavailable = false;
    std::vector<uint8_t> dxgi_rgba_buffer;
    std::chrono::steady_clock::time_point last_dxgi_retry = std::chrono::steady_clock::now();
    const std::chrono::seconds dxgi_retry_interval(5);

    while(active_flag->load())
    {
        std::chrono::steady_clock::time_point frame_start = std::chrono::steady_clock::now();

        screen = (screen_index >= 0 && screen_index < screens.size()) ? screens[screen_index] : nullptr;
        if(!screen)
        {
            if(!logged_screen_unavailable)
            {
                LOG_WARNING("[ScreenCapture] Screen %d unavailable, waiting for it to return", screen_index);
                logged_screen_unavailable = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        if(logged_screen_unavailable)
            logged_screen_unavailable = false;

        QImage image;
        if(use_dxgi && dxgi_state.IsValid())
        {
            DXGI_OUTDUPL_FRAME_INFO frame_info;
            IDXGIResource* resource = nullptr;
            HRESULT hr = dxgi_state.duplication->AcquireNextFrame(8, &frame_info, &resource);
            if(hr == DXGI_ERROR_WAIT_TIMEOUT)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(target_frame_time_ms > 0 ? target_frame_time_ms : 2));
                continue;
            }
            if(hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_ACCESS_DENIED || hr == DXGI_ERROR_DEVICE_REMOVED)
            {
                dxgi_state.Release();
                use_dxgi = false;
                last_dxgi_retry = std::chrono::steady_clock::now();
                image = GrabScreen(screen).toImage();
            }
            else if(SUCCEEDED(hr) && resource)
            {
                ID3D11Texture2D* tex = nullptr;
                hr = resource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex);
                resource->Release();
                if(SUCCEEDED(hr) && tex)
                {
                    dxgi_state.context->CopyResource(dxgi_state.staging_texture, tex);
                    tex->Release();

                    D3D11_MAPPED_SUBRESOURCE mapped;
                    hr = dxgi_state.context->Map(dxgi_state.staging_texture, 0, D3D11_MAP_READ, 0, &mapped);
                    if(SUCCEEDED(hr))
                    {
                        size_t need = (size_t)dxgi_state.width * dxgi_state.height * 4;
                        if(dxgi_rgba_buffer.size() < need)
                            dxgi_rgba_buffer.resize(need);
                        const uint8_t* src = (const uint8_t*)mapped.pData;
                        uint8_t* dst = dxgi_rgba_buffer.data();
                        const UINT row_pitch_skip = mapped.RowPitch - dxgi_state.width * 4;
                        for(UINT y = 0; y < dxgi_state.height; y++)
                        {
                            for(UINT x = 0; x < dxgi_state.width; x++)
                            {
                                dst[0] = src[2];
                                dst[1] = src[1];
                                dst[2] = src[0];
                                dst[3] = src[3];
                                src += 4;
                                dst += 4;
                            }
                            src += row_pitch_skip;
                        }
                        dxgi_state.context->Unmap(dxgi_state.staging_texture, 0);
                        image = QImage(dxgi_rgba_buffer.data(), (int)dxgi_state.width, (int)dxgi_state.height, (int)dxgi_state.width * 4, QImage::Format_RGBA8888).copy();
                    }
                    dxgi_state.duplication->ReleaseFrame();
                }
                else
                {
                    dxgi_state.duplication->ReleaseFrame();
                }
            }
            if(image.isNull() && use_dxgi)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
        }
        if(!use_dxgi || image.isNull())
        {
            if(!use_dxgi)
            {
                std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
                if(now - last_dxgi_retry >= dxgi_retry_interval)
                {
                    last_dxgi_retry = now;
                    QRect geo = screen->geometry();
                    if(TryCreateDXGIDuplication(geo.x(), geo.y(), geo.width(), geo.height(), dxgi_state))
                    {
                        use_dxgi = true;
                    }
                }
            }
            if(!use_dxgi || image.isNull())
            {
                QPixmap pixmap = GrabScreen(screen);
                if(pixmap.isNull())
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                image = pixmap.toImage();
            }
        }

        int target_w = target_width.load();
        int target_h = target_height.load();
        if(image.width() != target_w || image.height() != target_h)
            image = image.scaled(target_w, target_h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        image = image.convertToFormat(QImage::Format_RGBA8888);

        std::shared_ptr<CapturedFrame> frame = std::make_shared<CapturedFrame>();
        frame->width = image.width();
        frame->height = image.height();
        frame->data.resize(frame->width * frame->height * 4);
        const int line_bytes = frame->width * 4;
        const int src_stride = image.bytesPerLine();
        const uint8_t* src = image.constBits();
        uint8_t* dst = frame->data.data();
        for(int y = 0; y < frame->height; y++)
        {
            memcpy(dst, src, (size_t)line_bytes);
            dst += line_bytes;
            src += src_stride;
        }
        frame->frame_id = frame_counter++;
        frame->timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        frame->valid = true;

        {
            std::lock_guard<std::mutex> lock(frames_mutex);
            latest_frames[source_id] = frame;
        }

        std::chrono::steady_clock::time_point frame_end = std::chrono::steady_clock::now();
        int elapsed = (int)std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start).count();
        int sleep_time = target_frame_time_ms - elapsed;
        if(sleep_time < 2)
            sleep_time = 2;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
    }

    dxgi_state.Release();
}

#else

bool ScreenCaptureManager::InitializePlatform()
{
    return true;
}

void ScreenCaptureManager::ShutdownPlatform()
{
}

void ScreenCaptureManager::EnumerateSourcesPlatform()
{
    QList<QScreen*> screens = QGuiApplication::screens();
    for(int i = 0; i < screens.size(); i++)
    {
        QScreen* screen = screens[i];
        if(!screen) continue;

        CaptureSourceInfo info;
        info.id = "screen_" + std::to_string(i);
        info.name = screen->name().toStdString();
        info.device_name = screen->model().toStdString();

        QRect geometry = screen->geometry();
        info.width = geometry.width();
        info.height = geometry.height();
        info.x = geometry.x();
        info.y = geometry.y();
        info.is_primary = (screen == QGuiApplication::primaryScreen());
        info.is_available = true;

        sources[info.id] = info;
    }
}

bool ScreenCaptureManager::StartCapturePlatform(const std::string& source_id)
{
    size_t underscore_pos = source_id.find('_');
    if(underscore_pos == std::string::npos)
    {
        LOG_WARNING("[ScreenCapture] Invalid source_id format in StartCapturePlatform: '%s'", source_id.c_str());
        return false;
    }
    int screen_index = std::stoi(source_id.substr(underscore_pos + 1));
    QList<QScreen*> screens = QGuiApplication::screens();
    if(screen_index < 0 || screen_index >= screens.size())
    {
        LOG_WARNING("[ScreenCapture] Invalid screen index %d (total screens: %d)", screen_index, screens.size());
        return false;
    }
    return true;
}

void ScreenCaptureManager::StopCapturePlatform(const std::string& /*source_id*/)
{
}

static QPixmap GrabScreenLinux(QScreen* screen)
{
    if(!screen) return QPixmap();
    QRect geometry = screen->geometry();
    return screen->grabWindow(0, geometry.x(), geometry.y(), geometry.width(), geometry.height());
}

void ScreenCaptureManager::CaptureThreadFunction(const std::string& source_id)
{
    std::atomic<bool>* active_flag = nullptr;
    {
        std::lock_guard<std::mutex> lock(threads_mutex);
        std::map<std::string, std::atomic<bool>>::iterator active_it = capture_active.find(source_id);
        if(active_it == capture_active.end())
        {
            LOG_WARNING("[ScreenCapture] Active flag not found for '%s'", source_id.c_str());
            return;
        }
        active_flag = &(active_it->second);
    }

    size_t underscore_pos = source_id.find('_');
    if(underscore_pos == std::string::npos)
    {
        LOG_WARNING("[ScreenCapture] Invalid source_id format: '%s'", source_id.c_str());
        return;
    }
    int screen_index = std::stoi(source_id.substr(underscore_pos + 1));

    uint64_t frame_counter = 0;
    const int target_frame_time_ms = 1000 / target_fps.load();
    bool logged_screen_unavailable = false;

    while(active_flag->load())
    {
        std::chrono::steady_clock::time_point frame_start = std::chrono::steady_clock::now();

        QList<QScreen*> screens = QGuiApplication::screens();
        QScreen* screen = nullptr;
        if(screen_index >= 0 && screen_index < screens.size())
        {
            screen = screens[screen_index];
        }

        if(!screen)
        {
            if(!logged_screen_unavailable)
            {
                LOG_WARNING("[ScreenCapture] Screen %d unavailable, waiting for it to return", screen_index);
                logged_screen_unavailable = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        if(logged_screen_unavailable)
        {
            logged_screen_unavailable = false;
        }

        QPixmap pixmap = GrabScreenLinux(screen);
        if(pixmap.isNull())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        QImage image = pixmap.toImage();
        int target_w = target_width.load();
        int target_h = target_height.load();
        if(image.width() != target_w || image.height() != target_h)
        {
            image = image.scaled(target_w, target_h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        image = image.convertToFormat(QImage::Format_RGBA8888);

        std::shared_ptr<CapturedFrame> frame = std::make_shared<CapturedFrame>();
        frame->width = image.width();
        frame->height = image.height();
        frame->data.resize(frame->width * frame->height * 4);
        const int line_bytes = frame->width * 4;
        const int src_stride = image.bytesPerLine();
        const uint8_t* src = image.constBits();
        uint8_t* dst = frame->data.data();
        for(int y = 0; y < frame->height; y++)
        {
            memcpy(dst, src, (size_t)line_bytes);
            dst += line_bytes;
            src += src_stride;
        }
        frame->frame_id = frame_counter++;
        frame->timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        frame->valid = true;

        {
            std::lock_guard<std::mutex> lock(frames_mutex);
            latest_frames[source_id] = frame;
        }

        std::chrono::steady_clock::time_point frame_end = std::chrono::steady_clock::now();
        int elapsed = (int)std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start).count();
        int sleep_time = target_frame_time_ms - elapsed;
        if(sleep_time < 2)
        {
            sleep_time = 2;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
    }
}

#endif
