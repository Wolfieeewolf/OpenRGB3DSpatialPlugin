// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPECTRUMBARS_H
#define SPECTRUMBARS_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Effects3D/AudioReactiveCommon.h"
#include "EffectStratumBlend.h"
#include "SpatialVolumeFieldAssist.h"
#include <vector>
#include <limits>

class SpectrumBars : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit SpectrumBars(QWidget* parent = nullptr);
    ~SpectrumBars() override;

    EFFECT_REGISTERER_3D("SpectrumBars", "Spectrum Bars", "Audio", [](){ return new SpectrumBars; });

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    void RefreshBandRange();
    void EnsureSpectrumCache(float time);
    void UpdateSmoothedBands(const std::vector<float>& spectrum, float delta_time);
    void UploadBandsMedia();

    AudioReactiveSettings3D audio_settings = MakeDefaultSpectrumAudioReactiveSettings3D();
    float roll_speed = 0.0f;
    int band_start = 0;
    int band_end = -1;

    std::vector<float> smoothed_bands;
    std::vector<float> bands_cache;
    float last_sample_time = std::numeric_limits<float>::lowest();

    SpatialVolumeFieldAssist volume_assist_;
};

#endif
