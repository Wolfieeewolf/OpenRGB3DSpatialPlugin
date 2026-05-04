// SPDX-License-Identifier: GPL-2.0-only

#ifndef WIREFRAMECUBE_H
#define WIREFRAMECUBE_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"

class StratumBandPanel;
class StripKernelColormapPanel;

class WireframeCube : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit WireframeCube(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("WireframeCube", "Wireframe Cube", "Spatial", [](){ return new WireframeCube; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnStratumBandChanged();
    void SyncStripColormapFromPanel();

private:
    static float PointToSegmentDistance(float px, float py, float pz,
                                        float ax, float ay, float az,
                                        float bx, float by, float bz);

    float thickness = 0.08f;
    float line_brightness = 1.0f;

    float cube_cache_time = -1e9f;
    float cube_corners[8][3];
    float cached_angle_deg = 0.0f;

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool wireframecube_strip_cmap_on = false;
    int wireframecube_strip_cmap_kernel = 0;
    float wireframecube_strip_cmap_rep = 4.0f;
    int wireframecube_strip_cmap_unfold = 0;
    float wireframecube_strip_cmap_dir = 0.0f;
    int wireframecube_strip_cmap_color_style = 0;
};

#endif
