// SPDX-License-Identifier: GPL-2.0-only

#include "AudioInputManager.h"
#include <cmath>
#include <complex>
#include <algorithm>
#include <thread>
#include <functional>

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

static constexpr float PI_F = 3.14159265358979323846f;

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
        manager->channel_count = channels;
        manager->channel_levels.assign(channels, 0.0f);
        manager->channel_names.clear();
        static const char* defaultNames[8] = {"FL","FR","FC","LFE","BL","BR","SL","SR"};
        for(int ci=0; ci<channels; ++ci)
        {
            if(ci < 8) manager->channel_names << defaultNames[ci];
            else manager->channel_names << QString("Ch%1").arg(ci+1);
        }

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
            std::vector<double> channel_accum;
            if(channels > 0)
            {
                channel_accum.assign(channels, 0.0);
            }
            if(isFloat)
            {
                const float* f = reinterpret_cast<const float*>(data);
                for(UINT32 i=0;i<frames;i++)
                {
                    double sum = 0.0;
                    for(int c=0;c<channels;c++)
                    {
                        double sample = f[i*channels + c];
                        sum += sample;
                        if(c < (int)channel_accum.size())
                        {
                            channel_accum[c] += sample * sample;
                        }
                    }
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
                    {
                        int16_t sample16 = s[i*channels + c];
                        sum += sample16;
                        if(c < (int)channel_accum.size())
                        {
                            double sample = sample16 / 32768.0;
                            channel_accum[c] += sample * sample;
                        }
                    }
                    int divisor = std::max(1, channels);
                    int16_t v = (int16_t)(sum / divisor);
                    mono[i] = v;
                }
            }
            else if(isPCM && bitsPerSample >= 24)
            {
                const int32_t* s32 = reinterpret_cast<const int32_t*>(data);
                for(UINT32 i=0;i<frames;i++)
                {
                    long long sum = 0;
                    for(int c=0;c<channels;c++)
                    {
                        int32_t sample32 = s32[i*channels + c];
                        sum += sample32;
                        if(c < (int)channel_accum.size())
                        {
                            double sample = (double)sample32 / 2147483648.0;
                            channel_accum[c] += sample * sample;
                        }
                    }
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
                    {
                        int16_t sample16 = s[i*channels + c];
                        sum += sample16;
                        if(c < (int)channel_accum.size())
                        {
                            double sample = sample16 / 32768.0;
                            channel_accum[c] += sample * sample;
                        }
                    }
                    int divisor = std::max(1, channels);
                    mono[i] = (int16_t)(sum / divisor);
                }
            }
            std::vector<float> channel_levels_local;
            if(!channel_accum.empty() && frames > 0)
            {
                channel_levels_local.resize(channel_accum.size());
                for(size_t ci = 0; ci < channel_accum.size(); ++ci)
                {
                    double avg = channel_accum[ci] / (double)frames;
                    if(avg < 0.0)
                    {
                        avg = 0.0;
                    }
                    channel_levels_local[ci] = (avg > 0.0) ? (float)std::sqrt(avg) : 0.0f;
                }
            }
            manager->FeedPCM16(mono.data(), (int)mono.size());
            if(!channel_levels_local.empty())
            {
                manager->updateChannelLevels(channel_levels_local);
            }
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

static AudioInputManager* g_audio_instance = nullptr;

AudioInputManager* AudioInputManager::instance()
{
    if(!g_audio_instance)
    {
        g_audio_instance = new AudioInputManager();
    }
    return g_audio_instance;
}

AudioInputManager::AudioInputManager(QObject* parent)
    : QObject(parent)
{
    level_timer.setInterval(33);
    connect(&level_timer, &QTimer::timeout, this, &AudioInputManager::onLevelTick);
    sample_buffer.reserve(fft_size * 4);
    bands16.assign(bands_count, 0.0f);
    eq_gain.assign(bands_count, 1.0f);
    resetAutoLevel();
}

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

int AudioInputManager::defaultDeviceIndex() const
{
    QMutexLocker lock(&mutex);
    return 0;
}

void AudioInputManager::setDeviceByIndex(int index)
{
    bool was_running = false;
    {
        QMutexLocker lock(&mutex);
        if(index < 0) return;
        selected_index = index;
        was_running = running;
    }
    if(was_running)
    {
        stop();
        start();
    }
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
        bass_level = mid_level = treble_level = 0.0f;
    }
}

void AudioInputManager::setGain(float g)
{
    if(g < 0.05f) g = 0.05f;
    if(g > 100.0f) g = 100.0f;
    gain = g;
}

void AudioInputManager::setAutoLevelEnabled(bool enabled)
{
    auto_level_enabled = enabled;
    if(auto_level_enabled)
    {
        resetAutoLevel();
    }
}

void AudioInputManager::resetAutoLevel()
{
    auto_level_peak = auto_level_min_peak;
    auto_level_floor = auto_level_min_peak * 0.25f;
    if(auto_level_floor < 1e-6f)
    {
        auto_level_floor = 1e-6f;
    }
    if(auto_level_floor > auto_level_peak * 0.9f)
    {
        auto_level_floor = auto_level_peak * 0.9f;
    }
}

void AudioInputManager::setSmoothing(float s)
{
    if(s < 0.0f) s = 0.0f;
    if(s > 0.99f) s = 0.99f;
    ema_smoothing = s;
}

namespace
{
float clampDecay(float d, float lo, float hi)
{
    return std::clamp(d, lo, hi);
}
} // namespace

void AudioInputManager::setBandPeakDecay(float d)
{
    band_peak_decay = clampDecay(d, 0.90f, 0.9999f);
}

void AudioInputManager::setBassPeakDecay(float d)
{
    bass_peak_decay = clampDecay(d, 0.90f, 0.9999f);
}

void AudioInputManager::setActivityPeakDecay(float d)
{
    activity_peak_decay = clampDecay(d, 0.90f, 0.9999f);
}

void AudioInputManager::setVisualizerPeakDecay(float d)
{
    visualizer_peak_decay = clampDecay(d, 0.80f, 0.995f);
}

