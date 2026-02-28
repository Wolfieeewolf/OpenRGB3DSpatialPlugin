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
#include <QMutexLocker>
#include <QTimer>
#include <QString>
#include <QStringList>
#include <atomic>
#include <vector>

class AudioInputManager : public QObject
{
    Q_OBJECT
public:
    static AudioInputManager* instance();

    QStringList listInputDevices();
    int defaultDeviceIndex() const;
    void setDeviceByIndex(int index);

    void start();
    void stop();
    bool isRunning() const { return running; }

    enum class CaptureSource { InputDevice = 0, SystemLoopback = 1 };
    void setCaptureSource(CaptureSource) {}
    CaptureSource captureSource() const { return CaptureSource::InputDevice; }

    void setGain(float g);
    void setSmoothing(float s);
    void setBandsCount(int bands);
    void setCrossovers(float bass_upper_hz, float mid_upper_hz);
    void setFFTSize(int n);
    int  getFFTSize() const { return fft_size; }
    int  getBandsCount() const;
    float getBassUpperHz() const { return xover_bass_upper; }
    float getMidUpperHz() const { return xover_mid_upper; }
    void setSampleRate(int sr) { if(sr > 0) sample_rate_hz = sr; }
    int  getSampleRate() const { return sample_rate_hz; }

    float level() const { return current_level.load(); }

    void setAutoLevelEnabled(bool enabled);
    bool isAutoLevelEnabled() const { return auto_level_enabled; }
    void resetAutoLevel();

    void FeedPCM16(const int16_t* samples, int count);

    std::vector<float> getBands() const;
    void getBands(std::vector<float>& out) const;
    float getBassLevel() const;
    float getMidLevel() const;
    float getTrebleLevel() const;
    float getOnsetLevel() const;
    float getBandEnergyHz(float low_hz, float high_hz) const;

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
        QMutexLocker lock(&mutex);
        return channel_levels;
#else
        return {};
#endif
    }

    struct SpectrumSnapshot
    {
        std::vector<float> bins;
        std::vector<float> peaks;
        float min_frequency_hz = 0.0f;
        float max_frequency_hz = 0.0f;
    };

    SpectrumSnapshot getSpectrumSnapshot(int target_bins = 256) const;

signals:
    void LevelUpdated(float level);

private slots:
    void onLevelTick();

private:
    explicit AudioInputManager(QObject* parent = nullptr);

    void processBuffer(const char* data, int bytes);
    void updateChannelLevels(const std::vector<float>& levels);
    void updateVisualizerBuckets(const std::vector<float>& mags, float min_hz, float max_hz);

private:
    mutable QMutex mutex;
    int selected_index = -1;

    std::atomic<float> current_level{0.0f};
    float ema_smoothing = 0.8f;
    float gain = 1.0f;
    bool auto_level_enabled = true;
    float auto_level_peak = 0.0025f;
    float auto_level_floor = 0.0006f;
    float auto_level_min_peak = 0.0006f;
    float auto_level_min_range = 0.01f;
    float auto_level_peak_decay = 0.995f;
    float auto_level_floor_decay = 0.9995f;
    float auto_level_floor_rise = 0.05f;

    bool running = false;

    QTimer level_timer;

    int fft_size = 1024;
    int sample_rate_hz = 48000;
    std::vector<float> sample_buffer;
    std::vector<float> window;
    mutable QMutex bands_mutex;
    std::vector<float> bands16;
    float bass_level = 0.0f;
    float mid_level = 0.0f;
    float treble_level = 0.0f;
    float onset_level = 0.0f;
    std::vector<float> prev_mags;
    int bands_count = 16;
    float xover_bass_upper = 200.0f;
    float xover_mid_upper  = 2000.0f;

    std::vector<float> visualizer_bins;
    std::vector<float> visualizer_peaks;
    float visualizer_min_hz = 0.0f;
    float visualizer_max_hz = 0.0f;
    float visualizer_peak_decay = 0.92f;
    float visualizer_floor = 1e-4f;

    void ensureWindow();
    void computeSpectrum();

#ifdef _WIN32
    class WasapiCapturer;
    WasapiCapturer* capturer = nullptr;
    QStringList device_names;
    std::vector<QString> device_ids;
    std::vector<bool> device_is_loopback;
    int channel_count = 0;
    unsigned int channel_mask = 0;
    QStringList channel_names;
    std::vector<float> channel_levels;
#endif
};

#endif // AUDIOINPUTMANAGER_H
