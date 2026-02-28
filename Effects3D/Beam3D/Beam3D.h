// SPDX-License-Identifier: GPL-2.0-only

#ifndef BEAM3D_H
#define BEAM3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class Beam3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Beam3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("Beam3D", "Beam", "3D Spatial", [](){ return new Beam3D; })

    static std::string const ClassName() { return "Beam3D"; }
    static std::string const UIName() { return "Beam"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    enum Mode { MODE_CROSSING = 0, MODE_ROTATING, MODE_COUNT };
    static const char* ModeName(int m);

    int mode = MODE_CROSSING;
    float beam_thickness = 0.08f;
    float beam_width = 0.15f;
    float glow = 0.5f;
};

#endif
