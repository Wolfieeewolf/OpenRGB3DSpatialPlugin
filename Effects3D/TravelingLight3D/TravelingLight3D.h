// SPDX-License-Identifier: GPL-2.0-only

#ifndef TRAVELINGLIGHT3D_H
#define TRAVELINGLIGHT3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class TravelingLight3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit TravelingLight3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("TravelingLight3D", "Traveling Light", "3D Spatial", [](){ return new TravelingLight3D; })

    static std::string const ClassName() { return "TravelingLight3D"; }
    static std::string const UIName() { return "Traveling Light"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    enum Mode { MODE_COMET = 0, MODE_ZIGZAG, MODE_KITT, MODE_COUNT };
    static const char* ModeName(int m);

    int mode = MODE_COMET;
    float tail_size = 0.25f;
    float beam_width = 0.15f;
};

#endif