void AudioInputManager::setVisualizerFloor(float f)
{
    visualizer_floor = std::clamp(f, 1e-6f, 0.01f);
}

void AudioInputManager::setAutoLevelPeakDecay(float d)
{
    auto_level_peak_decay = clampDecay(d, 0.90f, 0.9999f);
}

void AudioInputManager::setAutoLevelFloorDecay(float d)
{
    auto_level_floor_decay = clampDecay(d, 0.90f, 0.9999f);
}

void AudioInputManager::resetAnalyzerTuning()
{
    ema_smoothing = 0.8f;
    band_peak_decay = 0.994f;
    bass_peak_decay = 0.998f;
    activity_peak_decay = 0.992f;
    visualizer_peak_decay = 0.92f;
    visualizer_floor = 1e-4f;
    auto_level_enabled = true;
    auto_level_peak_decay = 0.995f;
    auto_level_floor_decay = 0.9995f;
    resetAutoLevel();
}

void AudioInputManager::setMixClarity(float clarity_0_to_1)
{
    mix_clarity = std::clamp(clarity_0_to_1, 0.0f, 1.0f);
}

float AudioInputManager::getMixClarity() const
{
    return mix_clarity;
}

void AudioInputManager::setBandIsolation(float isolation_0_to_1)
{
    band_isolation = std::clamp(isolation_0_to_1, 0.0f, 1.0f);
}

float AudioInputManager::getBandIsolation() const
{
    return band_isolation;
}

int AudioInputManager::getEqBandCount() const
{
    return bands_count;
}

void AudioInputManager::ensureEqGainSizeLocked()
{
    if((int)eq_gain.size() != bands_count)
    {
        std::vector<float> old = eq_gain;
        eq_gain.assign(bands_count, 1.0f);
        if(!old.empty() && bands_count > 0)
        {
            for(int b = 0; b < bands_count; ++b)
            {
                const float t = ((float)b + 0.5f) / (float)bands_count;
                const float src = t * (float)old.size() - 0.5f;
                const int i0 = std::clamp((int)std::floor(src), 0, (int)old.size() - 1);
                const int i1 = std::min(i0 + 1, (int)old.size() - 1);
                const float frac = src - (float)i0;
                eq_gain[(size_t)b] = old[(size_t)i0] * (1.0f - frac) + old[(size_t)i1] * frac;
            }
        }
    }
}

namespace {

void SampleReferenceEqCurve(const float* ref, int ref_count, int bands_count, std::vector<float>& out)
{
    out.assign(bands_count, 1.0f);
    if(ref_count <= 0 || bands_count <= 0 || !ref)
    {
        return;
    }
    for(int b = 0; b < bands_count; ++b)
    {
        const float t = ((float)b + 0.5f) / (float)bands_count;
        const float ri = t * (float)ref_count - 0.5f;
        const int i0 = std::clamp((int)std::floor(ri), 0, ref_count - 1);
        const int i1 = std::min(i0 + 1, ref_count - 1);
        const float frac = std::clamp(ri - (float)i0, 0.0f, 1.0f);
        out[(size_t)b] = ref[i0] * (1.0f - frac) + ref[i1] * frac;
    }
}

} // namespace

float AudioInputManager::AnalysisBandCenterHz(int band_index, int bands_count, int sample_rate_hz, int fft_size_local)
{
    float low = 0.0f;
    float high = 0.0f;
    AnalysisBandHzRange(band_index, bands_count, sample_rate_hz, fft_size_local, low, high);
    return std::sqrt(low * high);
}

void AudioInputManager::AnalysisBandHzRange(int band_index,
                                            int bands_count,
                                            int sample_rate_hz,
                                            int fft_size_local,
                                            float& low_hz,
                                            float& high_hz)
{
    low_hz = 20.0f;
    high_hz = 20000.0f;
    if(band_index < 0 || bands_count <= 0 || band_index >= bands_count)
    {
        return;
    }
    float fs = (float)std::max(1, sample_rate_hz);
    int fft_n = std::max(512, fft_size_local);
    float bin_min = fs / (float)fft_n;
    float f_min = std::max(20.0f, bin_min);
    float f_max = fs * 0.5f;
    if(f_max <= f_min)
    {
        f_max = f_min + 1.0f;
    }
    const float ratio = f_max / f_min;
    const float t0 = (float)band_index / (float)bands_count;
    const float t1 = (float)(band_index + 1) / (float)bands_count;
    low_hz = f_min * std::pow(ratio, t0);
    high_hz = f_min * std::pow(ratio, t1);
}

