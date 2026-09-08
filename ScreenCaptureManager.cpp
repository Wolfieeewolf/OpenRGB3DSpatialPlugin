// SPDX-License-Identifier: GPL-2.0-only

#include "ScreenCaptureManager.h"
#include "PluginLog.h"
#include "ScreenCaptureDownscale.h"
#include <chrono>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <set>
#include <vector>
#include <QGuiApplication>
#include <QScreen>
#include <QImage>

namespace
{
void WorkingCaptureSize(int src_w, int src_h, int target_w, int target_h, int& out_w, int& out_h)
{
    out_w = std::max(1, std::min(target_w, std::max(src_w, 1)));
    out_h = std::max(1, std::min(target_h, std::max(src_h, 1)));
}

QImage BoxScaleRgbaImage(const QImage& src, int dst_w, int dst_h)
{
    if(src.isNull())
    {
        return QImage();
    }
    QImage rgba = src.convertToFormat(QImage::Format_RGBA8888);
    WorkingCaptureSize(rgba.width(), rgba.height(), dst_w, dst_h, dst_w, dst_h);
    if(dst_w == rgba.width() && dst_h == rgba.height())
    {
        return rgba;
    }
    QImage out(dst_w, dst_h, QImage::Format_RGBA8888);
    if(out.isNull())
    {
        return QImage();
    }
    if(out.bytesPerLine() == dst_w * 4)
    {
        BoxDownscaleToRgba(rgba.constBits(),
                           rgba.width(),
                           rgba.height(),
                           rgba.bytesPerLine(),
                           out.bits(),
                           dst_w,
                           dst_h,
                           0,
                           1,
                           2,
                           3);
        return out;
    }
    std::vector<uint8_t> packed((size_t)dst_w * (size_t)dst_h * 4u);
    BoxDownscaleToRgba(rgba.constBits(),
                       rgba.width(),
                       rgba.height(),
                       rgba.bytesPerLine(),
                       packed.data(),
                       dst_w,
                       dst_h,
                       0,
                       1,
                       2,
                       3);
    return QImage(packed.data(), dst_w, dst_h, dst_w * 4, QImage::Format_RGBA8888).copy();
}

std::shared_ptr<CapturedFrame> MakeCapturedFrameFromBgra(const uint8_t* bgra,
                                                          int w,
                                                          int h,
                                                          int stride,
                                                          bool flip_y,
                                                          bool gdi,
                                                          uint64_t frame_id)
{
    if(!bgra || w <= 0 || h <= 0)
    {
        return nullptr;
    }
    std::shared_ptr<CapturedFrame> frame = std::make_shared<CapturedFrame>();
    frame->width = w;
    frame->height = h;
    frame->data.resize((size_t)w * (size_t)h * 4u);
    CopyBgraToRgba(bgra, w, h, stride, frame->data.data(), flip_y);
    frame->frame_id = frame_id;
    frame->used_gdi_capture = gdi;
    frame->valid = true;
    frame->timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return frame;
}
} // namespace

#ifdef _WIN32
    #include <Windows.h>
    #include <d3d11.h>
    #include <dxgi1_2.h>
    #include <dxgi1_6.h>
    #include <d3dcompiler.h>
    #include <cmath>
    #include <cstring>

    #pragma comment(lib, "d3d11.lib")
    #pragma comment(lib, "dxgi.lib")
    #pragma comment(lib, "d3dcompiler.lib")
#endif

ScreenCaptureManager& ScreenCaptureManager::Instance()
{
    static ScreenCaptureManager instance;
    return instance;
}

