// SPDX-License-Identifier: GPL-2.0-only

#ifndef WAVESURFACE3D_H
#define WAVESURFACE3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class WaveSurface3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit WaveSurface3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("WaveSurface3D", "Wave Surface", "3D Spatial", [](){ return new WaveSurface3D; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    enum WaveStyle { STYLE_SINUS = 0, STYLE_RADIAL, STYLE_LINEAR, STYLE_PACIFICA, STYLE_GRADIENT, STYLE_COUNT };
    static const char* WaveStyleName(int s);

    int wave_style = STYLE_SINUS;
    float surface_thickness = 0.08f;
    float wave_frequency = 1.0f;
    float wave_amplitude = 1.0f;
    float wave_direction_deg = 0.0f;
};

#endif
