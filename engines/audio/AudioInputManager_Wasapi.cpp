// SPDX-License-Identifier: GPL-2.0-only

#include "AudioInputManager.h"

#include <algorithm>
#include <thread>
#include <functional>
#include <vector>
#include <atomic>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>
#include <Functiondiscoverykeys_devpkey.h>
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Avrt.lib")
#pragma comment(lib, "Mmdevapi.lib")
#endif

#ifdef _WIN32
class AudioInputManager::WasapiCapturer
{
public:
    WasapiCapturer(AudioInputManager* mgr, const QString& dev, bool use_loopback)
        : manager(mgr), dev_id(dev), loopback(use_loopback)
    {
        thread = std::thread([this](){ run(); });
    }
    ~WasapiCapturer()
    {
        stopping = true;
        if(thread.joinable()) thread.join();
    }
private:
    void run()
    {
        HRESULT coinithr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        IMMDeviceEnumerator* enumerator = nullptr;
        IMMDevice* device = nullptr;
        IAudioClient* client = nullptr;
        IAudioCaptureClient* capture = nullptr;

        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                      __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
        if(FAILED(hr)) { if(SUCCEEDED(coinithr)) CoUninitialize(); return; }
        if(dev_id.isEmpty())
            hr = enumerator->GetDefaultAudioEndpoint(loopback ? eRender : eCapture, eMultimedia, &device);
        else
            hr = enumerator->GetDevice(reinterpret_cast<LPCWSTR>(dev_id.utf16()), &device);
        if(FAILED(hr)) { enumerator->Release(); if(SUCCEEDED(coinithr)) CoUninitialize(); return; }
        hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client));
        if(FAILED(hr)) { device->Release(); enumerator->Release(); if(SUCCEEDED(coinithr)) CoUninitialize(); return; }

        WAVEFORMATEX* mix = nullptr;
        client->GetMixFormat(&mix);
        hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                (loopback ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0),
                                0, 0, mix, nullptr);
        if(FAILED(hr)) { CoTaskMemFree(mix); client->Release(); device->Release(); enumerator->Release(); if(SUCCEEDED(coinithr)) CoUninitialize(); return; }

        hr = client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&capture));
        if(FAILED(hr)) { client->Release(); device->Release(); enumerator->Release(); CoTaskMemFree(mix); if(SUCCEEDED(coinithr)) CoUninitialize(); return; }

        hr = client->Start();
        if(FAILED(hr)) { capture->Release(); client->Release(); device->Release(); enumerator->Release(); CoTaskMemFree(mix); if(SUCCEEDED(coinithr)) CoUninitialize(); return; }

        const WAVEFORMATEXTENSIBLE* mix_ext =
            (mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
                ? reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(mix)
                : nullptr;
        const bool isFloat = (mix->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
                             (mix_ext && mix_ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
        const bool isPCM   = (mix->wFormatTag == WAVE_FORMAT_PCM) ||
                             (mix_ext && mix_ext->SubFormat == KSDATAFORMAT_SUBTYPE_PCM);
        int bitsPerSample  = mix->wBitsPerSample;
        if(mix_ext)
        {
            WORD vbits = mix_ext->Samples.wValidBitsPerSample;
            if(vbits) bitsPerSample = vbits;
        }
        const int channels = mix->nChannels;
        manager->setSampleRate((int)mix->nSamplesPerSec);

        while(!stopping)
        {
            UINT32 packetFrames = 0;
            hr = capture->GetNextPacketSize(&packetFrames);
            if(FAILED(hr)) break;
            if(packetFrames == 0)
            {
                Sleep(5);
                continue;
            }
            BYTE* data = nullptr; UINT32 frames; DWORD flags;
            hr = capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
            if(FAILED(hr)) break;
            std::vector<int16_t> mono;
            mono.resize(frames);
            if(isFloat)
            {
                const float* f = reinterpret_cast<const float*>(data);
                for(UINT32 i=0;i<frames;i++)
                {
                    double sum = 0.0;
                    for(int c=0;c<channels;c++)
                        sum += f[i*channels + c];
                    double acc = (channels > 0) ? (sum / (double)channels) : sum;
                    if(acc > 1.0) acc = 1.0; if(acc < -1.0) acc = -1.0;
                    mono[i] = (int16_t)(acc * 32767.0);
                }
            }
            else if(isPCM && bitsPerSample == 16)
            {
                const int16_t* s = reinterpret_cast<const int16_t*>(data);
                for(UINT32 i=0;i<frames;i++)
                {
                    long long sum = 0;
                    for(int c=0;c<channels;c++)
                        sum += s[i*channels + c];
                    mono[i] = (int16_t)(sum / std::max(1, channels));
                }
            }
            else if(isPCM && bitsPerSample >= 24)
            {
                const int32_t* s32 = reinterpret_cast<const int32_t*>(data);
                for(UINT32 i=0;i<frames;i++)
                {
                    long long sum = 0;
                    for(int c=0;c<channels;c++)
                        sum += s32[i*channels + c];
                    double acc = (double)sum / (double)std::max(1, channels);
                    int v = (int)(acc / 2147483648.0 * 32767.0);
                    if(v > 32767) v = 32767; if(v < -32768) v = -32768;
                    mono[i] = (int16_t)v;
                }
            }
            else
            {
                const int16_t* s = reinterpret_cast<const int16_t*>(data);
                for(UINT32 i=0;i<frames;i++)
                {
                    long long sum = 0;
                    for(int c=0;c<channels;c++)
                        sum += s[i*channels + c];
                    mono[i] = (int16_t)(sum / std::max(1, channels));
                }
            }
            manager->FeedPCM16(mono.data(), (int)mono.size());
            capture->ReleaseBuffer(frames);
        }

        client->Stop();
        capture->Release();
        client->Release();
        device->Release();
        enumerator->Release();
        CoTaskMemFree(mix);
        if(SUCCEEDED(coinithr)) CoUninitialize();
    }
    std::thread thread;
    std::atomic<bool> stopping{false};
    AudioInputManager* manager;
    QString dev_id;
    bool loopback = true;
};
#endif // _WIN32

QStringList AudioInputManager::listInputDevices()
{
    QStringList names;
#ifdef _WIN32
    device_names.clear();
    device_ids.clear();
    device_is_loopback.clear();
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT coinithr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator));
    if(FAILED(hr))
    {
        if(SUCCEEDED(coinithr)) CoUninitialize();
        return names;
    }
    IMMDeviceCollection* coll = nullptr;
    hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &coll);
    if(SUCCEEDED(hr) && coll)
    {
        UINT count = 0; coll->GetCount(&count);
        for(UINT i=0;i<count;i++)
        {
            IMMDevice* dev = nullptr; coll->Item(i, &dev);
            if(!dev) continue;
            LPWSTR id = nullptr; dev->GetId(&id);
            IPropertyStore* props = nullptr;
            if(SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props)))
            {
                PROPVARIANT varName; PropVariantInit(&varName);
                props->GetValue(PKEY_Device_FriendlyName, &varName);
                QString qname = QString::fromWCharArray(varName.pwszVal ? varName.pwszVal : L"(Unknown)") + " (Loopback)";
                names << qname;
                device_names << qname;
                device_ids.push_back(QString::fromWCharArray(id));
                device_is_loopback.push_back(true);
                PropVariantClear(&varName);
                props->Release();
            }
            CoTaskMemFree(id);
            dev->Release();
        }
        coll->Release();
    }
    coll = nullptr;
    hr = enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &coll);
    if(SUCCEEDED(hr) && coll)
    {
        UINT count = 0; coll->GetCount(&count);
        for(UINT i=0;i<count;i++)
        {
            IMMDevice* dev = nullptr; coll->Item(i, &dev);
            if(!dev) continue;
            LPWSTR id = nullptr; dev->GetId(&id);
            IPropertyStore* props = nullptr;
            if(SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props)))
            {
                PROPVARIANT varName; PropVariantInit(&varName);
                props->GetValue(PKEY_Device_FriendlyName, &varName);
                QString qname = QString::fromWCharArray(varName.pwszVal ? varName.pwszVal : L"(Unknown)");
                names << qname;
                device_names << qname;
                device_ids.push_back(QString::fromWCharArray(id));
                device_is_loopback.push_back(false);
                PropVariantClear(&varName);
                props->Release();
            }
            CoTaskMemFree(id);
            dev->Release();
        }
        coll->Release();
    }
    if(enumerator) enumerator->Release();
    if(SUCCEEDED(coinithr)) CoUninitialize();
