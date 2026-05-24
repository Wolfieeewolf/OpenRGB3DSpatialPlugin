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
    float getSmoothing() const { return ema_smoothing; }
    void setMixClarity(float clarity_0_to_1);
    float getMixClarity() const;
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

    float getBandPeakDecay() const { return band_peak_decay; }
    void  setBandPeakDecay(float d);
    float getBassPeakDecay() const { return bass_peak_decay; }
    void  setBassPeakDecay(float d);
    float getActivityPeakDecay() const { return activity_peak_decay; }
    void  setActivityPeakDecay(float d);
    float getVisualizerPeakDecay() const { return visualizer_peak_decay; }
    void  setVisualizerPeakDecay(float d);
    float getVisualizerFloor() const { return visualizer_floor; }
    void  setVisualizerFloor(float f);
    float getAutoLevelPeakDecay() const { return auto_level_peak_decay; }
    void  setAutoLevelPeakDecay(float d);
    float getAutoLevelFloorDecay() const { return auto_level_floor_decay; }
    void  setAutoLevelFloorDecay(float d);

    void resetAnalyzerTuning();

    void FeedPCM16(const int16_t* samples, int count);

    std::vector<float> getBands() const;
    void getBands(std::vector<float>& out) const;
    float getBassLevel() const;
    float getMidLevel() const;
    float getTrebleLevel() const;
    float getOnsetLevel() const;
    float getBandOnsetLevel(float low_hz, float high_hz, float extra_isolation = 0.0f) const;
    float getBandTransientEnergyHz(float low_hz, float high_hz, float extra_isolation = 0.0f) const;
    float getBandSlowEnergyHz(float low_hz, float high_hz, float extra_isolation = 0.0f) const;
    float getBandEnergyHz(float low_hz, float high_hz, float extra_isolation = 0.0f) const;
    float getBandEnergyHzWithGain(float low_hz, float high_hz, const float* band_gain_16) const;

    void setBandIsolation(float isolation_0_to_1);
    float getBandIsolation() const;
    void applyEqMixPreset(int preset_id);
    int getEqBandCount() const;
    static float AnalysisBandCenterHz(int band_index, int bands_count, int sample_rate_hz, int fft_size);
    static void AnalysisBandHzRange(int band_index,
                                    int bands_count,
                                    int sample_rate_hz,
                                    int fft_size,
                                    float& low_hz,
                                    float& high_hz);

    static constexpr int REFERENCE_EQ_BANDS = 16;

    void setEqGain(int band_index, float gain);
    float getEqGain(int band_index) const;
    void resetEq();

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

    struct StreamStemLevels
    {
        float kick = 0.0f;
        float snare = 0.0f;
        float hihat = 0.0f;
        float bass = 0.0f;
    };

    StreamStemLevels getStreamStemLevels() const;
    float getStereoWidth() const;

signals:
    void LevelUpdated(float level);

private slots:
    void onLevelTick();

private:
    explicit AudioInputManager(QObject* parent = nullptr);

    void processBuffer(const char* data, int bytes);
    void updateChannelLevels(const std::vector<float>& levels);
    void updateVisualizerBuckets(const std::vector<float>& mags, float min_hz, float max_hz);
    void updateStreamStemsLocked();

private:
    mutable QMutex mutex;
    int selected_index = -1;

    std::atomic<float> current_level{0.0f};
    float ema_smoothing = 0.8f;
    float mix_clarity = 0.6f;
    float band_isolation = 0.55f;
    float stereo_width = 0.35f;
    float stream_kick = 0.0f;
    float stream_snare = 0.0f;
    float stream_hihat = 0.0f;
    float stream_bass = 0.0f;
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
    float bass_level = 0.0f;
    float mid_level = 0.0f;
    float treble_level = 0.0f;
    float onset_level = 0.0f;
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
