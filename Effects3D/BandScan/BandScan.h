// SPDX-License-Identifier: GPL-2.0-only

#ifndef BANDSCAN_H
#define BANDSCAN_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Audio/AudioInputManager.h"
#include "Effects3D/AudioReactiveCommon.h"
#include "EffectStratumBlend.h"
#include <vector>
#include <limits>

class StratumBandPanel;
class StripKernelColormapPanel;

class BandScan : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit BandScan(QWidget* parent = nullptr);
    ~BandScan() override = default;

    EFFECT_REGISTERER_3D("BandScan", "Band Scan", "Audio", [](){ return new BandScan; })

    static std::string const ClassName() { return "BandScan"; }
    static std::string const UIName() { return "Band Scan"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnStratumBandChanged();
    void SyncStripColormapFromPanel();

private:
    void RefreshBandRange();
    void EnsureSpectrumCache(float time);
    void UpdateSmoothedBands(const std::vector<float>& spectrum);
    float ResolveCoordinateNormalized(const GridContext3D* grid, float x, float y, float z) const;
    float ResolveHeightNormalized(const GridContext3D* grid, float x, float y, float z) const;
    float ResolveRadialNormalized(const GridContext3D* grid, float x, float y, float z) const;
    RGBColor ComposeColor(float axis_pos,
                          float height_norm,
                          float radial_norm,
                          float time,
                          float brightness,
                          const RGBColor& axis_color,
                          bool rainbow_mode,
                          float stratum_speed_mul,
                          float stratum_tight_mul,
                          float stratum_phase01,
                          float strip_cmap_p01) const;
    float WrapDistance(float a, float b, int modulo) const;

    AudioReactiveSettings3D audio_settings = MakeDefaultAudioReactiveSettings3D(20, 20000);
    int band_start = 0;
    int band_end = -1;

    std::vector<float> smoothed_bands;
    std::vector<float> bands_cache;
    float last_sample_time = std::numeric_limits<float>::lowest();

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool bandscan_strip_cmap_on = false;
    int bandscan_strip_cmap_kernel = 0;
    float bandscan_strip_cmap_rep = 4.0f;
    int bandscan_strip_cmap_unfold = 0;
    float bandscan_strip_cmap_dir = 0.0f;
    int bandscan_strip_cmap_color_style = 0;
};

#endif // BANDSCAN_H