void AudioInputManager::applyEqMixPreset(int preset_id)
{
    static const float kFlat[REFERENCE_EQ_BANDS] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    };
    static const float kKick[REFERENCE_EQ_BANDS] = {
        0.05f, 0.12f, 0.35f, 2.0f, 2.0f, 1.35f, 0.12f, 0.08f, 0.06f, 0.05f, 0.05f, 0.05f, 0.05f, 0.05f, 0.05f, 0.05f,
    };
    static const float kSnare[REFERENCE_EQ_BANDS] = {
        0.05f, 0.08f, 0.12f, 0.25f, 0.45f, 2.0f, 2.0f, 1.65f, 1.2f, 0.55f, 0.25f, 0.15f, 0.12f, 0.10f, 0.10f, 0.10f,
    };
    static const float kHiHat[REFERENCE_EQ_BANDS] = {
        0.05f, 0.05f, 0.05f, 0.06f, 0.08f, 0.10f, 0.12f, 0.18f, 0.28f, 0.55f, 1.35f, 2.0f, 2.0f, 1.65f, 1.2f, 0.9f,
    };
    static const float kBassNoHum[REFERENCE_EQ_BANDS] = {
        0.02f, 0.05f, 0.18f, 0.55f, 1.35f, 1.65f, 1.2f, 0.85f, 0.55f, 0.35f, 0.22f, 0.15f, 0.12f, 0.10f, 0.10f, 0.10f,
    };
    static const float kDrumKit[REFERENCE_EQ_BANDS] = {
        0.08f, 0.15f, 0.45f, 1.85f, 1.65f, 1.45f, 1.55f, 1.25f, 0.75f, 0.55f, 1.15f, 1.65f, 1.45f, 1.1f, 0.85f, 0.7f,
    };
    static const float kMudCut[REFERENCE_EQ_BANDS] = {
        0.55f, 0.65f, 0.85f, 1.1f, 0.35f, 0.25f, 0.22f, 0.35f, 0.75f, 1.0f, 1.1f, 1.0f, 0.95f, 0.9f, 0.85f, 0.8f,
    };
    static const float kVocalReduce[REFERENCE_EQ_BANDS] = {
        0.85f, 0.9f, 1.0f, 0.75f, 0.45f, 0.35f, 0.32f, 0.35f, 0.42f, 0.55f, 0.75f, 1.0f, 1.1f, 1.15f, 1.1f, 1.0f,
    };
    static const float kStreaming[REFERENCE_EQ_BANDS] = {
        0.06f, 0.12f, 0.55f, 1.75f, 1.55f, 1.45f, 1.65f, 1.35f, 0.85f, 0.65f, 1.25f, 1.75f, 1.55f, 1.25f, 0.95f, 0.75f,
    };

    const float* table = kFlat;
    switch(preset_id)
    {
    case 1:
        table = kKick;
        break;
    case 2:
        table = kSnare;
        break;
    case 3:
        table = kHiHat;
        break;
    case 4:
        table = kBassNoHum;
        break;
    case 5:
        table = kDrumKit;
        break;
    case 6:
        table = kMudCut;
        break;
    case 7:
        table = kVocalReduce;
        break;
    case 8:
        table = kStreaming;
        break;
    default:
        table = kFlat;
        break;
    }

    QMutexLocker bl(&bands_mutex);
    ensureEqGainSizeLocked();
    SampleReferenceEqCurve(table, REFERENCE_EQ_BANDS, bands_count, eq_gain);
    for(float& g : eq_gain)
    {
        g = std::clamp(g, 0.0f, 2.0f);
    }
}

void AudioInputManager::setBandsCount(int bands)
{
    if(bands != 8 && bands != 16 && bands != 32)
    {
        bands = 16;
    }
    QMutexLocker bl(&bands_mutex);
    bands_count = bands;
    band_peak_smoothed.assign(bands_count, 0.1f);
    ensureEqGainSizeLocked();
    bands16.assign(bands_count, 0.0f);
    prev_band_frame.assign(bands_count, 0.0f);
    band_slow.assign(bands_count, 0.0f);
    band_flux.assign(bands_count, 0.0f);
    band_transient.assign(bands_count, 0.0f);
    band_noise_floor.assign(bands_count, 0.0f);
    band_peak_activity.assign(bands_count, 0.05f);
}

void AudioInputManager::setFFTSize(int n)
{
    if(n < 512)
    {
        n = 512;
    }
    if(n > 8192)
    {
        n = 8192;
    }
    int p = 1;
    while(p < n)
    {
        p <<= 1;
    }
    int prev = p >> 1;
    if(prev < 512)
    {
        prev = 512;
    }
    int chosen = (p - n) < (n - prev) ? p : prev;
    if(chosen < 512)
    {
        chosen = 512;
    }
    if(chosen > 8192)
    {
        chosen = 8192;
    }
    QMutexLocker bl(&bands_mutex);
    if(chosen == fft_size)
    {
        return;
    }
    fft_size = chosen;
    band_peak_smoothed.assign(bands_count, 0.1f);
    sample_buffer.clear();
    window.clear();
    prev_mags.clear();
    prev_band_frame.assign(bands_count, 0.0f);
    band_slow.assign(bands_count, 0.0f);
    band_flux.assign(bands_count, 0.0f);
    band_transient.assign(bands_count, 0.0f);
    band_noise_floor.assign(bands_count, 0.0f);
    band_peak_activity.assign(bands_count, 0.05f);
}

void AudioInputManager::setCrossovers(float bass_upper_hz, float mid_upper_hz)
{
    if(bass_upper_hz < 20.0f) bass_upper_hz = 20.0f;
    if(mid_upper_hz <= bass_upper_hz) mid_upper_hz = bass_upper_hz + 1.0f;
    xover_bass_upper = bass_upper_hz;
    xover_mid_upper  = mid_upper_hz;
}

int AudioInputManager::getBandsCount() const
{
    return bands_count;
}



void AudioInputManager::processBuffer(const char* data, int bytes)
{
    if(bytes <= 0)
    {
        return;
    }

    QMutexLocker bl(&bands_mutex);

    int16_t const* samples = reinterpret_cast<const int16_t*>(data);
    int count = bytes / (int)sizeof(int16_t);
    if(count <= 0) return;

    double sum = 0.0;
    for(int i = 0; i < count; i++)
    {
        double s = samples[i] / 32768.0;
        float scaled = (float)(s * gain);
        float limited = (std::abs(scaled) >= 1.0f) ? (scaled > 0 ? 1.0f : -1.0f)
                       : (float)std::tanh((double)scaled);
        sample_buffer.push_back(limited);
        double d = (double)limited;
        sum += d * d;
    }
    double rms = std::sqrt(sum / std::max(1, count));
    double val = rms;

    if(auto_level_enabled)
    {
        if(val > auto_level_peak)
        {
            auto_level_peak = static_cast<float>(val);
        }
        else
        {
            auto_level_peak *= auto_level_peak_decay;
            if(auto_level_peak < auto_level_min_peak)
            {
                auto_level_peak = auto_level_min_peak;
            }
        }

        if(auto_level_floor <= 0.0f || auto_level_floor > auto_level_peak)
        {
            auto_level_floor = auto_level_peak * 0.25f;
        }

        float target_floor = (float)val;
        if(target_floor < auto_level_floor)
        {
            auto_level_floor = auto_level_floor * auto_level_floor_decay +
                               target_floor * (1.0f - auto_level_floor_decay);
        }
        else
        {
            auto_level_floor = auto_level_floor +
                               (target_floor - auto_level_floor) * auto_level_floor_rise;
        }

        if(auto_level_floor < 1e-6f)
        {
            auto_level_floor = 1e-6f;
        }
        if(auto_level_floor > auto_level_peak * 0.9f)
        {
            auto_level_floor = auto_level_peak * 0.9f;
        }

        float range = auto_level_peak - auto_level_floor;
        if(range < auto_level_min_range)
        {
            range = auto_level_min_range;
        }

        double adjusted = (val - auto_level_floor) / (double)range;
        if(adjusted < 0.0)
        {
            adjusted = 0.0;
        }
        val = adjusted;
    }

    if(val > 1.0) val = 1.0;

    float prev = current_level.load();
    float out = (float)(ema_smoothing * prev + (1.0 - ema_smoothing) * val);
    if(out < 0.0f) out = 0.0f; if(out > 1.0f) out = 1.0f;
    current_level.store(out);

    if((int)sample_buffer.size() >= fft_size)
    {
        computeSpectrum();
        int keep = fft_size / 2;
        if(keep < 1) keep = 1;
        if((int)sample_buffer.size() > keep)
        {
            sample_buffer.erase(sample_buffer.begin(), sample_buffer.end() - keep);
        }
    }
}

