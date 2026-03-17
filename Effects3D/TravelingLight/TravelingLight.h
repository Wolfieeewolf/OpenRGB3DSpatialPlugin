// SPDX-License-Identifier: GPL-2.0-only

#ifndef TRAVELINGLIGHT_H
#define TRAVELINGLIGHT_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class TravelingLight : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit TravelingLight(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("TravelingLight", "Traveling Light", "Spatial", [](){ return new TravelingLight; })

    static std::string const ClassName() { return "TravelingLight"; }
    static std::string const UIName() { return "Traveling Light"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    enum Mode {
        MODE_COMET = 0,
        MODE_CHASE,
        MODE_MARQUEE,
        MODE_ZIGZAG,
        MODE_KITT,
        MODE_WIPE,
        MODE_MOVING_PANES,
        MODE_CROSSING,
        MODE_ROTATING,
        MODE_COUNT
    };
    static const char* ModeName(int m);
    float smoothstep(float edge0, float edge1, float x) const;

    int mode = MODE_COMET;
    float glow = 0.5f;
    int wipe_edge_shape = 0;
    int num_divisions = 4;
};

#endif
