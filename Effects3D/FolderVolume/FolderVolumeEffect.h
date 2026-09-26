// SPDX-License-Identifier: GPL-2.0-only

#ifndef FOLDERVOLUMEEFFECT_H
#define FOLDERVOLUMEEFFECT_H

#include "SpatialEffect3D.h"
#include "Shaders/SpatialVolumeFieldAssist.h"
#include "Effects3D/AudioReactiveCommon.h"

#include <QImage>
#include <QMutex>
#include <QString>
#include <limits>
#include <memory>
#include <string>
#include <vector>

class QCheckBox;
class QComboBox;
class QLabel;
class QMovie;
class QPushButton;
class QTimer;

struct FolderVolumeSpec
{
    std::string class_name;
    std::string ui_name;
    std::string category = "Volume";
    std::string description;
    std::string shader_id;
    std::string pattern_key = "pattern_type";
    int pattern_index_default = 0;
    bool finish_depth = false;
    bool finish_spiral = false;
    bool finish_hex = false;
    bool finish_rgb = false;
    bool uses_media = false;
    bool rainbow = false;
    bool needs_frequency = true;
    bool show_speed = false;
    bool show_brightness = false;
    bool show_frequency = false;
    bool show_detail = false;
    bool show_size = false;
    bool show_scale = false;
    bool show_fps = false;
    bool show_color = false;
    bool show_surface = false;
    bool show_position = false;
    bool show_path = false;
    bool show_thickness = false;
    bool show_edge = false;
    bool show_count = false;
    bool show_plane = false;
    bool finish_atlas = false;
    bool finish_hsv = false;
    bool finish_surface = false;
    bool finish_audio = false;
    bool uses_audio = false;
    bool sample_room = false;
    int resolution = 0;
    int user_colors = 0;
    bool strip_colormap = false;
    bool height_bands = false;
    bool pattern_from_kernels = false;
    std::string audio_preset;
    std::string audio_media;
    std::string shader_override;
    std::vector<std::string> params;
    struct Slider
    {
        std::string key;
        QString label;
        QString tip;
        int min = 0;
        int max = 100;
        int value = 0;
        bool save_unit = false;
        bool show_percent = false;
    };
    std::vector<Slider> sliders;
    std::vector<float> flow;
    struct Pattern
    {
        QString name;
        QString tip;
    };
    struct Combo
    {
        std::string key;
        QString label;
        int value = 0;
        std::vector<Pattern> options;
    };
    std::vector<Combo> combos;
    struct Step
    {
        int kind = 0;
        int index = 0;
    };
    std::vector<Step> steps;
    QString pattern_label = QStringLiteral("Pattern:");
    std::vector<RGBColor> colors;
    std::vector<Pattern> patterns;
};

void RegisterFolderVolumeEffects();

class FolderVolumeEffect : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit FolderVolumeEffect(FolderVolumeSpec spec, QWidget* parent = nullptr);
    ~FolderVolumeEffect() override;

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool UsesSpatialSamplingQuantization() const override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

    void SetSpeed(unsigned int speed) override;

private slots:
    void OnBrowseMedia();
    void OnGifFrameTimerTimeout();

private:
    float ParamValue(const std::string& name, float detail, float size) const;
    int SliderRaw(const std::string& key) const;
    int ComboIndex(const std::string& key) const;

    void ClearMovie();
    void LoadMediaFile(const QString& path);
    void PublishDisplayFrame(const QImage& src);
    void ApplyGifPlaybackSpeed();
    void BindMediaAmbienceBlock(class QVBoxLayout* layout);

    FolderVolumeSpec spec_;
    QComboBox* pattern_combo = nullptr;
    int pattern_index = 0;
    std::vector<int> combo_values;
    std::vector<int> slider_values;
    float progress = 0.0f;
    float time_sec = 0.0f;
    float speed_mul = 1.0f;
    float tight_mul = 1.0f;
    float phase_shift = 0.0f;
    float atlas_sx = 2.0f;
    float atlas_sy = 2.0f;
    float atlas_sz = 2.0f;
    float atlas_ax = 1.0f;
    float atlas_az = 1.0f;
    float room_ymin = 0.0f;
    float room_ymax = 1.0f;
    float room_xmin = 0.0f;
    float room_xmax = 1.0f;
    float room_zmin = 0.0f;
    float room_zmax = 1.0f;
    SpatialVolumeFieldAssist volume_assist_;

    // Audio reactive state (populated when spec_.uses_audio)
    AudioReactiveSettings3D audio_settings;
    float audio_smoothed = 0.0f;
    float audio_last_intensity_time = std::numeric_limits<float>::lowest();
    float audio_fill_cached = 0.0f;
    float audio_band_lo = 0.0f;
    float audio_band_mid = 0.0f;
    float audio_band_hi = 0.0f;
    float audio_time_boost = 0.0f;

    // Audio media state — spectrum bands (audio_media == "bands")
    static constexpr int kFvAudioSpectrogramCols = 64;
    static constexpr int kFvAudioSpectrogramRows = 72;
    float audio_band_count_cached = 0.0f;
    float audio_roll_phase_cached = 0.0f;
    float audio_bar_edge_cached   = 0.02f;
    float audio_strip_scroll_cached = 0.0f;
    int   audio_spec_band_start = 0;
    int   audio_spec_band_end   = 0;
    std::vector<float> audio_smoothed_bands;
    // Audio media state — spectrogram (audio_media == "spectrogram")
    int   audio_spectrogram_write_index = 0;
    std::vector<std::vector<float>> audio_spectrogram_history;
    std::vector<float> audio_column_smoothed;
    float audio_last_spectrogram_push = std::numeric_limits<float>::lowest();

    // Pulse state (audio_preset == "beat" || "low_punch")
    struct AudioPulseEntry { float birth_time = 0.0f; float strength = 0.0f; uint32_t color_slot = 0u; };
    AudioPulseTriggerState audio_pulse_trigger;
    std::vector<AudioPulseEntry> audio_pulses;
    uint32_t audio_next_pulse_color_slot = 0u;
    int  audio_packed_pulse_count = 0;
    uint32_t audio_packed_color_slots[5] = {};
    float audio_last_pulse_tick_time = std::numeric_limits<float>::lowest();
    // Pulse geometry cached from grid (updated each PrepareGpuFields)
    float audio_radius_basis_cached = 1.0f;
    float audio_pulse_half_w_cached = 0.05f;
    float audio_max_travel_cached   = 1.0f;
    float audio_pulse_speed_cached  = 0.0f;
    float audio_pulse_hw_cached     = 1.0f;
    float audio_pulse_hh_cached     = 1.0f;
    float audio_pulse_hd_cached     = 1.0f;
    float audio_pulse_decay_cached  = 2.0f;

    // Note state (audio_preset == "high_sparkle")
    float audio_note_hues[4] = {};
    float audio_note_amps[4] = {};
    int   audio_note_count = 0;
    float audio_hi_drive_cached = 0.0f;

    QPushButton* browse_button = nullptr;
    QLabel* path_label = nullptr;
    QCheckBox* tile_repeat_check = nullptr;
    QString media_path;
    QMovie* movie = nullptr;
    QTimer* gif_frame_timer = nullptr;
    bool media_is_gif = false;
    bool tile_repeat_enabled = false;
    QMutex display_mutex;
    std::shared_ptr<QImage> display_frame;
    std::shared_ptr<QImage> previous_display_frame;
    qint64 last_gif_step_ms = 0;
    int gif_step_interval_ms = 0;
};

#endif
