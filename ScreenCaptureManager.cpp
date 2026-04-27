// SPDX-License-Identifier: GPL-2.0-only

#include "ScreenCaptureManager.h"
#include "LogManager.h"
#include <chrono>
#include <algorithm>
#include <set>
#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QImage>

#ifdef _WIN32
    #include <Windows.h>
    #include <d3d11.h>
    #include <dxgi1_2.h>
    #include <cmath>

    #pragma comment(lib, "d3d11.lib")
    #pragma comment(lib, "dxgi.lib")
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

    std::map<std::string, std::thread> threads_to_join;
    {
        std::lock_guard<std::mutex> lock(threads_mutex);
        threads_to_join.swap(capture_threads);
    }
    for(std::map<std::string, std::thread>::iterator it = threads_to_join.begin();
        it != threads_to_join.end();
        ++it)
    {
        if(it->second.joinable())
        {
            it->second.join();
        }
    }
    {
        std::lock_guard<std::mutex> lock(threads_mutex);
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
            static std::set<std::string> warned_unknown_sources;
            if(warned_unknown_sources.insert(source_id).second)
            {
                std::string available;
                for(const auto& kv : sources)
                {
                    if(!available.empty()) available += ", ";
                    available += kv.first;
                }
                LOG_WARNING("[ScreenCapture] StartCapture rejected unknown source '%s' (available: [%s])",
                            source_id.c_str(), available.c_str());
            }
            return false;
        }
    }

    std::thread stale_thread;
    {
        std::lock_guard<std::mutex> lock(threads_mutex);
        std::map<std::string, std::atomic<bool>>::iterator active_it = capture_active.find(source_id);
        if(active_it != capture_active.end() && active_it->second.load())
        {
            return true;
        }
        std::map<std::string, std::thread>::iterator thread_it = capture_threads.find(source_id);
        if(thread_it != capture_threads.end())
        {
            stale_thread = std::move(thread_it->second);
            capture_threads.erase(thread_it);
        }
        if(active_it != capture_active.end())
        {
            capture_active.erase(active_it);
        }
    }
    if(stale_thread.joinable())
    {
        stale_thread.join();
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

    std::thread thread_to_join;
    {
        std::lock_guard<std::mutex> lock(threads_mutex);
        std::map<std::string, std::thread>::iterator thread_it = capture_threads.find(source_id);
        if(thread_it != capture_threads.end())
        {
            thread_to_join = std::move(thread_it->second);
            capture_threads.erase(thread_it);
        }
    }
    if(thread_to_join.joinable())
    {
        thread_to_join.join();
    }

    {
        std::lock_guard<std::mutex> lock(threads_mutex);
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
    LOG_INFO("[ScreenCapture] Enumerating %d screen(s)", screens.size());

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
        LOG_INFO("[ScreenCapture]   id='%s' name='%s' geom=%dx%d at (%d,%d) primary=%d dpr=%.2f",
                 info.id.c_str(), info.name.c_str(),
                 info.width, info.height, info.x, info.y,
                 (int)info.is_primary, screen->devicePixelRatio());
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

    int screen_index = -1;
    try
    {
        screen_index = std::stoi(source_id.substr(underscore_pos + 1));
    }
    catch(...)
    {
        LOG_WARNING("[ScreenCapture] Invalid source index in StartCapturePlatform: '%s'", source_id.c_str());
        return false;
    }

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

struct DXGICaptureState
{
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGIOutputDuplication* duplication = nullptr;
    ID3D11Texture2D* staging_texture = nullptr;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT width = 0;
    UINT height = 0;
    bool desktop_frame_acquired = false;

    void Release()
    {
        if(duplication && desktop_frame_acquired)
        {
            duplication->ReleaseFrame();
            desktop_frame_acquired = false;
        }
        if(staging_texture) { staging_texture->Release(); staging_texture = nullptr; }
        if(duplication) { duplication->Release(); duplication = nullptr; }
        if(context) { context->Release(); context = nullptr; }
        if(device) { device->Release(); device = nullptr; }
        format = DXGI_FORMAT_UNKNOWN;
        width = 0;
        height = 0;
    }

    bool IsValid() const { return device && context && duplication && staging_texture && width > 0 && height > 0; }
};

static float HalfToFloat(uint16_t value)
{
    const uint16_t sign = (value >> 15) & 0x1;
    const uint16_t exponent = (value >> 10) & 0x1F;
    const uint16_t mantissa = value & 0x3FF;

    if(exponent == 0)
    {
        if(mantissa == 0) return sign ? -0.0f : 0.0f;
        const float m = (float)mantissa / 1024.0f;
        const float out = std::ldexp(m, -14);
        return sign ? -out : out;
    }
    if(exponent == 31)
    {
        return sign ? -INFINITY : INFINITY;
    }

    const float m = 1.0f + ((float)mantissa / 1024.0f);
    const float out = std::ldexp(m, (int)exponent - 15);
    return sign ? -out : out;
}

static uint8_t LinearHdrToSdr8(float v)
{
    if(!std::isfinite(v) || v <= 0.0f) return 0;
    const float compressed = v / (1.0f + v);
    const float srgb = std::pow((std::max)(0.0f, (std::min)(1.0f, compressed)), 1.0f / 2.2f);
    return (uint8_t)std::lround(srgb * 255.0f);
}

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

    auto RectArea = [](int w, int h) -> long long {
        if(w <= 0 || h <= 0) return 0;
        return (long long)w * (long long)h;
    };
    auto IntersectArea = [&](int ax0, int ay0, int ax1, int ay1, int bx0, int by0, int bx1, int by1) -> long long {
        int ix0 = (std::max)(ax0, bx0);
        int iy0 = (std::max)(ay0, by0);
        int ix1 = (std::min)(ax1, bx1);
        int iy1 = (std::min)(ay1, by1);
        return RectArea(ix1 - ix0, iy1 - iy0);
    };

    const int target_x0 = screen_x;
    const int target_y0 = screen_y;
    const int target_x1 = screen_x + screen_width;
    const int target_y1 = screen_y + screen_height;
    const long long target_area = RectArea(screen_width, screen_height);

    long long best_overlap = -1;
    DXGI_OUTPUT_DESC best_desc = {};
    IDXGIOutput* best_output = nullptr;

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
        long long overlap = IntersectArea(left, top, right, bottom, target_x0, target_y0, target_x1, target_y1);
        if(overlap > best_overlap)
        {
            if(best_output) best_output->Release();
            best_output = output; // take ownership
            best_desc = desc;
            best_overlap = overlap;
            continue;
        }
        output->Release();
    }

    if(!best_output || best_overlap <= 0 || (target_area > 0 && best_overlap < target_area / 4))
    {
        if(best_output) best_output->Release();
        adapter->Release();
        context->Release();
        device->Release();
        return false;
    }

    hr = best_output->QueryInterface(__uuidof(IDXGIOutput1), (void**)&output1);
    best_output->Release();
    if(FAILED(hr) || !output1)
    {
        adapter->Release();
        context->Release();
        device->Release();
        return false;
    }

    hr = output1->DuplicateOutput(device, &duplication);
    output1->Release();
    if(FAILED(hr) || !duplication)
    {
        adapter->Release();
        context->Release();
        device->Release();
        return false;
    }

    DXGI_OUTDUPL_DESC dup_desc;
    duplication->GetDesc(&dup_desc);
    out_width = dup_desc.ModeDesc.Width;
    out_height = dup_desc.ModeDesc.Height;
    if(dup_desc.ModeDesc.Format != DXGI_FORMAT_B8G8R8A8_UNORM &&
       dup_desc.ModeDesc.Format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB &&
       dup_desc.ModeDesc.Format != DXGI_FORMAT_R8G8B8A8_UNORM &&
       dup_desc.ModeDesc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB &&
       dup_desc.ModeDesc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT)
    {
        duplication->Release();
        adapter->Release();
        context->Release();
        device->Release();
        return false;
    }

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
        adapter->Release();
        context->Release();
        device->Release();
        return false;
    }

    out.device = device;
    out.context = context;
    out.duplication = duplication;
    out.staging_texture = staging_texture;
    out.format = dup_desc.ModeDesc.Format;
    out.width = out_width;
    out.height = out_height;
    adapter->Release();
    return true;
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
    int screen_index = -1;
    try
    {
        screen_index = std::stoi(source_id.substr(underscore_pos + 1));
    }
    catch(...)
    {
        LOG_WARNING("[ScreenCapture] Invalid source index in source_id '%s'", source_id.c_str());
        return;
    }

    CaptureSourceInfo source_info;
    {
        std::lock_guard<std::mutex> lock(sources_mutex);
        std::map<std::string, CaptureSourceInfo>::const_iterator source_it = sources.find(source_id);
        if(source_it == sources.end())
        {
            LOG_WARNING("[ScreenCapture] Source '%s' not found", source_id.c_str());
            return;
        }
        source_info = source_it->second;
    }

    if(source_info.width <= 0 || source_info.height <= 0)
    {
        LOG_WARNING("[ScreenCapture] Invalid geometry for '%s' (%dx%d)", source_id.c_str(), source_info.width, source_info.height);
        return;
    }

    const int dx = source_info.x;
    const int dy = source_info.y;
    const int dw = source_info.width;
    const int dh = source_info.height;
    DXGICaptureState dxgi_state;
    bool use_dxgi = TryCreateDXGIDuplication(dx, dy, dw, dh, dxgi_state);
    bool logged_dxgi_unavailable = false;

    uint64_t frame_counter = 0;
    std::vector<uint8_t> dxgi_rgba_buffer;
    std::chrono::steady_clock::time_point thread_start_tp = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point last_dxgi_retry = thread_start_tp;
    std::chrono::steady_clock::time_point last_gdi_force = thread_start_tp;
    std::chrono::steady_clock::time_point last_frame_produced = thread_start_tp;
    std::chrono::steady_clock::time_point last_stuck_warn = thread_start_tp;
    LOG_INFO("[ScreenCapture] Capture thread started for '%s' (use_dxgi=%d, %dx%d at (%d,%d), target_fps=%d)",
             source_id.c_str(), (int)use_dxgi, dw, dh, dx, dy, target_fps.load());
    while(active_flag->load())
    {
        const int loop_fps = target_fps.load();
        const int target_frame_time_ms = 1000 / (loop_fps > 0 ? loop_fps : 1);
        std::chrono::steady_clock::time_point frame_start = std::chrono::steady_clock::now();
        {
            int idle_ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(frame_start - last_frame_produced).count();
            int since_warn_ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(frame_start - last_stuck_warn).count();
            if(idle_ms >= 2000 && since_warn_ms >= 5000)
            {
                LOG_WARNING("[ScreenCapture] '%s' has not produced a frame for %d ms (frames=%llu, use_dxgi=%d). Forcing GDI capture.",
                            source_id.c_str(), idle_ms, (unsigned long long)frame_counter, (int)use_dxgi);
                last_stuck_warn = frame_start;
                last_gdi_force = frame_start - std::chrono::milliseconds(target_frame_time_ms);
            }
        }

        QImage image;
        bool dxgi_timed_out = false;
        if(use_dxgi && dxgi_state.IsValid())
        {
            DXGI_OUTDUPL_FRAME_INFO frame_info;
            IDXGIResource* resource = nullptr;
            HRESULT hr = dxgi_state.duplication->AcquireNextFrame(8, &frame_info, &resource);
            if(hr == DXGI_ERROR_WAIT_TIMEOUT)
            {
                dxgi_timed_out = true;
            }
            else if(hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_ACCESS_DENIED || hr == DXGI_ERROR_DEVICE_REMOVED)
            {
                LOG_WARNING("[ScreenCapture] DXGI access lost for '%s' (hr=0x%08lx), falling back to GDI",
                            source_id.c_str(), (unsigned long)hr);
                if(resource) { resource->Release(); resource = nullptr; }
                dxgi_state.Release();
                use_dxgi = false;
            }
            else if(SUCCEEDED(hr) && resource)
            {
                dxgi_state.desktop_frame_acquired = true;
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
                        const uint8_t* src = (const uint8_t*)mapped.pData;
                        const UINT expected_row_bytes = (dxgi_state.format == DXGI_FORMAT_R16G16B16A16_FLOAT)
                                                        ? (dxgi_state.width * 8)
                                                        : (dxgi_state.width * 4);
                        if(mapped.RowPitch < expected_row_bytes)
                        {
                            dxgi_state.context->Unmap(dxgi_state.staging_texture, 0);
                            if(dxgi_state.desktop_frame_acquired)
                            {
                                dxgi_state.duplication->ReleaseFrame();
                                dxgi_state.desktop_frame_acquired = false;
                            }
                            use_dxgi = false;
                            continue;
                        }
                        const bool src_is_bgra =
                            (dxgi_state.format == DXGI_FORMAT_B8G8R8A8_UNORM ||
                             dxgi_state.format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);
                        const bool src_is_fp16 = (dxgi_state.format == DXGI_FORMAT_R16G16B16A16_FLOAT);

                        const int target_w = target_width.load();
                        const int target_h = target_height.load();
                        const bool needs_downscale = ((int)dxgi_state.width != target_w || (int)dxgi_state.height != target_h);

                        if(src_is_fp16)
                        {
                            const int out_w = needs_downscale ? target_w : (int)dxgi_state.width;
                            const int out_h = needs_downscale ? target_h : (int)dxgi_state.height;
                            const size_t out_need = (size_t)out_w * (size_t)out_h * 4;
                            if(dxgi_rgba_buffer.size() < out_need)
                                dxgi_rgba_buffer.resize(out_need);
                            uint8_t* out_dst = dxgi_rgba_buffer.data();

                            for(int y = 0; y < out_h; y++)
                            {
                                const UINT src_y = needs_downscale
                                    ? (UINT)(((uint64_t)y * dxgi_state.height) / (uint64_t)out_h)
                                    : (UINT)y;
                                const uint16_t* src_row = (const uint16_t*)(src + (size_t)src_y * mapped.RowPitch);
                                uint8_t* row_dst = out_dst + (size_t)y * out_w * 4;
                                for(int x = 0; x < out_w; x++)
                                {
                                    const UINT src_x = needs_downscale
                                        ? (UINT)(((uint64_t)x * dxgi_state.width) / (uint64_t)out_w)
                                        : (UINT)x;
                                    const uint16_t* px = src_row + (size_t)src_x * 4;
                                    row_dst[0] = LinearHdrToSdr8(HalfToFloat(px[0]));
                                    row_dst[1] = LinearHdrToSdr8(HalfToFloat(px[1]));
                                    row_dst[2] = LinearHdrToSdr8(HalfToFloat(px[2]));
                                    row_dst[3] = 255;
                                    row_dst += 4;
                                }
                            }
                            dxgi_state.context->Unmap(dxgi_state.staging_texture, 0);
                            image = QImage(out_dst, out_w, out_h, out_w * 4, QImage::Format_RGBA8888).copy();
                        }
                        else if(src_is_bgra)
                        {
                            QImage wrap((const uchar*)src, (int)dxgi_state.width, (int)dxgi_state.height,
                                        (int)mapped.RowPitch, QImage::Format_ARGB32);
                            if(needs_downscale)
                            {
                                image = wrap.scaled(target_w, target_h, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                                            .convertToFormat(QImage::Format_RGBA8888);
                            }
                            else
                            {
                                image = wrap.convertToFormat(QImage::Format_RGBA8888);
                            }
                            dxgi_state.context->Unmap(dxgi_state.staging_texture, 0);
                        }
                        else
                        {
                            QImage wrap((const uchar*)src, (int)dxgi_state.width, (int)dxgi_state.height,
                                        (int)mapped.RowPitch, QImage::Format_RGBA8888);
                            if(needs_downscale)
                            {
                                image = wrap.scaled(target_w, target_h, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                                            .convertToFormat(QImage::Format_RGBA8888);
                            }
                            else
                            {
                                image = wrap.copy();
                            }
                            dxgi_state.context->Unmap(dxgi_state.staging_texture, 0);
                        }
                    }
                    if(dxgi_state.desktop_frame_acquired)
                    {
                        dxgi_state.duplication->ReleaseFrame();
                        dxgi_state.desktop_frame_acquired = false;
                    }
                }
                else
                {
                    if(dxgi_state.desktop_frame_acquired)
                    {
                        dxgi_state.duplication->ReleaseFrame();
                        dxgi_state.desktop_frame_acquired = false;
                    }
                }
            }
            else
            {
                LOG_WARNING("[ScreenCapture] DXGI AcquireNextFrame returned unexpected hr=0x%08lx for '%s' (resource=%p), recreating duplication",
                            (unsigned long)hr, source_id.c_str(), (void*)resource);
                if(resource) { resource->Release(); resource = nullptr; }
                if(dxgi_state.desktop_frame_acquired)
                {
                    dxgi_state.duplication->ReleaseFrame();
                    dxgi_state.desktop_frame_acquired = false;
                }
                dxgi_state.Release();
                use_dxgi = false;
            }
        }
        if(image.isNull())
        {
            std::chrono::steady_clock::time_point now_tp = std::chrono::steady_clock::now();
            const int gdi_force_threshold_ms = 250;
            const int idle_ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(now_tp - last_frame_produced).count();
            const bool should_force_gdi = dxgi_timed_out &&
                idle_ms >= gdi_force_threshold_ms &&
                std::chrono::duration_cast<std::chrono::milliseconds>(now_tp - last_gdi_force).count() >= gdi_force_threshold_ms;
            if(dxgi_timed_out && !should_force_gdi)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(target_frame_time_ms > 0 ? target_frame_time_ms : 2));
                continue;
            }
            if(should_force_gdi)
            {
                last_gdi_force = now_tp;
            }
            if(!use_dxgi || should_force_gdi)
            {
                if(!use_dxgi && !logged_dxgi_unavailable)
                {
                    LOG_WARNING("[ScreenCapture] DXGI unavailable for '%s' (screen %d), falling back to GDI", source_id.c_str(), screen_index);
                    logged_dxgi_unavailable = true;
                }
                HDC screen_dc = GetDC(nullptr);
                if(screen_dc)
                {
                    HDC mem_dc = CreateCompatibleDC(screen_dc);
                    if(mem_dc)
                    {
                        HBITMAP bmp = CreateCompatibleBitmap(screen_dc, dw, dh);
                        if(bmp)
                        {
                            HGDIOBJ old_obj = SelectObject(mem_dc, bmp);
                            if(BitBlt(mem_dc, 0, 0, dw, dh, screen_dc, dx, dy, SRCCOPY))
                            {
                                BITMAPINFO bmi = {};
                                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                                bmi.bmiHeader.biWidth = dw;
                                bmi.bmiHeader.biHeight = -dh;
                                bmi.bmiHeader.biPlanes = 1;
                                bmi.bmiHeader.biBitCount = 32;
                                bmi.bmiHeader.biCompression = BI_RGB;

                                std::vector<uint8_t> buffer((size_t)dw * (size_t)dh * 4);
                                if(GetDIBits(mem_dc, bmp, 0, (UINT)dh, buffer.data(), &bmi, DIB_RGB_COLORS) > 0)
                                {
                                    image = QImage(buffer.data(), dw, dh, dw * 4, QImage::Format_RGBA8888).copy();
                                }
                            }
                            SelectObject(mem_dc, old_obj);
                            DeleteObject(bmp);
                        }
                        DeleteDC(mem_dc);
                    }
                    ReleaseDC(nullptr, screen_dc);
                }
            }

            if(image.isNull())
            {
                if(!use_dxgi)
                {
                    std::chrono::steady_clock::time_point retry_tp = std::chrono::steady_clock::now();
                    if(std::chrono::duration_cast<std::chrono::seconds>(retry_tp - last_dxgi_retry).count() >= 3)
                    {
                        last_dxgi_retry = retry_tp;
                        if(TryCreateDXGIDuplication(dx, dy, dw, dh, dxgi_state))
                        {
                            use_dxgi = true;
                            logged_dxgi_unavailable = false;
                            LOG_INFO("[ScreenCapture] DXGI re-established for '%s'", source_id.c_str());
                        }
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(target_frame_time_ms > 0 ? target_frame_time_ms : 2));
                continue;
            }
        }
        else if(!use_dxgi)
        {
            std::chrono::steady_clock::time_point now_tp = std::chrono::steady_clock::now();
            if(std::chrono::duration_cast<std::chrono::seconds>(now_tp - last_dxgi_retry).count() >= 3)
            {
                last_dxgi_retry = now_tp;
                if(TryCreateDXGIDuplication(dx, dy, dw, dh, dxgi_state))
                {
                    use_dxgi = true;
                    logged_dxgi_unavailable = false;
                    LOG_INFO("[ScreenCapture] DXGI re-established for '%s' after GDI fallback", source_id.c_str());
                }
            }
        }

        int target_w = target_width.load();
        int target_h = target_height.load();
        if(image.width() != target_w || image.height() != target_h)
        {
            image = image.scaled(target_w, target_h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        if(image.format() != QImage::Format_RGBA8888)
        {
            image = image.convertToFormat(QImage::Format_RGBA8888);
        }

        std::shared_ptr<CapturedFrame> frame = std::make_shared<CapturedFrame>();
        frame->width = image.width();
        frame->height = image.height();
        frame->data.resize(frame->width * frame->height * 4);
        const int line_bytes = frame->width * 4;
        const int src_stride = image.bytesPerLine();
        const uint8_t* src = image.constBits();
        uint8_t* dst = frame->data.data();
        if(src_stride == line_bytes)
        {
            memcpy(dst, src, (size_t)line_bytes * (size_t)frame->height);
        }
        else
        {
            for(int y = 0; y < frame->height; y++)
            {
                memcpy(dst, src, (size_t)line_bytes);
                dst += line_bytes;
                src += src_stride;
            }
        }
        frame->frame_id = frame_counter++;
        frame->timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        frame->valid = true;

        {
            std::lock_guard<std::mutex> lock(frames_mutex);
            latest_frames[source_id] = frame;
        }

        last_frame_produced = std::chrono::steady_clock::now();
        if(frame_counter == 1)
        {
            LOG_INFO("[ScreenCapture] '%s' first frame produced (%dx%d, dxgi=%d)",
                     source_id.c_str(), frame->width, frame->height, (int)use_dxgi);
        }
        else if(frame_counter == 30 || (frame_counter % 600) == 0)
        {
            LOG_VERBOSE("[ScreenCapture] '%s' produced frame #%llu (%dx%d, dxgi=%d)",
                        source_id.c_str(), (unsigned long long)frame_counter,
                        frame->width, frame->height, (int)use_dxgi);
        }

        std::chrono::steady_clock::time_point frame_end = last_frame_produced;
        int elapsed = (int)std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start).count();
        int sleep_time = target_frame_time_ms - elapsed;
        if(sleep_time < 2)
            sleep_time = 2;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
    }

    LOG_INFO("[ScreenCapture] Capture thread stopped for '%s' (produced %llu frames)",
             source_id.c_str(), (unsigned long long)frame_counter);
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
    int screen_index = -1;
    try
    {
        screen_index = std::stoi(source_id.substr(underscore_pos + 1));
    }
    catch(...)
    {
        LOG_WARNING("[ScreenCapture] Invalid source index in StartCapturePlatform: '%s'", source_id.c_str());
        return false;
    }
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