void AudioInputManager::updateChannelLevels(const std::vector<float>& levels)
{
#ifdef _WIN32
    if(levels.empty())
    {
        return;
    }
    QMutexLocker lock(&mutex);
    if(channel_levels.size() != levels.size())
    {
        channel_levels.assign(levels.size(), 0.0f);
    }
    float range = auto_level_peak - auto_level_floor;
    if(range < auto_level_min_range)
    {
        range = auto_level_min_range;
    }
    for(size_t i = 0; i < levels.size(); ++i)
    {
        if(i >= channel_levels.size())
        {
            break;
        }
        double value = (double)levels[i] * (double)gain;
        double normalized = (value - auto_level_floor) / (double)range;
        if(normalized < 0.0)
        {
            normalized = 0.0;
        }
        else if(normalized > 1.0)
        {
            normalized = 1.0;
        }
        channel_levels[i] = 0.7f * channel_levels[i] + 0.3f * (float)normalized;
    }
    if(levels.size() >= 2)
    {
        const float left = channel_levels[0];
        const float right = channel_levels[1];
        const float mono = left + right + 1e-4f;
        const float width = std::abs(left - right) / mono;
        stereo_width = 0.88f * stereo_width + 0.12f * std::clamp(width, 0.0f, 1.0f);
    }
#else
    (void)levels;
#endif
}

void AudioInputManager::onLevelTick()
{
    emit LevelUpdated(current_level.load());
}

void AudioInputManager::FeedPCM16(const int16_t* samples, int count)
{
    if(!samples || count <= 0) return;
    processBuffer(reinterpret_cast<const char*>(samples), count * (int)sizeof(int16_t));
}

void AudioInputManager::ensureWindow()
{
    if((int)window.size() == fft_size) return;
    window.resize(fft_size);
    for(int i = 0; i < fft_size; i++)
    {
        window[i] = 0.5f * (1.0f - std::cos(2.0f * PI_F * i / (fft_size - 1)));
    }
}

