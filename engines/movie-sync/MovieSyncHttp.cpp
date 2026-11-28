// SPDX-License-Identifier: GPL-2.0-or-later

#include "MovieSyncHttp.h"

#include <chrono>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <winhttp.h>
#endif

namespace MovieSync
{

#ifdef _WIN32

namespace
{

std::wstring Widen(const std::string& s)
{
    if(s.empty())
    {
        return std::wstring();
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w((std::size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

struct Handle
{
    HINTERNET h = nullptr;
    explicit Handle(HINTERNET v) : h(v) {}
    ~Handle()
    {
        if(h)
        {
            WinHttpCloseHandle(h);
        }
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};

} // namespace

bool NativeHttpsAvailable()
{
    return true;
}

void HttpJob::Cancel()
{
    std::lock_guard<std::mutex> lock(mutex_);
    cancelled_ = true;
    if(request_)
    {
        WinHttpCloseHandle((HINTERNET)request_);
        request_ = nullptr;
    }
}

bool DecodeImageNative(const std::vector<std::uint8_t>& bytes, int max_side, int* width, int* height,
                       std::vector<std::uint8_t>* bgra)
{
    if(bytes.empty() || bytes.size() > 0x7fffffff)
    {
        return false;
    }
    const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool ok = false;
    IWICImagingFactory* factory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICBitmapScaler* scaler = nullptr;
    IWICFormatConverter* converter = nullptr;
    do
    {
        if(FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
            break;
        if(FAILED(factory->CreateStream(&stream)))
            break;
        if(FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()), (DWORD)bytes.size())))
            break;
        if(FAILED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder)))
            break;
        if(FAILED(decoder->GetFrame(0, &frame)))
            break;
        UINT w = 0, h = 0;
        if(FAILED(frame->GetSize(&w, &h)) || w == 0 || h == 0)
            break;
        IWICBitmapSource* source = frame;
        if(max_side > 0 && (w > (UINT)max_side || h > (UINT)max_side))
        {
            const double k = (double)max_side / (double)(w > h ? w : h);
            const UINT sw = (UINT)(w * k) > 0 ? (UINT)(w * k) : 1;
            const UINT sh = (UINT)(h * k) > 0 ? (UINT)(h * k) : 1;
            if(FAILED(factory->CreateBitmapScaler(&scaler))
               || FAILED(scaler->Initialize(frame, sw, sh, WICBitmapInterpolationModeFant)))
                break;
            source = scaler;
            w = sw;
            h = sh;
        }
        if(FAILED(factory->CreateFormatConverter(&converter))
           || FAILED(converter->Initialize(source, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
                                           WICBitmapPaletteTypeCustom)))
            break;
        bgra->resize((std::size_t)w * h * 4);
        if(FAILED(converter->CopyPixels(nullptr, w * 4, (UINT)bgra->size(), bgra->data())))
            break;
        *width = (int)w;
        *height = (int)h;
        ok = true;
    } while(false);
    if(converter) converter->Release();
    if(scaler) scaler->Release();
    if(frame) frame->Release();
    if(decoder) decoder->Release();
    if(stream) stream->Release();
    if(factory) factory->Release();
    if(SUCCEEDED(init))
    {
        CoUninitialize();
    }
    if(!ok)
    {
        bgra->clear();
    }
    return ok;
}

bool HttpsGet(const std::string& url, std::vector<std::uint8_t>* out, std::string* error, std::size_t max_bytes,
              int total_ms, HttpJob* job)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(total_ms);
    const int timeout_ms = total_ms / 3 > 1000 ? total_ms / 3 : 1000; /* per phase */
    const auto fail = [error](const char* why) {
        if(error)
        {
            *error = why;
        }
        return false;
    };
    out->clear();
    if(url.rfind("https://", 0) != 0 || url.size() > 2048)
    {
        return fail("not an https link");
    }
    std::wstring wurl = Widen(url);
    URL_COMPONENTS uc = {};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {};
    std::wstring path(wurl.size() + 1, L'\0');
    uc.lpszHostName = host;
    uc.dwHostNameLength = 255;
    uc.lpszUrlPath = path.data();
    uc.dwUrlPathLength = (DWORD)path.size();
    std::wstring extra(wurl.size() + 1, L'\0');
    uc.lpszExtraInfo = extra.data();
    uc.dwExtraInfoLength = (DWORD)extra.size();
    if(!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &uc) || uc.nScheme != INTERNET_SCHEME_HTTPS)
    {
        return fail("bad link");
    }
    const std::wstring object = std::wstring(uc.lpszUrlPath, uc.dwUrlPathLength)
                                + std::wstring(uc.lpszExtraInfo, uc.dwExtraInfoLength);

