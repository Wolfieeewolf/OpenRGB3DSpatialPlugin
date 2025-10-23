/*---------------------------------------------------------*\
| AudioInputManager.cpp                                     |
|                                                           |
|   Minimal audio capture for audio-reactive effects        |
|                                                           |
|   Date: 2025-10-14                                        |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#include "AudioInputManager.h"
#include <cmath>
#include <complex>
#include <algorithm>
#include <thread>

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

// Math constant for MSVC where M_PI may be undefined
static constexpr float PI_F = 3.14159265358979323846f;

#ifdef _WIN32
// Minimal WASAPI capturer (loopback or capture)
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
        // COM must be initialized in this thread
        HRESULT coinithr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        IMMDeviceEnumerator* enumerator = nullptr;
        IMMDevice* device = nullptr;
        IAudioClient* client = nullptr;
        IAudioCaptureClient* capture = nullptr;

        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                      __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
        if(FAILED(hr)) { if(SUCCEEDED(coinithr)) CoUninitialize(); return; }
        if(dev_id.isEmpty())
            hr = enumerator->GetDefaultAudioEndpoint(loopback ? eRender : eCapture, eMultimedia, &device);
        else
            hr = enumerator->GetDevice((LPCWSTR)dev_id.utf16(), &device);
        if(FAILED(hr)) { enumerator->Release(); if(SUCCEEDED(coinithr)) CoUninitialize(); return; }
        hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&client);
        if(FAILED(hr)) { device->Release(); enumerator->Release(); if(SUCCEEDED(coinithr)) CoUninitialize(); return; }

        WAVEFORMATEX* mix = nullptr;
        client->GetMixFormat(&mix);
        hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                (loopback ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0),
                                0, 0, mix, nullptr);
        if(FAILED(hr)) { CoTaskMemFree(mix); client->Release(); device->Release(); enumerator->Release(); if(SUCCEEDED(coinithr)) CoUninitialize(); return; }

        hr = client->GetService(__uuidof(IAudioCaptureClient), (void**)&capture);
        if(FAILED(hr)) { client->Release(); device->Release(); enumerator->Release(); CoTaskMemFree(mix); if(SUCCEEDED(coinithr)) CoUninitialize(); return; }

        hr = client->Start();
        if(FAILED(hr)) { capture->Release(); client->Release(); device->Release(); enumerator->Release(); CoTaskMemFree(mix); if(SUCCEEDED(coinithr)) CoUninitialize(); return; }

        const bool isFloat = (mix->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
                             (mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE && ((WAVEFORMATEXTENSIBLE*)mix)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
        const bool isPCM   = (mix->wFormatTag == WAVE_FORMAT_PCM) ||
                             (mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE && ((WAVEFORMATEXTENSIBLE*)mix)->SubFormat == KSDATAFORMAT_SUBTYPE_PCM);
        int bitsPerSample  = mix->wBitsPerSample;
        if(mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
        {
            WORD vbits = ((WAVEFORMATEXTENSIBLE*)mix)->Samples.wValidBitsPerSample;
            if(vbits) bitsPerSample = vbits;
        }
        const int channels = mix->nChannels;
        manager->setSampleRate((int)mix->nSamplesPerSec);
        // Set channel info for diagnostics
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
            if(isFloat)
            {
                const float* f = reinterpret_cast<const float*>(data);
                for(UINT32 i=0;i<frames;i++)
                {
                    double acc = 0.0;
                    for(int c=0;c<channels;c++) acc += f[i*channels + c];
                    acc /= (double)channels;
                    if(acc > 1.0) acc = 1.0; if(acc < -1.0) acc = -1.0;
                    mono[i] = (int16_t)(acc * 32767.0);
                }
            }
            else if(isPCM && bitsPerSample == 16)
            {
                const int16_t* s = reinterpret_cast<const int16_t*>(data);
                for(UINT32 i=0;i<frames;i++)
                {
                    int sum = 0;
                    for(int c=0;c<channels;c++) sum += s[i*channels + c];
                    int16_t v = (int16_t)(sum / std::max(1, channels));
                    mono[i] = v;
                }
            }
            else if(isPCM && bitsPerSample >= 24)
            {
                // Treat as 32-bit container and scale from int32
                const int32_t* s32 = reinterpret_cast<const int32_t*>(data);
                for(UINT32 i=0;i<frames;i++)
                {
                    long long sum = 0;
                    for(int c=0;c<channels;c++) sum += s32[i*channels + c];
                    double acc = (double)sum / (double)std::max(1, channels);
                    // scale from 32-bit to 16-bit
                    int v = (int)(acc / 2147483648.0 * 32767.0);
                    if(v > 32767) v = 32767; if(v < -32768) v = -32768;
                    mono[i] = (int16_t)v;
                }
            }
            else
            {
                // Fallback: treat bytes as 16-bit
                const int16_t* s = reinterpret_cast<const int16_t*>(data);
                for(UINT32 i=0;i<frames;i++)
                {
                    int sum = 0;
                    for(int c=0;c<channels;c++) sum += s[i*channels + c];
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
    level_timer.setInterval(33); // ~30Hz UI updates
    connect(&level_timer, &QTimer::timeout, this, &AudioInputManager::onLevelTick);
    sample_buffer.reserve(fft_size * 4);
    bands16.assign(16, 0.0f);
}

QStringList AudioInputManager::listInputDevices()
{
    QStringList names;
#ifdef _WIN32
    device_names.clear();
    device_ids.clear();
    device_is_loopback.clear();
    IMMDeviceEnumerator* enumerator = nullptr;
    // Ensure COM initialized for enumeration
    HRESULT coinithr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
    if(FAILED(hr))
    {
        if(SUCCEEDED(coinithr)) CoUninitialize();
        return names;
    }
    // Render devices as loopback sources
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
    // Capture devices
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
    // Release mutex before calling stop()/start() to avoid deadlock
    if(was_running)
    {
        stop();
        start();
    }
}

// Unified device list; separate render list removed


void AudioInputManager::start()
{
    QMutexLocker lock(&mutex);
    if(running) return;

#ifdef _WIN32
    if(selected_index < 0 || selected_index >= (int)device_ids.size()) return;
    if(capturer) { delete capturer; capturer = nullptr; }
    QString devId = device_ids[selected_index];
    bool is_loop = (selected_index < (int)device_is_loopback.size()) ? device_is_loopback[selected_index] : true;
    capturer = new WasapiCapturer(this, devId, is_loop);
    running = true;
    level_timer.start();
    return;
#else
    // Non-Windows not implemented
    running = true;
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
    {
        QMutexLocker bl(&bands_mutex);
        std::fill(bands16.begin(), bands16.end(), 0.0f);
        bass_level = mid_level = treble_level = 0.0f;
    }
}

void AudioInputManager::setGain(float g)
{
    if(g < 0.1f) g = 0.1f;
    if(g > 10.0f) g = 10.0f;
    gain = g;
}

void AudioInputManager::setSmoothing(float s)
{
    if(s < 0.0f) s = 0.0f;
    if(s > 0.99f) s = 0.99f;
    ema_smoothing = s;
}

// Deprecated: capture source is implicit in device selection

void AudioInputManager::setBandsCount(int bands)
{
    if(bands != 8 && bands != 16 && bands != 32) bands = 16;
    bands_count = bands;
    QMutexLocker bl(&bands_mutex);
    bands16.assign(bands_count, 0.0f);
}

void AudioInputManager::setFFTSize(int n)
{
    // Accept common sizes 512..8192 and coerce to power-of-two
    if(n < 512) n = 512;
    if(n > 8192) n = 8192;
    // Round to nearest power of two
    int p = 1; while(p < n) p <<= 1; // next power
    // choose closer of previous and next powers
    int prev = p >> 1; if(prev < 512) prev = 512;
    int chosen = (p - n) < (n - prev) ? p : prev;
    if(chosen < 512) chosen = 512; if(chosen > 8192) chosen = 8192;
    if(chosen == fft_size) return;
    fft_size = chosen;
    // Reset analysis buffers so new size takes effect cleanly
    sample_buffer.clear();
    window.clear();
    prev_mags.clear();
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
    // Compute RMS of 16-bit mono PCM
    if(bytes <= 0) return;

    int16_t const* samples = reinterpret_cast<const int16_t*>(data);
    int count = bytes / (int)sizeof(int16_t);
    if(count <= 0) return;

    double sum = 0.0;
    for(int i = 0; i < count; i++)
    {
        double s = samples[i] / 32768.0; // -1..1
        sum += s * s;
        sample_buffer.push_back((float)s);
    }
    double rms = std::sqrt(sum / std::max(1, count));

    // Apply simple soft clip and gain
    double val = rms * gain;
    if(val > 1.0) val = 1.0;

    // EMA smoothing
    float prev = current_level.load();
    float out = (float)(ema_smoothing * prev + (1.0 - ema_smoothing) * val);
    if(out < 0.0f) out = 0.0f; if(out > 1.0f) out = 1.0f;
    current_level.store(out);

    // When enough samples accumulated, compute spectrum
    if((int)sample_buffer.size() >= fft_size)
    {
        computeSpectrum();
        // keep last fft_size samples as overlap (50%)
        int keep = fft_size / 2;
        if(keep < 1) keep = 1;
        if((int)sample_buffer.size() > keep)
        {
            sample_buffer.erase(sample_buffer.begin(), sample_buffer.end() - keep);
        }
    }
}

void AudioInputManager::onLevelTick()
{
    emit LevelUpdated(current_level.load());
}

void AudioInputManager::FeedPCM16(const int16_t* samples, int count)
{
    if(!samples || count <= 0) return;
    // Reuse processing path
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
    // bit-reverse
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
    // Take last fft_size samples
    std::vector<std::complex<float>> buf(fft_size);
    int start = (int)sample_buffer.size() - fft_size;
    for(int i = 0; i < fft_size; i++)
    {
        float s = sample_buffer[start + i] * window[i];
        buf[i] = std::complex<float>(s, 0.0f);
    }
    fft_cooley_tukey(buf);
    // Magnitude spectrum (first half)
    int n2 = fft_size / 2;
    std::vector<float> mags(n2);
    for(int i = 0; i < n2; i++)
    {
        float m = std::abs(buf[i]) / (fft_size * 0.5f);
        mags[i] = m;
    }

    // Map to N log bands between ~bin resolution and Nyquist
    float fs = (float)sample_rate_hz;
    // Smallest meaningful frequency is one FFT bin (skip DC at bin 0)
    float bin_min = fs / (float)fft_size;
    float f_min = std::max(1.0f, bin_min);
    float f_max = fs * 0.5f;
    std::vector<float> newBands(bands_count, 0.0f);
    for(int b = 0; b < bands_count; b++)
    {
        float t0 = (float)b / (float)bands_count;
        float t1 = (float)(b + 1) / (float)bands_count;
        float fb0 = f_min * std::pow(f_max / f_min, t0);
        float fb1 = f_min * std::pow(f_max / f_min, t1);
        int i0 = (int)std::floor(fb0 * n2 / f_max);
        int i1 = (int)std::ceil (fb1 * n2 / f_max);
        if(i0 < 1) i0 = 1; if(i1 <= i0) i1 = i0 + 1; if(i1 >= n2) i1 = n2 - 1;
        float accum = 0.0f; int cnt = 0;
        for(int i = i0; i < i1; i++) { accum += mags[i]; cnt++; }
        float v = (cnt > 0) ? (accum / cnt) : 0.0f;
        // log-like companding and normalization
        v = std::log10(1.0f + 9.0f * v);
        // smooth with EMA per band
        newBands[b] = v;
    }
    // Normalize rough range 0..1
    float maxv = 1e-6f;
    for(float v : newBands) if(v > maxv) maxv = v;
    for(float& v : newBands) v = std::min(1.0f, v / maxv);

    // Store with smoothing
    {
        QMutexLocker bl(&bands_mutex);
        if((int)bands16.size() != bands_count) bands16.assign(bands_count, 0.0f);
        for(int b = 0; b < bands_count; b++)
        {
            bands16[b] = ema_smoothing * bands16[b] + (1.0f - ema_smoothing) * newBands[b];
        }
        // Aggregate bass/mid/treble using crossovers
        auto bandIndexForHz = [&](float hz){
            float t = std::log(hz / f_min) / std::log(f_max / f_min);
            int idx = (int)std::floor(t * bands_count);
            if(idx < 0) idx = 0; if(idx > bands_count) idx = bands_count;
            return idx;
        };
        int b_end = bandIndexForHz(xover_bass_upper);
        int m_end = bandIndexForHz(xover_mid_upper);
        if(b_end < 1) b_end = 1; if(m_end <= b_end) m_end = b_end + 1; if(m_end > bands_count) m_end = bands_count;
        float bsum=0, msum=0, tsum=0; int bc=0, mc=0, tc=0;
        for(int i=0;i<bands_count;i++)
        {
            if(i < b_end) { bsum += bands16[i]; bc++; }
            else if(i < m_end) { msum += bands16[i]; mc++; }
            else { tsum += bands16[i]; tc++; }
        }
        bass_level = (bc? bsum/bc:0.0f);
        mid_level  = (mc? msum/mc:0.0f);
        treble_level=(tc? tsum/tc:0.0f);

        // Onset detection via spectral flux (half-wave rectified diff of magnitudes)
        if(prev_mags.size() != mags.size()) prev_mags.assign(mags.size(), 0.0f);
        double flux = 0.0;
        for(size_t i=0;i<mags.size();i++)
        {
            double diff = mags[i] - prev_mags[i];
            if(diff > 0) flux += diff;
        }
        prev_mags = mags;
        // Normalize flux and smooth
        double nf = std::log10(1.0 + 9.0 * flux);
        onset_level = (float)(0.6 * onset_level + 0.4 * std::min(1.0, nf));
    }
}

std::vector<float> AudioInputManager::getBands() const
{
    QMutexLocker bl(&bands_mutex);
    return bands16;
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

float AudioInputManager::getBandEnergyHz(float low_hz, float high_hz) const
{
    QMutexLocker bl(&bands_mutex);
    if(bands16.empty() || sample_rate_hz <= 0) return 0.0f;
    float fs = (float)sample_rate_hz;
    float bin_min = fs / (float)fft_size;
    float f_min = std::max(1.0f, bin_min);
    float f_max = fs * 0.5f;
    if(high_hz <= low_hz) high_hz = low_hz + 1.0f;
    auto idxForHz = [&](float hz){
        if(hz < f_min) hz = f_min;
        if(hz > f_max) hz = f_max;
        float t = std::log(hz / f_min) / std::log(f_max / f_min);
        int idx = (int)std::floor(t * (int)bands16.size());
        if(idx < 0) idx = 0; if(idx > (int)bands16.size()-1) idx = (int)bands16.size()-1;
        return idx;
    };
    int i0 = idxForHz(low_hz);
    int i1 = idxForHz(high_hz);
    if(i1 < i0) std::swap(i0, i1);
    if(i1 == i0) i1 = std::min(i0+1, (int)bands16.size()-1);
    float sum = 0.0f; int cnt = 0;
    for(int i=i0; i<=i1; ++i){ sum += bands16[i]; cnt++; }
    return (cnt>0) ? std::min(1.0f, sum / (float)cnt) : 0.0f;
}

// Removed duplicate WasapiLoopback class (now defined at top)

