// SPDX-License-Identifier: GPL-2.0-only

#ifndef CUBELAYER3D_H
#define CUBELAYER3D_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Audio/AudioInputManager.h"
#include "Effects3D/AudioReactiveCommon.h"
#include <limits>

class CubeLayer3D : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit CubeLayer3D(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("CubeLayer3D", "Cube Layer", "Audio", [](){ return new CubeLayer3D; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    float EvaluateIntensity(float amplitude, float time);
    static float AxisPosition(int axis, float x, float y, float z,
                              float min_x, float max_x, float min_y, float max_y, float min_z, float max_z);

    AudioReactiveSettings3D audio_settings = MakeDefaultAudioReactiveSettings3D(20, 20000);
    float smoothed = 0.0f;
    float last_intensity_time = std::numeric_limits<float>::lowest();
    float layer_thickness = 0.12f;
};

#endif