static void fft_cooley_tukey(std::vector<std::complex<float>>& a)
{
    const size_t n = a.size();
    size_t j = 0;
    for(size_t i = 1; i < n; i++)
    {
        size_t bit = n >> 1;
        for(; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if(i < j) std::swap(a[i], a[j]);
    }
    for(size_t len = 2; len <= n; len <<= 1)
    {
        float ang = -2.0f * PI_F / (float)len;
        std::complex<float> wlen(std::cos(ang), std::sin(ang));
        for(size_t i = 0; i < n; i += len)
        {
            std::complex<float> w(1.0f, 0.0f);
            for(size_t k = 0; k < len/2; k++)
            {
                std::complex<float> u = a[i + k];
                std::complex<float> v = a[i + k + len/2] * w;
                a[i + k] = u + v;
                a[i + k + len/2] = u - v;
                w *= wlen;
            }
        }
    }
}

void AudioInputManager::computeSpectrum()
{
    ensureWindow();
    if((int)sample_buffer.size() < fft_size) return;
    std::vector<std::complex<float>> buf(fft_size);
    int start = (int)sample_buffer.size() - fft_size;
    for(int i = 0; i < fft_size; i++)
    {
        float s = sample_buffer[start + i] * window[i];
        buf[i] = std::complex<float>(s, 0.0f);
    }
    fft_cooley_tukey(buf);
    int n2 = fft_size / 2;
    std::vector<float> mags(n2);
    for(int i = 0; i < n2; i++)
    {
        float m = std::abs(buf[i]) / (fft_size * 0.5f);
        mags[i] = m;
    }

    float fs = (float)sample_rate_hz;
    float bin_min = (fft_size > 0) ? (fs / (float)fft_size) : 1.0f;
    float f_min = std::max(1.0f, bin_min);
    float f_max = fs * 0.5f;
    if(f_max <= f_min || f_max < 0.001f)
    {
        return;
    }
    std::vector<float> eq_copy;
    {
        QMutexLocker bl(&bands_mutex);
        ensureEqGainSizeLocked();
        eq_copy = eq_gain;
    }
    std::vector<float> newBands(bands_count, 0.0f);
    for(int b = 0; b < bands_count; b++)
    {
        float t0 = (float)b / (float)bands_count;
        float t1 = (float)(b + 1) / (float)bands_count;
        float ratio = f_max / f_min;
        float fb0 = f_min * std::pow(ratio, t0);
        float fb1 = f_min * std::pow(ratio, t1);
        int i0 = (int)std::floor(fb0 * n2 / f_max);
        int i1 = (int)std::ceil (fb1 * n2 / f_max);
        if(i0 < 1) i0 = 1; if(i1 <= i0) i1 = i0 + 1; if(i1 >= n2) i1 = n2 - 1;
        float accum = 0.0f; int cnt = 0;
        for(int i = i0; i < i1; i++) { accum += mags[i]; cnt++; }
        float v = (cnt > 0) ? (accum / cnt) : 0.0f;
        float log_part = 0.4f * std::log10(1.0f + 9.0f * v);
        float linear_part = 0.6f * std::min(v, 1.0f);
        v = std::min(1.0f, std::max(0.0f, log_part + linear_part));
        const float eq_mul = (b < (int)eq_copy.size()) ? eq_copy[(size_t)b] : 1.0f;
        newBands[b] = v * eq_mul;
    }
    if((int)band_peak_smoothed.size() != bands_count)
    {
        band_peak_smoothed.assign(bands_count, 0.1f);
    }
    const float clarity = mix_clarity;
    for(int band_index = 0; band_index < (int)newBands.size(); band_index++)
    {
        float v = newBands[band_index];
        float decay = (band_index < bands_count / 4) ? bass_peak_decay : band_peak_decay;
        float peak = band_peak_smoothed[band_index];
        peak = std::max(v, peak * decay);
        if(peak < 1e-6f) peak = 1e-6f;
        band_peak_smoothed[band_index] = peak;
        float peak_norm = std::min(1.0f, v / peak);

        float activity_norm = peak_norm;
        if(clarity > 1e-4f)
        {
            if((int)band_noise_floor.size() != bands_count)
            {
                band_noise_floor.assign(bands_count, 0.0f);
            }
            if((int)band_peak_activity.size() != bands_count)
            {
                band_peak_activity.assign(bands_count, 0.05f);
            }
            float floor = band_noise_floor[band_index];
            if(v <= floor)
            {
                floor = 0.992f * floor + 0.008f * v;
            }
            else
            {
                floor = 0.9992f * floor + 0.0008f * v;
            }
            band_noise_floor[band_index] = floor;

            const float floor_cut = floor + clarity * (v - floor) * 0.88f;
            float above = std::max(0.0f, v - floor_cut);

            float act_peak = band_peak_activity[band_index];
            act_peak = std::max(above, act_peak * activity_peak_decay);
            if(act_peak < 1e-5f) act_peak = 1e-5f;
            band_peak_activity[band_index] = act_peak;

            activity_norm = std::min(1.0f, above / act_peak);
            activity_norm = activity_norm * activity_norm * (3.0f - 2.0f * activity_norm);
        }

        newBands[band_index] = (1.0f - clarity) * peak_norm + clarity * activity_norm;
    }

    {
        QMutexLocker bl(&bands_mutex);
        if((int)bands16.size() != bands_count) bands16.assign(bands_count, 0.0f);
        if((int)prev_band_frame.size() != bands_count) prev_band_frame.assign(bands_count, 0.0f);
        if((int)band_slow.size() != bands_count) band_slow.assign(bands_count, 0.0f);
        if((int)band_flux.size() != bands_count) band_flux.assign(bands_count, 0.0f);
        if((int)band_transient.size() != bands_count) band_transient.assign(bands_count, 0.0f);
        const float slow_alpha = 0.04f + 0.03f * (1.0f - clarity);
        for(int b = 0; b < bands_count; b++)
        {
            float frame = newBands[b];
            float delta = frame - prev_band_frame[b];
            band_flux[b] = (delta > 0.0f) ? delta : 0.0f;
            float slow = band_slow[b];
            slow = (1.0f - slow_alpha) * slow + slow_alpha * frame;
            band_slow[b] = slow;
            band_transient[b] = std::max(0.0f, frame - slow);
            prev_band_frame[b] = frame;
            bands16[b] = ema_smoothing * bands16[b] + (1.0f - ema_smoothing) * frame;
        }
        float log_ratio = std::log(f_max / f_min);
        float bass_norm = 0.0f;
        if(std::abs(log_ratio) > 1e-6f)
        {
            bass_norm = std::log(xover_bass_upper / f_min) / log_ratio;
        }
        int b_end = (int)std::floor(bass_norm * bands_count);
        if(b_end < 0) b_end = 0;
        if(b_end > bands_count) b_end = bands_count;

        float mid_norm = 0.0f;
        if(std::abs(log_ratio) > 1e-6f)
        {
            mid_norm = std::log(xover_mid_upper / f_min) / log_ratio;
        }
        int m_end = (int)std::floor(mid_norm * bands_count);
        if(m_end < 0) m_end = 0;
        if(m_end > bands_count) m_end = bands_count;

        if(b_end < 1) b_end = 1; if(m_end <= b_end) m_end = b_end + 1; if(m_end > bands_count) m_end = bands_count;
        float bsum=0, msum=0, tsum=0;
        float bslow=0, mslow=0, tslow=0;
        int bc=0, mc=0, tc=0;
        for(int i=0;i<bands_count;i++)
        {
            if(i < b_end)
            {
                bsum += bands16[i];
                bslow += band_slow[i];
                bc++;
            }
            else if(i < m_end)
            {
                msum += bands16[i];
                mslow += band_slow[i];
                mc++;
            }
            else
            {
                tsum += bands16[i];
                tslow += band_slow[i];
                tc++;
            }
        }
        float bass_now = (bc ? bsum / bc : 0.0f);
        float mid_now  = (mc ? msum / mc : 0.0f);
        float treb_now = (tc ? tsum / tc : 0.0f);
        float bass_sl  = (bc ? bslow / bc : 0.0f);
        float mid_sl   = (mc ? mslow / mc : 0.0f);
        float treb_sl  = (tc ? tslow / tc : 0.0f);
        bass_level   = (1.0f - clarity) * bass_now + clarity * bass_sl;
        mid_level    = (1.0f - clarity) * mid_now  + clarity * mid_sl;
        treble_level = (1.0f - clarity) * treb_now + clarity * treb_sl;

        updateVisualizerBuckets(mags, f_min, f_max);

        if(prev_mags.size() != mags.size()) prev_mags.assign(mags.size(), 0.0f);
        double flux = 0.0;
        for(size_t mag_index = 0; mag_index < mags.size(); mag_index++)
        {
            double diff = mags[mag_index] - prev_mags[mag_index];
            if(diff > 0) flux += diff;
        }
        prev_mags = mags;
        double nf = std::log10(1.0 + 9.0 * flux);
        onset_level = (float)(0.6 * onset_level + 0.4 * std::min(1.0, nf));
        updateStreamStemsLocked();
    }
}

std::vector<float> AudioInputManager::getBands() const
{
    QMutexLocker bl(&bands_mutex);
    return bands16;
}

void AudioInputManager::getBands(std::vector<float>& out) const
{
    QMutexLocker bl(&bands_mutex);
    out = bands16;
}

float AudioInputManager::getBassLevel() const
{
    QMutexLocker bl(&bands_mutex);
    return bass_level;
}

float AudioInputManager::getMidLevel() const
{
    QMutexLocker bl(&bands_mutex);
    return mid_level;
}

float AudioInputManager::getTrebleLevel() const
{
    QMutexLocker bl(&bands_mutex);
    return treble_level;
}

float AudioInputManager::getOnsetLevel() const
{
    QMutexLocker bl(&bands_mutex);
    return onset_level;
}

namespace {

void BandIndexRangeForHz(int bands_count,
                         int sample_rate_hz,
                         int fft_bins,
                         float low_hz,
                         float high_hz,
                         int& i0,
                         int& i1)
{
    i0 = 0;
    i1 = 0;
    if(bands_count <= 0 || sample_rate_hz <= 0)
    {
        return;
    }
    float fs = (float)sample_rate_hz;
    int fft_size_local = fft_bins;
    if(fft_size_local < 512) fft_size_local = 512;
    float bin_min = fs / (float)fft_size_local;
    if(bin_min < 1.0f)
    {
        bin_min = 1.0f;
    }
    float f_min = bin_min;
    float f_max = fs * 0.5f;
    if(f_max <= f_min || f_max < 0.001f)
    {
        i1 = bands_count - 1;
        return;
    }
    if(high_hz <= low_hz)
    {
        high_hz = low_hz + 1.0f;
    }
    float clamped_low = low_hz;
    if(clamped_low < f_min) clamped_low = f_min;
    if(clamped_low > f_max) clamped_low = f_max;
    float log_ratio = std::log(f_max / f_min);
    float low_norm = 0.0f;
    if(std::abs(log_ratio) > 1e-6f)
    {
        low_norm = std::log(clamped_low / f_min) / log_ratio;
    }
    i0 = (int)std::floor(low_norm * bands_count);
    if(i0 < 0) i0 = 0;
    if(i0 > bands_count - 1) i0 = bands_count - 1;

    float clamped_high = high_hz;
    if(clamped_high < f_min) clamped_high = f_min;
    if(clamped_high > f_max) clamped_high = f_max;
    float high_norm = 0.0f;
    if(std::abs(log_ratio) > 1e-6f)
    {
        high_norm = std::log(clamped_high / f_min) / log_ratio;
    }
    i1 = (int)std::floor(high_norm * bands_count);
    if(i1 < 0) i1 = 0;
    if(i1 > bands_count - 1) i1 = bands_count - 1;
    if(i1 < i0) std::swap(i0, i1);
    if(i1 == i0) i1 = std::min(i0 + 1, bands_count - 1);
}

float EffectiveIsolation(float global_isolation, float /*extra_isolation*/)
{
    return std::clamp(global_isolation, 0.0f, 1.0f);
}

float IsolatedBandMeasure(const std::vector<float>& values, int i0, int i1, float isolation)
{
    if(values.empty() || i0 < 0 || i1 < 0)
    {
        return 0.0f;
    }
    if(i0 >= (int)values.size())
    {
        i0 = (int)values.size() - 1;
    }
    if(i1 >= (int)values.size())
    {
        i1 = (int)values.size() - 1;
    }
    if(i1 < i0)
    {
        std::swap(i0, i1);
    }

    const int span = i1 - i0 + 1;
    float weighted_sum = 0.0f;
    float weight_total = 0.0f;
    for(int i = i0; i <= i1; ++i)
    {
        float t = (span <= 1) ? 0.5f : (float)(i - i0) / (float)(span - 1);
        float w = std::sin(3.14159265f * t);
        w = 0.12f + 0.88f * w * w;
        weighted_sum += values[(size_t)i] * w;
        weight_total += w;
    }
    float inside = (weight_total > 1e-6f) ? (weighted_sum / weight_total) : 0.0f;

    if(isolation <= 1e-4f)
    {
        return std::min(1.0f, inside);
    }

    const int bleed = std::max(1, span / 2);
    float below = 0.0f;
    float above = 0.0f;
    int below_count = 0;
    int above_count = 0;
    for(int i = std::max(0, i0 - bleed); i < i0; ++i)
    {
        below += values[(size_t)i];
        below_count++;
    }
    for(int i = i1 + 1; i <= std::min((int)values.size() - 1, i1 + bleed); ++i)
    {
        above += values[(size_t)i];
        above_count++;
    }
    float bleed_level = 0.0f;
    if(below_count > 0 && above_count > 0)
    {
        bleed_level = 0.5f * (below / (float)below_count + above / (float)above_count);
    }
    else if(below_count > 0)
    {
        bleed_level = below / (float)below_count;
    }
    else if(above_count > 0)
    {
        bleed_level = above / (float)above_count;
    }

    const float reject = isolation * bleed_level * (1.05f + 0.35f * (float)span / 8.0f);
    return std::clamp(inside - reject, 0.0f, 1.0f);
}

} // namespace

void AudioInputManager::updateStreamStemsLocked()
{
    const float iso = band_isolation;
    auto query_trans = [&](float low_hz, float high_hz) {
        int i0 = 0;
        int i1 = 0;
        BandIndexRangeForHz(bands_count, sample_rate_hz, fft_size, low_hz, high_hz, i0, i1);
        return IsolatedBandMeasure(band_transient, i0, i1, iso);
    };
    auto query_slow = [&](float low_hz, float high_hz) {
        int i0 = 0;
        int i1 = 0;
        BandIndexRangeForHz(bands_count, sample_rate_hz, fft_size, low_hz, high_hz, i0, i1);
        return IsolatedBandMeasure(band_slow, i0, i1, iso * 0.85f);
    };

    const float kick_trans = query_trans(48.0f, 118.0f);
    const float kick_slow = query_slow(48.0f, 118.0f);
    float kick = std::max(0.0f, kick_trans - 0.78f * kick_slow);

    float bass = query_slow(62.0f, 260.0f);
    bass = std::max(0.0f, bass - kick * 0.42f);

    float snare = query_trans(165.0f, 980.0f);
    snare = std::max(0.0f, snare - kick * 0.38f - bass * 0.18f);

    float hihat = query_trans(2600.0f, 14500.0f);
    const float side_mul = 0.55f + 0.65f * stereo_width;
    hihat *= side_mul;
    hihat = std::max(0.0f, hihat - snare * 0.22f);

    const float smooth = 0.82f + 0.1f * (1.0f - mix_clarity);
    stream_kick = smooth * stream_kick + (1.0f - smooth) * std::min(1.0f, kick * 1.12f);
    stream_snare = smooth * stream_snare + (1.0f - smooth) * std::min(1.0f, snare * 1.08f);
    stream_hihat = smooth * stream_hihat + (1.0f - smooth) * std::min(1.0f, hihat * 1.10f);
    stream_bass = smooth * stream_bass + (1.0f - smooth) * std::min(1.0f, bass * 1.05f);
}

AudioInputManager::StreamStemLevels AudioInputManager::getStreamStemLevels() const
{
    QMutexLocker bl(&bands_mutex);
    StreamStemLevels s;
    s.kick = stream_kick;
    s.snare = stream_snare;
    s.hihat = stream_hihat;
    s.bass = stream_bass;
    return s;
}

float AudioInputManager::getStereoWidth() const
{
    QMutexLocker bl(&bands_mutex);
    return stereo_width;
}

float AudioInputManager::getBandOnsetLevel(float low_hz, float high_hz, float extra_isolation) const
{
    QMutexLocker bl(&bands_mutex);
    if(band_flux.empty())
    {
        return 0.0f;
    }
    int i0 = 0;
    int i1 = 0;
    BandIndexRangeForHz(bands_count, sample_rate_hz, fft_size, low_hz, high_hz, i0, i1);
    const float iso = EffectiveIsolation(band_isolation, extra_isolation);
    float flux = IsolatedBandMeasure(band_flux, i0, i1, iso);
    double nf = std::log10(1.0 + 9.0 * (double)flux);
    return (float)std::min(1.0, nf);
}

float AudioInputManager::getBandTransientEnergyHz(float low_hz, float high_hz, float extra_isolation) const
{
    QMutexLocker bl(&bands_mutex);
    if(band_transient.empty())
    {
        return 0.0f;
    }
    int i0 = 0;
    int i1 = 0;
    BandIndexRangeForHz(bands_count, sample_rate_hz, fft_size, low_hz, high_hz, i0, i1);
    const float iso = EffectiveIsolation(band_isolation, extra_isolation);
    return std::min(1.0f, IsolatedBandMeasure(band_transient, i0, i1, iso));
}

float AudioInputManager::getBandSlowEnergyHz(float low_hz, float high_hz, float extra_isolation) const
{
    QMutexLocker bl(&bands_mutex);
    if(band_slow.empty())
    {
        return 0.0f;
    }
    int i0 = 0;
    int i1 = 0;
    BandIndexRangeForHz(bands_count, sample_rate_hz, fft_size, low_hz, high_hz, i0, i1);
    const float iso = EffectiveIsolation(band_isolation, extra_isolation);
    return std::min(1.0f, IsolatedBandMeasure(band_slow, i0, i1, iso));
}

float AudioInputManager::getBandEnergyHz(float low_hz, float high_hz, float extra_isolation) const
{
    QMutexLocker bl(&bands_mutex);
    if(bands16.empty() || sample_rate_hz <= 0)
    {
        return 0.0f;
    }
    int i0 = 0;
    int i1 = 0;
    BandIndexRangeForHz(bands_count, sample_rate_hz, fft_size, low_hz, high_hz, i0, i1);
    const float iso = EffectiveIsolation(band_isolation, extra_isolation);
    return std::min(1.0f, IsolatedBandMeasure(bands16, i0, i1, iso));
}

float AudioInputManager::getBandEnergyHzWithGain(float low_hz, float high_hz, const float* band_gain_16) const
{
    QMutexLocker bl(&bands_mutex);
    if(bands16.empty() || sample_rate_hz <= 0)
    {
        return 0.0f;
    }
    float fs = (float)sample_rate_hz;
    if(fft_size <= 0 || fs <= 0.0f) return 0.0f;
    float bin_min = fs / (float)fft_size;
    float f_min = std::max(1.0f, bin_min);
    float f_max = fs * 0.5f;
    if(f_max <= f_min || f_max < 0.001f) return 0.0f;
    if(high_hz <= low_hz) high_hz = low_hz + 1.0f;
    float clamped_low = low_hz;
    if(clamped_low < f_min) clamped_low = f_min;
    if(clamped_low > f_max) clamped_low = f_max;
    float log_ratio = std::log(f_max / f_min);
    float low_norm = (std::abs(log_ratio) > 1e-6f) ? (std::log(clamped_low / f_min) / log_ratio) : 0.0f;
    int i0 = (int)std::floor(low_norm * (int)bands16.size());
    if(i0 < 0) i0 = 0;
    if(i0 > (int)bands16.size() - 1) i0 = (int)bands16.size() - 1;
    float clamped_high = high_hz;
    if(clamped_high < f_min) clamped_high = f_min;
    if(clamped_high > f_max) clamped_high = f_max;
    float high_norm = (std::abs(log_ratio) > 1e-6f) ? (std::log(clamped_high / f_min) / log_ratio) : 0.0f;
    int i1 = (int)std::floor(high_norm * (int)bands16.size());
    if(i1 < 0) i1 = 0;
    if(i1 > (int)bands16.size() - 1) i1 = (int)bands16.size() - 1;
    if(i1 < i0) std::swap(i0, i1);
    if(i1 == i0) i1 = std::min(i0 + 1, (int)bands16.size() - 1);
    float sum = 0.0f;
    int cnt = 0;
    for(int i = i0; i <= i1; ++i)
    {
        float gv = 1.0f;
        if(band_gain_16)
        {
            const int gain_count = (int)bands16.size();
            const int gi = std::clamp(i, 0, gain_count > 0 ? gain_count - 1 : 0);
            gv = band_gain_16[gi];
        }
        else if(i >= 0 && i < (int)eq_gain.size())
        {
            gv = eq_gain[(size_t)i];
        }
        sum += bands16[i] * std::max(0.0f, gv);
        cnt++;
    }
    return (cnt > 0) ? std::min(1.0f, sum / (float)cnt) : 0.0f;
}

void AudioInputManager::setEqGain(int band_index, float gain)
{
    QMutexLocker bl(&bands_mutex);
    ensureEqGainSizeLocked();
    if(band_index < 0 || band_index >= (int)eq_gain.size())
    {
        return;
    }
    eq_gain[(size_t)band_index] = std::max(0.0f, std::min(2.0f, gain));
}

float AudioInputManager::getEqGain(int band_index) const
{
    QMutexLocker bl(&bands_mutex);
    if(band_index < 0 || band_index >= (int)eq_gain.size())
    {
        return 1.0f;
    }
    return eq_gain[(size_t)band_index];
}

void AudioInputManager::resetEq()
{
    QMutexLocker bl(&bands_mutex);
    ensureEqGainSizeLocked();
    for(float& g : eq_gain)
    {
        g = 1.0f;
    }
}

AudioInputManager::SpectrumSnapshot AudioInputManager::getSpectrumSnapshot(int target_bins) const
{
    SpectrumSnapshot snapshot;
    if(target_bins <= 0)
    {
        target_bins = 256;
    }

    QMutexLocker bl(&bands_mutex);
    if(visualizer_bins.empty())
    {
        return snapshot;
    }

    std::function<void(const std::vector<float>&, std::vector<float>&)> resample = [target_bins](const std::vector<float>& src, std::vector<float>& dst)
    {
        if(src.empty())
        {
            dst.clear();
            return;
        }
        if((int)src.size() == target_bins)
        {
            dst = src;
            return;
        }
        dst.assign(target_bins, 0.0f);
        int src_count = (int)src.size();
        for(int i = 0; i < target_bins; ++i)
        {
            float pos = (i + 0.5f) / (float)target_bins;
            int idx = (int)std::floor(pos * src_count);
            if(idx < 0) idx = 0;
            if(idx >= src_count) idx = src_count - 1;
            dst[i] = src[idx];
        }
    };

    snapshot.min_frequency_hz = visualizer_min_hz;
    snapshot.max_frequency_hz = visualizer_max_hz;
    resample(visualizer_bins, snapshot.bins);
    resample(visualizer_peaks, snapshot.peaks);
    return snapshot;
}

void AudioInputManager::updateVisualizerBuckets(const std::vector<float>& mags, float min_hz, float max_hz)
{
    const int viz_bins_count = 256;
    if((int)visualizer_bins.size() != viz_bins_count)
    {
        visualizer_bins.assign(viz_bins_count, 0.0f);
        visualizer_peaks.assign(viz_bins_count, 0.0f);
    }

    if(mags.empty())
    {
        std::fill(visualizer_bins.begin(), visualizer_bins.end(), 0.0f);
        std::fill(visualizer_peaks.begin(), visualizer_peaks.end(), 0.0f);
        visualizer_min_hz = min_hz;
        visualizer_max_hz = max_hz;
        return;
    }

    float fs = (float)sample_rate_hz;
    if(fs <= 0.0f)
    {
        fs = 48000.0f;
    }
    float bin_hz = fs / (float)fft_size;
    if(bin_hz <= 0.0f)
    {
        bin_hz = 1.0f;
    }

    float low = std::max(min_hz, bin_hz);
    float high = std::max(max_hz, low + bin_hz);

    float log_span = std::log(high / low);
    if(std::abs(log_span) < 1e-6f)
    {
        log_span = 1.0f;
    }

    int fft_bins = (int)mags.size();
    for(int i = 0; i < viz_bins_count; ++i)
    {
        float start_ratio = (float)i / (float)viz_bins_count;
        float end_ratio = (float)(i + 1) / (float)viz_bins_count;
        float start_hz = low * std::exp(log_span * start_ratio);
        float end_hz = low * std::exp(log_span * end_ratio);
        if(end_hz <= start_hz)
        {
            end_hz = start_hz + bin_hz;
        }
        int start_idx = (int)std::floor(start_hz / bin_hz);
        int end_idx = (int)std::ceil(end_hz / bin_hz);
        if(start_idx < 1) start_idx = 1;
        if(end_idx <= start_idx) end_idx = start_idx + 1;
        if(end_idx > fft_bins) end_idx = fft_bins;
        if(start_idx >= end_idx) start_idx = std::max(1, end_idx - 1);

        float accum = 0.0f;
        int count = 0;
        for(int k = start_idx; k < end_idx; ++k)
        {
            accum += mags[k];
            count++;
        }
        float value = (count > 0) ? (accum / (float)count) : 0.0f;
        float log_part = 0.4f * std::log10(1.0f + 9.0f * value);
        float linear_part = 0.6f * std::min(value, 1.0f);
        value = std::min(1.0f, std::max(0.0f, log_part + linear_part));

        if(visualizer_peaks[i] < value)
        {
            visualizer_peaks[i] = value;
        }
        else
        {
            visualizer_peaks[i] *= visualizer_peak_decay;
            if(visualizer_peaks[i] < visualizer_floor)
            {
                visualizer_peaks[i] = visualizer_floor;
            }
        }
        float peak = std::max(visualizer_peaks[i], visualizer_floor);
        visualizer_bins[i] = std::min(1.0f, value / peak);
    }

    visualizer_min_hz = low;
    visualizer_max_hz = high;
}
