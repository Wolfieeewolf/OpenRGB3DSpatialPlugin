/*---------------------------------------------------------*\
| AudioInputManager.h                                       |
|                                                           |
|   Minimal audio capture for audio-reactive effects        |
|                                                           |
|   Captures system/mic audio level and exposes a           |
|   smoothed 0..1 amplitude value.                          |
|                                                           |
|   Date: 2025-10-14                                        |
|                                                           |
|   SPDX-License-Identifier: GPL-2.0-only                   |
\*---------------------------------------------------------*/

#ifndef AUDIOINPUTMANAGER_H
#define AUDIOINPUTMANAGER_H

#include <QObject>
#include <QByteArray>
#include <QMutex>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <atomic>
#include <vector>

// No Qt Multimedia dependency; using WASAPI directly on Windows

class AudioInputManager : public QObject
{
    Q_OBJECT
public:
    static AudioInputManager* instance();

    // Device management (Windows: returns both loopback outputs and capture inputs)
    QStringList listInputDevices(); // Human-readable names (outputs have "(Loopback)")
    int defaultDeviceIndex() const;
    void setDeviceByIndex(int index);
    // Windows: unified device list; non-Windows returns empty list

    // Capture control
    void start();
    void stop();
    bool isRunning() const { return running; }

    // Deprecated: capture source is implicit in device choice now
    enum class CaptureSource { InputDevice = 0, SystemLoopback = 1 };
    void setCaptureSource(CaptureSource) {}
    CaptureSource captureSource() const { return CaptureSource::InputDevice; }

    // Processing params
    void setGain(float g);          // 0.1 .. 10.0
    void setSmoothing(float s);     // 0.0 .. 0.99 (EMA)
    void setBandsCount(int bands);  // 8, 16, 32
    void setCrossovers(float bass_upper_hz, float mid_upper_hz);
    void setFFTSize(int n);         // 512..8192 power-of-two
    int  getFFTSize() const { return fft_size; }
    int  getBandsCount() const;
    float getBassUpperHz() const { return xover_bass_upper; }
    float getMidUpperHz() const { return xover_mid_upper; }
    void setSampleRate(int sr) { if(sr > 0) sample_rate_hz = sr; }
    int  getSampleRate() const { return sample_rate_hz; }

    // Output
    float level() const { return current_level.load(); } // 0..1

    // Feed external PCM16 samples (for WASAPI loopback)
    void FeedPCM16(const int16_t* samples, int count);

    // Spectrum data (FFT-based), 16 bands log-mapped
    std::vector<float> getBands() const;       // 0..1 per band
    float getBassLevel() const;                // approx 20-200 Hz
    float getMidLevel() const;                 // approx 200-2000 Hz
    float getTrebleLevel() const;              // approx 2k-16k Hz
    float getOnsetLevel() const;               // spectral onset strength 0..1
    float getBandEnergyHz(float low_hz, float high_hz) const; // 0..1 avg across bands in Hz range
    // Channel diagnostics
    int getChannelCount() const {
#ifdef _WIN32
        return channel_count;
#else
        return 0;
#endif
    }
    QStringList getChannelNames() const {
#ifdef _WIN32
        return channel_names;
#else
        return QStringList();
#endif
    }
    std::vector<float> getChannelLevels() const {
#ifdef _WIN32
        return channel_levels;
#else
        return {};
#endif
    }

signals:
    void LevelUpdated(float level);

private slots:
    void onLevelTick();

private:
    explicit AudioInputManager(QObject* parent = nullptr);

    void processBuffer(const char* data, int bytes);

private:
    mutable QMutex mutex;
    int selected_index = -1;

    std::atomic<float> current_level{0.0f};
    float ema_smoothing = 0.8f;
    float gain = 1.0f;
    bool running = false;

    QTimer level_timer; // emits LevelUpdated at UI rate

    // FFT / Spectrum
    int fft_size = 1024;                // power of two
    int sample_rate_hz = 48000;         // updated by capturer
    std::vector<float> sample_buffer;   // mono float samples
    std::vector<float> window;          // Hann window
    mutable QMutex bands_mutex;
    std::vector<float> bands16;         // N-band log spectrum (size=bands_count)
    float bass_level = 0.0f;
    float mid_level = 0.0f;
    float treble_level = 0.0f;
    float onset_level = 0.0f;
    std::vector<float> prev_mags;
    int bands_count = 16;
    float xover_bass_upper = 200.0f;    // Hz
    float xover_mid_upper  = 2000.0f;   // Hz

    void ensureWindow();
    void computeSpectrum();

#ifdef _WIN32
    class WasapiCapturer;
    WasapiCapturer* capturer = nullptr;
    // Unified device list
    QStringList device_names;
    std::vector<QString> device_ids;
    std::vector<bool> device_is_loopback;
    // Channel info
    int channel_count = 0;
    unsigned int channel_mask = 0;
    QStringList channel_names;
    std::vector<float> channel_levels; // smoothed 0..1 per channel
#endif
};

#endif // AUDIOINPUTMANAGER_H