    Handle session(WinHttpOpen(L"OpenRGB-MovieSync", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS, 0));
    if(!session.h)
    {
        session.h = WinHttpOpen(L"OpenRGB-MovieSync", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                WINHTTP_NO_PROXY_BYPASS, 0);
    }
    if(!session.h)
    {
        return fail("no HTTP session");
    }
    WinHttpSetTimeouts(session.h, timeout_ms, timeout_ms, timeout_ms, timeout_ms);
    Handle connect(WinHttpConnect(session.h, std::wstring(uc.lpszHostName, uc.dwHostNameLength).c_str(), uc.nPort, 0));
    if(!connect.h)
    {
        return fail("cannot reach the server");
    }
    Handle request(WinHttpOpenRequest(connect.h, L"GET", object.c_str(), nullptr, WINHTTP_NO_REFERER,
                                      WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if(!request.h)
    {
        return fail("cannot make the request");
    }
    /* Hand the request to the job so Cancel() can close it; take it back before it is closed
       here (or forget it if Cancel() already closed it). */
    struct Detach
    {
        HttpJob* job;
        Handle& request;
        ~Detach()
        {
            if(!job)
            {
                return;
            }
            std::lock_guard<std::mutex> lock(job->mutex_);
            if(job->request_)
            {
                job->request_ = nullptr; /* still ours: closed by its Handle */
            }
            else
            {
                request.h = nullptr; /* Cancel() closed it */
            }
        }
    } detach{job, request};
    if(job)
    {
        std::lock_guard<std::mutex> lock(job->mutex_);
        if(job->cancelled_)
        {
            job = nullptr; /* nothing handed over: the Handle closes it */
            return fail("cancelled");
        }
        job->request_ = request.h;
    }
    /* Never follow a redirect from https down to http (the default policy, made explicit). */
    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    WinHttpSetOption(request.h, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));
    const wchar_t* headers = L"Accept: image/jpeg,image/png,image/gif;q=0.8,*/*;q=0.1\r\n";
    if(!WinHttpSendRequest(request.h, headers, (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
       || !WinHttpReceiveResponse(request.h, nullptr))
    {
        return fail("no answer");
    }
    DWORD status = 0;
    DWORD size = sizeof(status);
    WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &size, WINHTTP_NO_HEADER_INDEX);
    if(status != 200)
    {
        if(error)
        {
            *error = "HTTP " + std::to_string(status);
        }
        return false;
    }
    for(;;)
    {
        if((job && job->Cancelled()) || std::chrono::steady_clock::now() > deadline)
        {
            out->clear();
            return fail(job && job->Cancelled() ? "cancelled" : "took too long");
        }
        DWORD avail = 0;
        if(!WinHttpQueryDataAvailable(request.h, &avail))
        {
            return fail("download broke off");
        }
        if(avail == 0)
        {
            break;
        }
        if(out->size() + avail > max_bytes)
        {
            out->clear();
            return fail("image too large");
        }
        const std::size_t at = out->size();
        out->resize(at + avail);
        DWORD got = 0;
        if(!WinHttpReadData(request.h, out->data() + at, avail, &got))
        {
            out->clear();
            return fail("download broke off");
        }
        out->resize(at + got);
    }
    return !out->empty() || fail("empty answer");
}

#else

bool NativeHttpsAvailable()
{
    return false;
}

void HttpJob::Cancel()
{
    cancelled_ = true;
}

bool DecodeImageNative(const std::vector<std::uint8_t>&, int, int*, int*, std::vector<std::uint8_t>*)
{
    return false;
}

bool HttpsGet(const std::string&, std::vector<std::uint8_t>* out, std::string* error, std::size_t, int, HttpJob*)
{
    out->clear();
    if(error)
    {
        *error = "unsupported";
    }
    return false;
}

#endif

} // namespace MovieSync
