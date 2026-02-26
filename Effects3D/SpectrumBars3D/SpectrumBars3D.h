// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPECTRUMBARS3D_H
#define SPECTRUMBARS3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Audio/AudioInputManager.h"
#include "Effects3D/AudioReactiveCommon.h"
#include <vector>
#include <limits>

class SpectrumBars3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit SpectrumBars3D(QWidget* parent = nullptr);
    ~SpectrumBars3D() override;

    // Auto-registration system
    EFFECT_REGISTERER_3D("SpectrumBars3D", "Spectrum Bars", "Audio", [](){ return new SpectrumBars3D; });

    static std::string const ClassName() { return "SpectrumBars3D"; }
    static std::string const UIName() { return "Spectrum Bars 3D"; }

    // Pure virtual implementations
    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }

    // Settings persistence
    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    void RefreshBandRange();
    void EnsureSpectrumCache(float time);
    void UpdateSmoothedBands(const std::vector<float>& spectrum, float delta_time);
    float ResolveCoordinateNormalized(const GridContext3D* grid, float x, float y, float z) const;
    float ResolveHeightNormalized(const GridContext3D* grid, float x, float y, float z) const;
    float ResolveRadialNormalized(const GridContext3D* grid, float x, float y, float z) const;
    RGBColor ComposeColor(float axis_pos, float height_norm, float radial_norm, float time, float brightness, const RGBColor& user_color) const;

    /*---------------------------------------------------------*\
    | Audio-specific parameters                                |
    | (Controlled by standard Audio Controls panel)           |
    \*---------------------------------------------------------*/
    AudioReactiveSettings3D audio_settings = MakeDefaultAudioReactiveSettings3D(20, 20000);
    int band_start = 0; // inclusive (auto-calculated from low_hz)
    int band_end = -1;  // inclusive (-1 = auto to last, auto-calculated from high_hz)

    std::vector<float> smoothed_bands;
    std::vector<float> bands_cache;
    float last_sample_time = std::numeric_limits<float>::lowest();
};

#endif // SPECTRUMBARS3D_H
