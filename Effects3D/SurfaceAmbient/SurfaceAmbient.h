// SPDX-License-Identifier: GPL-2.0-only

#ifndef SURFACEAMBIENT_H
#define SURFACEAMBIENT_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"

class StratumBandPanel;

class SurfaceAmbient : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit SurfaceAmbient(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("SurfaceAmbient", "Surface Fire/Water/Slime", "Spatial", [](){ return new SurfaceAmbient; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnStratumBandChanged();

private:
    enum Style { STYLE_FIRE = 0, STYLE_WATER, STYLE_SLIME, STYLE_LAVA, STYLE_EMBER, STYLE_OCEAN, STYLE_STEAM, STYLE_COUNT };
    enum SurfaceMask {
        SURF_FLOOR  = 1,
        SURF_CEIL   = 2,
        SURF_WALL_XM = 4,
        SURF_WALL_XP = 8,
        SURF_WALL_ZM = 16,
        SURF_WALL_ZP = 32
    };
    static const char* StyleName(int s);
    static float EvalPlasmaStyle(int style, float u, float v, float dist_norm, float time, float freq, float speed);

    int style = STYLE_FIRE;
    float height_pct = 0.25f;
    float thickness = 0.08f;

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};
};

#endif
