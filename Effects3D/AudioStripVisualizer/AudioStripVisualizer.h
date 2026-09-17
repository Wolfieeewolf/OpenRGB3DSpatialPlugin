// SPDX-License-Identifier: GPL-2.0-only

#ifndef AUDIOSTRIPVISUALIZER_H
#define AUDIOSTRIPVISUALIZER_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Effects3D/AudioReactiveCommon.h"
#include "SpatialVolumeFieldAssist.h"
#include <vector>
#include <limits>

class AudioStripVisualizer : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit AudioStripVisualizer(QWidget* parent = nullptr);
    ~AudioStripVisualizer() override = default;

    EFFECT_REGISTERER_3D("AudioStripVisualizer", "Audio Strip Visualizer", "Audio", []() {
        return new AudioStripVisualizer;
    })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    enum DisplayMode
    {
        MODE_BARS = 0,
        MODE_SPECTROGRAM = 1
    };

    void RefreshSpectrumColumns();
    void PushSpectrogramHistory();
    void UploadMediaTexture();
    RGBColor ComposeStripColor(float path01, float energy, float time, const GridContext3D& grid, float x, float y, float z,
                             const Vector3D& origin, const Vector3D& rotated_pos, float stratum_phase01,
                             const EffectStratumBlend::BandBlendScalars& bb);

    AudioReactiveSettings3D audio_settings = MakeDefaultSpectrumAudioReactiveSettings3D();
    int display_mode = MODE_BARS;
    float scroll_speed = 0.35f;
    bool mirror_bars = false;

    std::vector<float> column_levels;
    std::vector<float> column_smoothed;
    std::vector<std::vector<float>> spectrogram_history;
    int spectrogram_write_index = 0;
    float last_push_time = std::numeric_limits<float>::lowest();

    SpatialVolumeFieldAssist volume_assist_;
};

#endif