#endif
    return names;
}

void AudioInputManager::start()
{
    QMutexLocker lock(&mutex);
    if(running) return;

#ifdef _WIN32
    if(selected_index < 0 || selected_index >= (int)device_ids.size()) return;
    if(capturer) { delete capturer; capturer = nullptr; }
    if(selected_index < 0 || selected_index >= (int)device_ids.size())
    {
        return;
    }
    QString devId = device_ids[selected_index];
    bool is_loop = (selected_index < (int)device_is_loopback.size()) ? device_is_loopback[selected_index] : true;
    capturer = new WasapiCapturer(this, devId, is_loop);
    running = true;
    resetAutoLevel();
    level_timer.start();
    return;
#else
    running = true;
    resetAutoLevel();
    level_timer.start();
#endif
}

void AudioInputManager::stop()
{
    QMutexLocker lock(&mutex);
    if(!running) return;

    level_timer.stop();

#ifdef _WIN32
    if(capturer)
    {
        delete capturer;
        capturer = nullptr;
    }
#endif
    running = false;
    current_level.store(0.0f);
    resetAutoLevel();
    {
        QMutexLocker bl(&bands_mutex);
        std::fill(bands16.begin(), bands16.end(), 0.0f);
        note_peak_count = 0;
        note_drive = 0.0f;
        for(int i = 0; i < kMaxNotePeaks; ++i)
            note_peaks[i] = {};
        if((int)chroma12.size() == kChromaBins)
            std::fill(chroma12.begin(), chroma12.end(), 0.0f);
    }
}
