// SPDX-License-Identifier: GPL-2.0-only

#ifndef BANDSCAN3D_H
#define BANDSCAN3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Audio/AudioInputManager.h"
#include "Effects3D/AudioReactiveCommon.h"
#include <vector>
#include <limits>

class BandScan3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit BandScan3D(QWidget* parent = nullptr);
    ~BandScan3D() override = default;

    EFFECT_REGISTERER_3D("BandScan3D", "Band Scan", "Audio", [](){ return new BandScan3D; })

    static std::string const ClassName() { return "BandScan3D"; }
    static std::string const UIName() { return "Band Scan 3D"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    void RefreshBandRange();
    void EnsureSpectrumCache(float time);
    void UpdateSmoothedBands(const std::vector<float>& spectrum);
    float ResolveCoordinateNormalized(const GridContext3D* grid, float x, float y, float z) const;
    float ResolveHeightNormalized(const GridContext3D* grid, float x, float y, float z) const;
    float ResolveRadialNormalized(const GridContext3D* grid, float x, float y, float z) const;
    RGBColor ComposeColor(float axis_pos, float height_norm, float radial_norm, float time, float brightness, const RGBColor& axis_color, bool rainbow_mode) const;
    float WrapDistance(float a, float b, int modulo) const;

    AudioReactiveSettings3D audio_settings = MakeDefaultAudioReactiveSettings3D(20, 20000);
    int band_start = 0;
    int band_end = -1;

    std::vector<float> smoothed_bands;
    float last_sample_time = std::numeric_limits<float>::lowest();
};

#endif // BANDSCAN3D_H
