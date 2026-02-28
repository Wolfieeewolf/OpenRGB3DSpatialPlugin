// SPDX-License-Identifier: GPL-2.0-only

#ifndef ZIGZAG3D_H
#define ZIGZAG3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class ZigZag3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit ZigZag3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("ZigZag3D", "ZigZag", "3D Spatial", [](){ return new ZigZag3D; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    enum Mode { MODE_ZIGZAG = 0, MODE_MARQUEE, MODE_COUNT };
    static const char* ModeName(int m);
    int path_mode = MODE_ZIGZAG;
    float tail_length = 0.3f;
};

#endif