ScreenCaptureManager::ScreenCaptureManager()
    : initialized(false)
    , capture_session_active(false)
    , target_width(480)
    , target_height(270)
    , target_fps(30)
    , windows_capture_backend_mode(1)
    , render_tick_snapshot_active(false)
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
        render_tick_snapshot.clear();
        render_tick_snapshot_active = false;
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
    if(!capture_session_active.load())
    {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(sources_mutex);
        if(sources.find(source_id) == sources.end())
        {
            static std::set<std::string> warned_unknown_sources;
            if(warned_unknown_sources.insert(source_id).second)
            {
                std::string available;
                for(std::map<std::string, CaptureSourceInfo>::const_iterator sit = sources.begin();
                    sit != sources.end();
                    ++sit)
                {
                    if(!available.empty()) available += ", ";
                    available += sit->first;
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
    if(render_tick_snapshot_active)
    {
        std::map<std::string, std::shared_ptr<CapturedFrame>>::const_iterator snap_it =
            render_tick_snapshot.find(source_id);
        if(snap_it != render_tick_snapshot.end())
        {
            return snap_it->second;
        }
    }
    std::map<std::string, std::shared_ptr<CapturedFrame>>::const_iterator it = latest_frames.find(source_id);
    if(it != latest_frames.end())
    {
        return it->second;
    }
    return nullptr;
}

void ScreenCaptureManager::BeginRenderTickSnapshot()
{
    std::lock_guard<std::mutex> lock(frames_mutex);
    render_tick_snapshot = latest_frames;
    render_tick_snapshot_active = true;
}

void ScreenCaptureManager::EndRenderTickSnapshot()
{
    std::lock_guard<std::mutex> lock(frames_mutex);
    render_tick_snapshot_active = false;
    render_tick_snapshot.clear();
}

void ScreenCaptureManager::SetDownscaleResolution(int width, int height)
{
    target_width.store((std::max)(32, (std::min)(width, kScreenMirrorMaxWorkingWidth)));
    target_height.store((std::max)(32, (std::min)(height, kScreenMirrorMaxWorkingHeight)));
}

void ScreenCaptureManager::SetTargetFPS(int fps)
{
    target_fps.store((std::max)(1, (std::min)(fps, 360)));
}

void ScreenCaptureManager::SetWindowsCaptureBackendMode(int mode)
{
    windows_capture_backend_mode.store((std::clamp)(mode, 0, 2));
}

void ScreenCaptureManager::SetCaptureSessionActive(bool active)
{
    const bool was_active = capture_session_active.exchange(active);
    if(active)
    {
        if(!was_active)
        {
            Initialize();
            RefreshSources();
            LOG_INFO("[ScreenCapture] Capture session started (DXGI/GDI enabled)");
        }
        return;
    }

    if(was_active)
    {
        Shutdown();
        LOG_INFO("[ScreenCapture] Capture session stopped (DXGI/GDI released)");
    }
}

#ifdef _WIN32

static constexpr UINT k_dxgi_acquire_timeout_ms = 100;
static constexpr int k_gdi_poll_after_dxgi_timeout_ms = 200;

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

        QRect geometry = screen->geometry();
        info.width = geometry.width();
        info.height = geometry.height();
        info.x = geometry.x();
        info.y = geometry.y();
        info.display_number = i + 1;

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

#ifdef _MSC_VER
static void SafeDupReleaseFrame(IDXGIOutputDuplication* dup, bool& acquired_flag)
{
    if(!dup || !acquired_flag)
        return;
    __try
    {
        dup->ReleaseFrame();
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }
    acquired_flag = false;
}
#else
static void SafeDupReleaseFrame(IDXGIOutputDuplication* dup, bool& acquired_flag)
{
    if(!dup || !acquired_flag)
        return;
    dup->ReleaseFrame();
    acquired_flag = false;
}
#endif

struct GpuFrameBlit
{
    ID3D11Device* device_ref = nullptr;
    ID3D11Texture2D* rt = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* ps = nullptr;
    ID3D11Buffer* cbuf = nullptr;
    ID3D11SamplerState* samp = nullptr;
    UINT dst_width = 0;
    UINT dst_height = 0;
    DXGI_FORMAT src_format = DXGI_FORMAT_UNKNOWN;

    void Release()
    {
        if(rtv)
        {
            rtv->Release();
            rtv = nullptr;
        }
        if(rt)
        {
            rt->Release();
            rt = nullptr;
        }
        if(vs)
        {
            vs->Release();
            vs = nullptr;
        }
        if(ps)
        {
            ps->Release();
            ps = nullptr;
        }
        if(cbuf)
        {
            cbuf->Release();
            cbuf = nullptr;
        }
        if(samp)
        {
            samp->Release();
            samp = nullptr;
        }
        device_ref = nullptr;
        dst_width = 0;
        dst_height = 0;
        src_format = DXGI_FORMAT_UNKNOWN;
    }

    bool Init(ID3D11Device* device, UINT dst_w, UINT dst_h, DXGI_FORMAT srcFmt)
    {
        if(device_ref == device && dst_width == dst_w && dst_height == dst_h && src_format == srcFmt &&
           vs && ps && cbuf && samp && rt && rtv)
        {
            return true;
        }
        Release();
        device_ref = device;
        dst_width = dst_w;
        dst_height = dst_h;
        src_format = srcFmt;

        static const char kHlsl[] =
            "cbuffer Params : register(b0) {\n"
            "  float g_hdrScale; float g_tonemap; float g_srcW; float g_srcH;\n"
            "  float g_dstW; float g_dstH; float2 g_pad;\n"
            "};\n"
            "Texture2D g_tex : register(t0);\n"
            "SamplerState g_sam : register(s0);\n"
            "struct VS_OUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };\n"
            "VS_OUT vs_main(uint vid : SV_VertexID) {\n"
            "  VS_OUT o;\n"
            "  float2 uv = float2((vid << 1) & 2, vid & 2);\n"
            "  o.pos = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);\n"
            "  o.uv = float2(uv.x, 1.0f - uv.y);\n"
            "  return o;\n"
            "}\n"
            "float4 ps_main(VS_OUT i) : SV_Target {\n"
            "  float4 c;\n"
            "  if(g_dstW + 0.5f < g_srcW || g_dstH + 0.5f < g_srcH) {\n"
            "    float2 duv = 0.25f * float2(g_srcW / max(g_dstW, 1.0f), g_srcH / max(g_dstH, 1.0f))\n"
            "                 / float2(max(g_srcW, 1.0f), max(g_srcH, 1.0f));\n"
            "    c  = g_tex.Sample(g_sam, i.uv + float2(-duv.x, -duv.y));\n"
            "    c += g_tex.Sample(g_sam, i.uv + float2( duv.x, -duv.y));\n"
            "    c += g_tex.Sample(g_sam, i.uv + float2(-duv.x,  duv.y));\n"
            "    c += g_tex.Sample(g_sam, i.uv + float2( duv.x,  duv.y));\n"
            "    c *= 0.25f;\n"
            "  } else {\n"
            "    c = g_tex.Sample(g_sam, i.uv);\n"
            "  }\n"
            "  if(g_tonemap > 0.5f) {\n"
            "    float3 rgb = c.rgb * g_hdrScale;\n"
            "    rgb = rgb / (1.0f + rgb);\n"
            "    rgb = pow(saturate(rgb), 1.0f / 2.2f);\n"
            "    c = float4(rgb, 1.0f);\n"
            "  }\n"
            "  return float4(c.rgb, 1.0f);\n"
            "}\n";

        ID3DBlob* vs_blob = nullptr;
        ID3DBlob* ps_blob = nullptr;
        ID3DBlob* err = nullptr;
        HRESULT hr = D3DCompile(kHlsl, std::strlen(kHlsl), nullptr, nullptr, nullptr, "vs_main", "vs_5_0", 0, 0, &vs_blob, &err);
        if(err)
        {
            err->Release();
            err = nullptr;
        }
        if(FAILED(hr) || !vs_blob)
        {
            Release();
            return false;
        }
        hr = D3DCompile(kHlsl, std::strlen(kHlsl), nullptr, nullptr, nullptr, "ps_main", "ps_5_0", 0, 0, &ps_blob, &err);
        if(err)
        {
            err->Release();
        }
        if(FAILED(hr) || !ps_blob)
        {
            vs_blob->Release();
            Release();
            return false;
        }

        hr = device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, &vs);
        vs_blob->Release();
        if(FAILED(hr))
        {
            Release();
            return false;
        }
        hr = device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, &ps);
        ps_blob->Release();
        if(FAILED(hr))
        {
            Release();
            return false;
        }

        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = 32;
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = device->CreateBuffer(&bd, nullptr, &cbuf);
        if(FAILED(hr))
        {
            Release();
            return false;
        }

        D3D11_SAMPLER_DESC sd = {};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        hr = device->CreateSamplerState(&sd, &samp);
        if(FAILED(hr))
        {
            Release();
            return false;
        }

        D3D11_TEXTURE2D_DESC td = {};
        td.Width = dst_w;
        td.Height = dst_h;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET;
        hr = device->CreateTexture2D(&td, nullptr, &rt);
        if(FAILED(hr))
        {
            Release();
            return false;
        }
        hr = device->CreateRenderTargetView(rt, nullptr, &rtv);
        if(FAILED(hr))
        {
            Release();
            return false;
        }
        return true;
    }

    bool Blit(ID3D11DeviceContext* ctx,
              ID3D11ShaderResourceView* srv,
              UINT src_w,
              UINT src_h,
              bool tonemap,
              float hdr_scale)
    {
        if(!ctx || !srv || !rtv || !vs || !ps || !cbuf || !samp)
            return false;

        struct CbParams
        {
            float hdr_scale;
            float tonemap;
            float src_w;
            float src_h;
            float dst_w;
            float dst_h;
            float pad[2];
        } params = {
            hdr_scale,
            tonemap ? 1.0f : 0.0f,
            (float)src_w,
            (float)src_h,
            (float)dst_width,
            (float)dst_height,
            {0.0f, 0.0f}
        };

        D3D11_MAPPED_SUBRESOURCE mr = {};
        if(FAILED(ctx->Map(cbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mr)))
            return false;
        std::memcpy(mr.pData, &params, sizeof(params));
        ctx->Unmap(cbuf, 0);

        D3D11_VIEWPORT vp = {};
        vp.Width = (FLOAT)dst_width;
        vp.Height = (FLOAT)dst_height;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        ctx->RSSetViewports(1, &vp);
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        ctx->IASetInputLayout(nullptr);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->VSSetShader(vs, nullptr, 0);
        ctx->PSSetShader(ps, nullptr, 0);
        ctx->PSSetConstantBuffers(0, 1, &cbuf);
        ctx->PSSetSamplers(0, 1, &samp);
        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->Draw(3, 0);
        ID3D11ShaderResourceView* null_srv = nullptr;
        ctx->PSSetShaderResources(0, 1, &null_srv);
        ID3D11RenderTargetView* null_rtv = nullptr;
        ctx->OMSetRenderTargets(0, &null_rtv, nullptr);
        return true;
    }

    bool CopyToStaging(ID3D11DeviceContext* ctx, ID3D11Texture2D* staging_bgra8) const
    {
        if(!ctx || !rt || !staging_bgra8)
            return false;
        ctx->CopyResource(staging_bgra8, rt);
        return true;
    }
};

struct DXGICaptureState
{
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    IDXGIOutputDuplication* duplication = nullptr;
    ID3D11Texture2D* staging_texture[2] = {nullptr, nullptr};
    ID3D11Texture2D* gpu_copy = nullptr;
    ID3D11ShaderResourceView* gpu_copy_srv = nullptr;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT width = 0;
    UINT height = 0;
    UINT staging_w = 0;
    UINT staging_h = 0;
    int staging_write = 0;
    int staging_ready = -1;
    bool desktop_frame_acquired = false;
    bool wide_gamut_hdr = false;
    bool use_gpu_hdr_tonemap = false;
    bool gpu_blit_failed = false;
    GpuFrameBlit gpu_blit;

    void ReleaseStaging()
    {
        for(int i = 0; i < 2; ++i)
        {
            if(staging_texture[i])
            {
                staging_texture[i]->Release();
                staging_texture[i] = nullptr;
            }
        }
        staging_w = 0;
        staging_h = 0;
        staging_write = 0;
        staging_ready = -1;
    }

    void Release()
    {
        gpu_blit.Release();
        gpu_blit_failed = false;
        use_gpu_hdr_tonemap = false;
        wide_gamut_hdr = false;
        if(duplication && desktop_frame_acquired)
        {
            SafeDupReleaseFrame(duplication, desktop_frame_acquired);
        }
        if(gpu_copy_srv)
        {
            gpu_copy_srv->Release();
            gpu_copy_srv = nullptr;
        }
        if(gpu_copy)
        {
            gpu_copy->Release();
            gpu_copy = nullptr;
        }
        ReleaseStaging();
        if(duplication)
        {
            duplication->Release();
            duplication = nullptr;
        }
        if(context)
        {
            context->Release();
            context = nullptr;
        }
        if(device)
        {
            device->Release();
            device = nullptr;
        }
        format = DXGI_FORMAT_UNKNOWN;
        width = 0;
        height = 0;
    }

    bool IsValid() const { return device && context && duplication && gpu_copy && width > 0 && height > 0; }

    bool EnsureStaging(UINT w, UINT h, DXGI_FORMAT staging_format)
    {
        if(staging_texture[0] && staging_texture[1] && staging_w == w && staging_h == h)
        {
            return true;
        }
        ReleaseStaging();
        for(int i = 0; i < 2; ++i)
        {
            D3D11_TEXTURE2D_DESC tex_desc = {};
            tex_desc.Width = w;
            tex_desc.Height = h;
            tex_desc.MipLevels = 1;
            tex_desc.ArraySize = 1;
            tex_desc.Format = staging_format;
            tex_desc.SampleDesc.Count = 1;
            tex_desc.Usage = D3D11_USAGE_STAGING;
            tex_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            HRESULT hr = device->CreateTexture2D(&tex_desc, nullptr, &staging_texture[i]);
            if(FAILED(hr) || !staging_texture[i])
            {
                ReleaseStaging();
                return false;
            }
        }
        staging_w = w;
        staging_h = h;
        return true;
    }
};

static long long dxgi_rect_area(int w, int h)
{
    if(w <= 0 || h <= 0)
        return 0;
    return (long long)w * (long long)h;
}

static long long dxgi_intersect_area(int ax0, int ay0, int ax1, int ay1,
                                     int bx0, int by0, int bx1, int by1)
{
    int ix0 = (std::max)(ax0, bx0);
    int iy0 = (std::max)(ay0, by0);
    int ix1 = (std::min)(ax1, bx1);
    int iy1 = (std::min)(ay1, by1);
    return dxgi_rect_area(ix1 - ix0, iy1 - iy0);
}

static bool TryCreateDXGIDuplication(int screen_x, int screen_y, int screen_width, int screen_height, DXGICaptureState& out)
{
    out.Release();
    IDXGIOutput1* output1 = nullptr;
    IDXGIOutputDuplication* duplication = nullptr;
    UINT out_width = 0, out_height = 0;
    bool wide_gamut = false;

    const int target_x0 = screen_x;
    const int target_y0 = screen_y;
    const int target_x1 = screen_x + screen_width;
    const int target_y1 = screen_y + screen_height;
    const long long target_area = dxgi_rect_area(screen_width, screen_height);

    IDXGIFactory1* factory = nullptr;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory);
    if(FAILED(hr) || !factory)
        return false;

    long long best_overlap = -1;
    IDXGIOutput* best_output = nullptr;
    IDXGIAdapter* best_adapter = nullptr;

    for(UINT adapter_index = 0; ; adapter_index++)
    {
        IDXGIAdapter* adapter = nullptr;
        if(factory->EnumAdapters(adapter_index, &adapter) == DXGI_ERROR_NOT_FOUND)
            break;
        if(!adapter)
            continue;

        for(UINT output_index = 0; ; output_index++)
        {
            IDXGIOutput* output = nullptr;
            HRESULT e = adapter->EnumOutputs(output_index, &output);
            if(e == DXGI_ERROR_NOT_FOUND)
                break;
            if(FAILED(e) || !output)
                continue;

            DXGI_OUTPUT_DESC desc;
            if(FAILED(output->GetDesc(&desc)))
            {
                output->Release();
                continue;
            }
            RECT& r = desc.DesktopCoordinates;
            int left = r.left;
            int top = r.top;
            int right = r.right;
            int bottom = r.bottom;
            long long overlap = dxgi_intersect_area(left, top, right, bottom, target_x0, target_y0, target_x1, target_y1);
            if(overlap > best_overlap)
            {
                if(best_output)
                    best_output->Release();
                if(best_adapter)
                    best_adapter->Release();
                adapter->AddRef();
                best_adapter = adapter;
                best_output = output;
                best_overlap = overlap;
            }
            else
            {
                output->Release();
            }
        }
        adapter->Release();
    }

    factory->Release();
    factory = nullptr;

    if(!best_output || !best_adapter || best_overlap <= 0 || (target_area > 0 && best_overlap < target_area / 4))
    {
        if(best_output)
            best_output->Release();
        if(best_adapter)
            best_adapter->Release();
        return false;
    }

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL feature_level;
    const UINT dev_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    hr = D3D11CreateDevice(
        best_adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, dev_flags, nullptr, 0,
        D3D11_SDK_VERSION, &device, &feature_level, &context);
    if(best_adapter)
    {
        best_adapter->Release();
        best_adapter = nullptr;
    }
    if(FAILED(hr) || !device || !context)
    {
        if(best_output)
            best_output->Release();
        return false;
    }

    hr = best_output->QueryInterface(__uuidof(IDXGIOutput1), (void**)&output1);
    best_output->Release();
    best_output = nullptr;
    if(FAILED(hr) || !output1)
    {
        context->Release();
        device->Release();
        return false;
    }

    IDXGIOutput6* output6 = nullptr;
    if(SUCCEEDED(output1->QueryInterface(__uuidof(IDXGIOutput6), (void**)&output6)) && output6)
    {
        DXGI_OUTPUT_DESC1 desc1 = {};
        if(SUCCEEDED(output6->GetDesc1(&desc1)))
        {
            wide_gamut = (desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
        }
        const DXGI_FORMAT prefer_formats[] = {
            DXGI_FORMAT_R16G16B16A16_FLOAT,
            DXGI_FORMAT_R10G10B10A2_UNORM,
            DXGI_FORMAT_B8G8R8A8_UNORM,
        };
        hr = output6->DuplicateOutput1(device, 0, 3, prefer_formats, &duplication);
        output6->Release();
        output6 = nullptr;
    }
    else
    {
        hr = E_FAIL;
    }

    if(FAILED(hr) || !duplication)
    {
        hr = output1->DuplicateOutput(device, &duplication);
    }
    output1->Release();
    output1 = nullptr;
    if(FAILED(hr) || !duplication)
    {
        context->Release();
        device->Release();
        return false;
    }

    DXGI_OUTDUPL_DESC dup_desc = {};
    duplication->GetDesc(&dup_desc);
    out_width = dup_desc.ModeDesc.Width;
    out_height = dup_desc.ModeDesc.Height;
    const DXGI_FORMAT dup_fmt = dup_desc.ModeDesc.Format;
    if(dup_fmt != DXGI_FORMAT_B8G8R8A8_UNORM && dup_fmt != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB &&
       dup_fmt != DXGI_FORMAT_R8G8B8A8_UNORM && dup_fmt != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB &&
       dup_fmt != DXGI_FORMAT_R16G16B16A16_FLOAT && dup_fmt != DXGI_FORMAT_R10G10B10A2_UNORM)
    {
        duplication->Release();
        context->Release();
        device->Release();
        return false;
    }

    const bool needs_hdr_tonemap =
        (dup_fmt == DXGI_FORMAT_R16G16B16A16_FLOAT || dup_fmt == DXGI_FORMAT_R10G10B10A2_UNORM);

    D3D11_TEXTURE2D_DESC copy_desc = {};
    copy_desc.Width = out_width;
    copy_desc.Height = out_height;
    copy_desc.MipLevels = 1;
    copy_desc.ArraySize = 1;
    /* Sample as UNORM so SDR desktop pixels stay display-referred (no sRGB view). */
    copy_desc.Format = dup_fmt;
    if(copy_desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
    {
        copy_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    }
    else if(copy_desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
    {
        copy_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    copy_desc.SampleDesc.Count = 1;
    copy_desc.Usage = D3D11_USAGE_DEFAULT;
    copy_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ID3D11Texture2D* gpu_copy = nullptr;
    hr = device->CreateTexture2D(&copy_desc, nullptr, &gpu_copy);
    if(FAILED(hr) || !gpu_copy)
    {
        duplication->Release();
        context->Release();
        device->Release();
        return false;
    }
    ID3D11ShaderResourceView* gpu_copy_srv = nullptr;
    hr = device->CreateShaderResourceView(gpu_copy, nullptr, &gpu_copy_srv);
    if(FAILED(hr) || !gpu_copy_srv)
    {
        gpu_copy->Release();
        duplication->Release();
        context->Release();
        device->Release();
        return false;
    }

    out.device = device;
    out.context = context;
    out.duplication = duplication;
    out.gpu_copy = gpu_copy;
    out.gpu_copy_srv = gpu_copy_srv;
    out.format = dup_fmt;
    out.width = out_width;
    out.height = out_height;
    out.wide_gamut_hdr = wide_gamut;
    out.use_gpu_hdr_tonemap = needs_hdr_tonemap;
    out.gpu_blit_failed = false;
    LOG_INFO("[ScreenCapture] DXGI path: format=%u wide_gamut=%d hdr=%d %ux%u (GPU scale/copy before Map)",
             (unsigned)dup_fmt, (int)wide_gamut, (int)needs_hdr_tonemap, (int)out_width, (int)out_height);
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
    bool use_dxgi = false;
    if(windows_capture_backend_mode.load() != 2)
    {
        use_dxgi = TryCreateDXGIDuplication(dx, dy, dw, dh, dxgi_state);
    }
    bool logged_dxgi_unavailable = false;
    bool logged_gpu_map_size = false;

    uint64_t frame_counter = 0;
    std::chrono::steady_clock::time_point thread_start_tp = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point last_dxgi_retry = thread_start_tp;
    std::chrono::steady_clock::time_point last_gdi_force = thread_start_tp;
    std::chrono::steady_clock::time_point last_frame_produced = thread_start_tp;
    std::chrono::steady_clock::time_point last_stuck_warn = thread_start_tp;
    uint64_t missed_presents = 0;
    LOG_INFO("[ScreenCapture] Capture thread started for '%s' (use_dxgi=%d, backend_mode=%d, %dx%d at (%d,%d), target_fps=%d)",
             source_id.c_str(), (int)use_dxgi, windows_capture_backend_mode.load(), dw, dh, dx, dy, target_fps.load());

    auto store_latest = [&](std::shared_ptr<CapturedFrame> frame) {
        if(!frame)
        {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(frames_mutex);
            latest_frames[source_id] = frame;
        }
        last_frame_produced = std::chrono::steady_clock::now();
        if(frame->frame_id == 0)
        {
            LOG_INFO("[ScreenCapture] '%s' first frame produced (%dx%d, dxgi=%d)",
                     source_id.c_str(), frame->width, frame->height, (int)use_dxgi);
        }
        else if(frame->frame_id == 29 || (frame->frame_id % 600) == 0)
        {
            LOG_VERBOSE("[ScreenCapture] '%s' produced frame #%llu (%dx%d, dxgi=%d, missed_presents=%llu)",
                        source_id.c_str(), (unsigned long long)(frame->frame_id + 1),
                        frame->width, frame->height, (int)use_dxgi,
                        (unsigned long long)missed_presents);
        }
    };
    while(active_flag->load())
    {
        const int backend_mode = windows_capture_backend_mode.load();
        const bool allow_gdi = (backend_mode != 1);
        if(backend_mode == 2 && use_dxgi)
        {
            dxgi_state.Release();
            use_dxgi = false;
        }

        const int loop_fps = target_fps.load();
        const int target_frame_time_ms = 1000 / (loop_fps > 0 ? loop_fps : 1);
        std::chrono::steady_clock::time_point frame_start = std::chrono::steady_clock::now();
        {
            int idle_ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(frame_start - last_frame_produced).count();
            int since_warn_ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(frame_start - last_stuck_warn).count();
            if(idle_ms >= 2000 && since_warn_ms >= 5000)
            {
                if(allow_gdi)
                {
                    LOG_WARNING("[ScreenCapture] '%s' has not produced a frame for %d ms (frames=%llu, use_dxgi=%d). Forcing GDI capture.",
                                source_id.c_str(), idle_ms, (unsigned long long)frame_counter, (int)use_dxgi);
                    last_gdi_force = frame_start - std::chrono::milliseconds(target_frame_time_ms);
                }
                else
                {
                    LOG_WARNING("[ScreenCapture] '%s' has not produced a frame for %d ms (frames=%llu, DXGI-only; no GDI fallback).",
                                source_id.c_str(), idle_ms, (unsigned long long)frame_counter);
                }
                last_stuck_warn = frame_start;
            }
        }

        bool dxgi_timed_out = false;
        bool frame_from_gdi = false;
        if(use_dxgi && dxgi_state.IsValid())
        {
            DXGI_OUTDUPL_FRAME_INFO frame_info;
            IDXGIResource* resource = nullptr;
            HRESULT hr = dxgi_state.duplication->AcquireNextFrame(k_dxgi_acquire_timeout_ms, &frame_info, &resource);
            if(hr == DXGI_ERROR_WAIT_TIMEOUT)
            {
                dxgi_timed_out = true;
            }
            else if(hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_ACCESS_DENIED || hr == DXGI_ERROR_DEVICE_REMOVED)
            {
                if(windows_capture_backend_mode.load() == 1)
                {
                    LOG_WARNING("[ScreenCapture] DXGI access lost for '%s' (hr=0x%08lx); releasing duplication (DXGI-only, will retry)",
                                source_id.c_str(), (unsigned long)hr);
                }
                else
                {
                    LOG_WARNING("[ScreenCapture] DXGI access lost for '%s' (hr=0x%08lx), falling back to GDI",
                                source_id.c_str(), (unsigned long)hr);
                }
                if(resource) { resource->Release(); resource = nullptr; }
                dxgi_state.Release();
                use_dxgi = false;
            }
            else if(SUCCEEDED(hr) && resource)
            {
                if(frame_info.LastPresentTime.QuadPart == 0 && frame_info.AccumulatedFrames == 0 &&
                   frame_counter > 0)
                {
                    resource->Release();
                    dxgi_state.desktop_frame_acquired = true;
                    SafeDupReleaseFrame(dxgi_state.duplication, dxgi_state.desktop_frame_acquired);
                    continue;
                }
                if(frame_info.AccumulatedFrames > 1)
                {
                    missed_presents += (uint64_t)(frame_info.AccumulatedFrames - 1);
                }

                dxgi_state.desktop_frame_acquired = true;
                ID3D11Texture2D* tex = nullptr;
                hr = resource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex);
                resource->Release();
                if(SUCCEEDED(hr) && tex)
                {
                    D3D11_TEXTURE2D_DESC acquired_desc = {};
                    tex->GetDesc(&acquired_desc);
                    const int target_w = target_width.load();
                    const int target_h = target_height.load();
                    int out_w = 0;
                    int out_h = 0;
                    WorkingCaptureSize((int)dxgi_state.width, (int)dxgi_state.height,
                                       target_w, target_h, out_w, out_h);

                    bool gpu_ok = false;
                    const bool size_ok = (acquired_desc.Width == dxgi_state.width &&
                                          acquired_desc.Height == dxgi_state.height);
                    const bool need_tonemap = dxgi_state.use_gpu_hdr_tonemap;
                    const bool native_bgra =
                        (dxgi_state.format == DXGI_FORMAT_B8G8R8A8_UNORM ||
                         dxgi_state.format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);
                    const bool copy_1to1 = size_ok && !need_tonemap && native_bgra &&
                        out_w == (int)dxgi_state.width && out_h == (int)dxgi_state.height;

                    if(copy_1to1)
                    {
                        if(dxgi_state.EnsureStaging((UINT)out_w, (UINT)out_h, DXGI_FORMAT_B8G8R8A8_UNORM))
                        {
                            dxgi_state.context->CopyResource(
                                dxgi_state.staging_texture[dxgi_state.staging_write], tex);
                            gpu_ok = true;
                        }
                    }
                    else if(size_ok && !dxgi_state.gpu_blit_failed)
                    {
                        dxgi_state.context->CopyResource(dxgi_state.gpu_copy, tex);
                        const bool is_r10 = (dxgi_state.format == DXGI_FORMAT_R10G10B10A2_UNORM);
                        float hdr_scale = 1.0f;
                        if(need_tonemap)
                        {
                            if(dxgi_state.wide_gamut_hdr)
                                hdr_scale = is_r10 ? 6.0f : 1.0f;
                            else
                                hdr_scale = is_r10 ? 1.5f : 1.0f;
                        }
                        if(dxgi_state.gpu_blit.Init(dxgi_state.device, (UINT)out_w, (UINT)out_h, dxgi_state.format) &&
                           dxgi_state.EnsureStaging((UINT)out_w, (UINT)out_h, DXGI_FORMAT_B8G8R8A8_UNORM) &&
                           dxgi_state.gpu_blit.Blit(dxgi_state.context,
                                                    dxgi_state.gpu_copy_srv,
                                                    dxgi_state.width,
                                                    dxgi_state.height,
                                                    need_tonemap,
                                                    hdr_scale) &&
                           dxgi_state.gpu_blit.CopyToStaging(
                               dxgi_state.context,
                               dxgi_state.staging_texture[dxgi_state.staging_write]))
                        {
                            gpu_ok = true;
                        }
                        else
                        {
                            dxgi_state.gpu_blit_failed = true;
                            LOG_WARNING("[ScreenCapture] GPU blit failed for '%s'; DXGI will not scale this output",
                                        source_id.c_str());
                        }
                    }
                    tex->Release();
                    tex = nullptr;

                    /* Release the duplication slot before Map so the desktop can keep presenting. */
                    if(dxgi_state.desktop_frame_acquired)
                        SafeDupReleaseFrame(dxgi_state.duplication, dxgi_state.desktop_frame_acquired);

                    if(gpu_ok)
                    {
                        const int ready = dxgi_state.staging_ready;
                        dxgi_state.staging_ready = dxgi_state.staging_write;
                        dxgi_state.staging_write = 1 - dxgi_state.staging_write;
                        if(ready >= 0 && dxgi_state.staging_texture[ready])
                        {
                            D3D11_MAPPED_SUBRESOURCE mapped = {};
                            hr = dxgi_state.context->Map(dxgi_state.staging_texture[ready], 0, D3D11_MAP_READ, 0, &mapped);
                            if(SUCCEEDED(hr))
                            {
                                const UINT expected_row_bytes = dxgi_state.staging_w * 4u;
                                if(mapped.RowPitch >= expected_row_bytes)
                                {
                                    std::shared_ptr<CapturedFrame> frame = MakeCapturedFrameFromBgra(
                                        (const uint8_t*)mapped.pData,
                                        (int)dxgi_state.staging_w,
                                        (int)dxgi_state.staging_h,
                                        (int)mapped.RowPitch,
                                        true,
                                        false,
                                        frame_counter++);
                                    dxgi_state.context->Unmap(dxgi_state.staging_texture[ready], 0);
                                    if(!logged_gpu_map_size)
                                    {
                                        logged_gpu_map_size = true;
                                        LOG_INFO("[ScreenCapture] DXGI mapped %dx%d BGRA from native %ux%u for '%s'",
                                                 (int)dxgi_state.staging_w, (int)dxgi_state.staging_h,
                                                 dxgi_state.width, dxgi_state.height, source_id.c_str());
                                    }
                                    store_latest(frame);
                                }
                                else
                                {
                                    dxgi_state.context->Unmap(dxgi_state.staging_texture[ready], 0);
                                    use_dxgi = false;
                                }
                            }
                        }
                        continue;
                    }
                }
                else
                {
                    if(dxgi_state.desktop_frame_acquired)
                        SafeDupReleaseFrame(dxgi_state.duplication, dxgi_state.desktop_frame_acquired);
                }
            }
            else
            {
                LOG_WARNING("[ScreenCapture] DXGI AcquireNextFrame returned unexpected hr=0x%08lx for '%s' (resource=%p), recreating duplication",
                            (unsigned long)hr, source_id.c_str(), (void*)resource);
                if(resource) { resource->Release(); resource = nullptr; }
                if(dxgi_state.desktop_frame_acquired)
                    SafeDupReleaseFrame(dxgi_state.duplication, dxgi_state.desktop_frame_acquired);
                dxgi_state.Release();
                use_dxgi = false;
            }
        }
        std::chrono::steady_clock::time_point now_tp = std::chrono::steady_clock::now();
            const int ms_since_last_gdi = (int)std::chrono::duration_cast<std::chrono::milliseconds>(now_tp - last_gdi_force).count();
            const bool should_force_gdi =
                allow_gdi && dxgi_timed_out && ms_since_last_gdi >= k_gdi_poll_after_dxgi_timeout_ms;
            if(dxgi_timed_out && !should_force_gdi)
            {
                if(dxgi_state.staging_ready >= 0 && dxgi_state.staging_texture[dxgi_state.staging_ready])
                {
                    D3D11_MAPPED_SUBRESOURCE mapped = {};
                    if(SUCCEEDED(dxgi_state.context->Map(dxgi_state.staging_texture[dxgi_state.staging_ready],
                                                          0, D3D11_MAP_READ, 0, &mapped)))
                    {
                        std::shared_ptr<CapturedFrame> frame = MakeCapturedFrameFromBgra(
                            (const uint8_t*)mapped.pData,
                            (int)dxgi_state.staging_w,
                            (int)dxgi_state.staging_h,
                            (int)mapped.RowPitch,
                            true,
                            false,
                            frame_counter++);
                        dxgi_state.context->Unmap(dxgi_state.staging_texture[dxgi_state.staging_ready], 0);
                        store_latest(frame);
                    }
                    dxgi_state.staging_ready = -1;
                }
                continue;
            }
            if(should_force_gdi)
            {
                last_gdi_force = now_tp;
            }
            if(allow_gdi && (!use_dxgi || should_force_gdi))
            {
                if(!use_dxgi && !logged_dxgi_unavailable && backend_mode == 0)
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
                        int out_w = 0;
                        int out_h = 0;
                        WorkingCaptureSize(dw, dh, target_width.load(), target_height.load(), out_w, out_h);
                        HBITMAP bmp = CreateCompatibleBitmap(screen_dc, out_w, out_h);
                        if(bmp)
                        {
                            HGDIOBJ old_obj = SelectObject(mem_dc, bmp);
                            SetStretchBltMode(mem_dc, HALFTONE);
                            SetBrushOrgEx(mem_dc, 0, 0, nullptr);
                            if(StretchBlt(mem_dc, 0, 0, out_w, out_h, screen_dc, dx, dy, dw, dh, SRCCOPY))
                            {
                                BITMAPINFO bmi = {};
                                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                                bmi.bmiHeader.biWidth = out_w;
                                bmi.bmiHeader.biHeight = -out_h;
                                bmi.bmiHeader.biPlanes = 1;
                                bmi.bmiHeader.biBitCount = 32;
                                bmi.bmiHeader.biCompression = BI_RGB;

                                std::vector<uint8_t> buffer((size_t)out_w * (size_t)out_h * 4);
                                if(GetDIBits(mem_dc, bmp, 0, (UINT)out_h, buffer.data(), &bmi, DIB_RGB_COLORS) > 0)
                                {
                                    store_latest(MakeCapturedFrameFromBgra(buffer.data(),
                                                                            out_w,
                                                                            out_h,
                                                                            out_w * 4,
                                                                            false,
                                                                            true,
                                                                            frame_counter++));
                                    frame_from_gdi = true;
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

            if(!frame_from_gdi)
            {
                if(!use_dxgi && backend_mode != 2)
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

        /* GDI (and Linux-style grab) are CPU-paced. DXGI is paced by AcquireNextFrame. */
        std::chrono::steady_clock::time_point frame_end = std::chrono::steady_clock::now();
        int elapsed = (int)std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start).count();
        int sleep_time = target_frame_time_ms - elapsed;
        if(sleep_time > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        }
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

        QRect geometry = screen->geometry();
        info.width = geometry.width();
        info.height = geometry.height();
        info.x = geometry.x();
        info.y = geometry.y();
        info.display_number = i + 1;
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

static QImage GrabScreenLinux(QScreen* screen)
{
    if(!screen)
    {
        return QImage();
    }
    QRect geometry = screen->geometry();
    return screen->grabWindow(0, geometry.x(), geometry.y(), geometry.width(), geometry.height()).toImage();
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

        QImage image = GrabScreenLinux(screen);
        if(image.isNull())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        int target_w = target_width.load();
        int target_h = target_height.load();
        if(image.width() != target_w || image.height() != target_h)
        {
            image = BoxScaleRgbaImage(image, target_w, target_h);
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

        std::chrono::steady_clock::time_point frame_end = std::chrono::steady_clock::now();
        int elapsed = (int)std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start).count();
        int sleep_time = target_frame_time_ms - elapsed;
        if(sleep_time > 0)
        {
            if(sleep_time < 1)
            {
                sleep_time = 1;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        }
    }
}

#endif
