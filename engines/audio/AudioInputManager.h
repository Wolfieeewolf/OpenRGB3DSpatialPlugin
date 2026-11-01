// SPDX-License-Identifier: GPL-2.0-only

#ifndef AUDIOINPUTMANAGER_H
#define AUDIOINPUTMANAGER_H

#include <QObject>
#include <QMutex>
#include <QMutexLocker>
#include <QRecursiveMutex>
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
    void setDeviceByIndex(int index);

    void start();
    void stop();
    bool isRunning() const { return running; }

    void setGain(float g);
    void setSmoothing(float s);
    float getSmoothing() const { return ema_smoothing; }
    void setBandsCount(int bands);
    void setFFTSize(int n);
    int  getFFTSize() const { return fft_size; }
    int  getBandsCount() const;
    void setSampleRate(int sr) { if(sr > 0) sample_rate_hz = sr; }
    int  getSampleRate() const { return sample_rate_hz; }

    void setAutoLevelEnabled(bool enabled);
    bool isAutoLevelEnabled() const { return auto_level_enabled; }
    void resetAutoLevel();
    void resetAnalyzerTuning();

    void FeedPCM16(const int16_t* samples, int count);

    void getBands(std::vector<float>& out) const;
    float getOnsetLevel() const;
    float getBandOnsetLevel(float low_hz, float high_hz) const;
    float getBandTransientEnergyHz(float low_hz, float high_hz) const;
    float getBandSlowEnergyHz(float low_hz, float high_hz) const;

    static constexpr int kChromaBins = 12;
    static constexpr int kMaxNotePeaks = 8;

    struct NotePeak
    {
        float mean = 0.0f;
        float amp = 0.0f;
    };

    std::vector<NotePeak> getActiveNotes() const;
    float getNoteEnergyInHz(float low_hz, float high_hz) const;
    float getDominantNoteHue01() const;
    float getNoteDrive01() const;

    int getEqBandCount() const;
    static float AnalysisBandCenterHz(int band_index, int bands_count, int sample_rate_hz, int fft_size);
    static void AnalysisBandHzRange(int band_index,
                                    int bands_count,
                                    int sample_rate_hz,
                                    int fft_size,
                                    float& low_hz,
                                    float& high_hz);

    void setEqGain(int band_index, float gain);
    float getEqGain(int band_index) const;
    void resetEq();

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
    void updateVisualizerBuckets(const std::vector<float>& mags, float min_hz, float max_hz);

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

    float band_peak_decay = 0.994f;
    float bass_peak_decay = 0.998f;
    float activity_peak_decay = 0.992f;

    bool running = false;

    QTimer level_timer;

    int fft_size = 512;
    int sample_rate_hz = 48000;
    std::vector<float> sample_buffer;
    std::vector<float> window;
    mutable QRecursiveMutex bands_mutex;
    std::vector<float> bands16;
    float onset_level = 0.0f;
    float onset_flux_mean = 0.08f;
    std::vector<float> chroma12;
    float chroma_peak = 0.05f;
    NotePeak note_peaks[kMaxNotePeaks]{};
    int note_peak_count = 0;
    float note_drive = 0.0f;
    float dominant_note_hue01 = 0.0f;
    std::vector<float> prev_mags;
    std::vector<float> prev_band_frame;
    std::vector<float> band_slow;
    std::vector<float> band_flux;
    std::vector<float> band_transient;
    int bands_count = 8;
    std::vector<float> band_peak_smoothed;
    std::vector<float> band_noise_floor;
    std::vector<float> band_peak_activity;
    std::vector<float> eq_gain;
    void ensureEqGainSizeLocked();

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
#endif
};

#endif
