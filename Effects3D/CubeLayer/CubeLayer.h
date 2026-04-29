// SPDX-License-Identifier: GPL-2.0-only

#ifndef CUBELAYER_H
#define CUBELAYER_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Audio/AudioInputManager.h"
#include "Effects3D/AudioReactiveCommon.h"
#include "EffectStratumBlend.h"
#include <limits>

class StratumBandPanel;

class CubeLayer : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit CubeLayer(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("CubeLayer", "Cube Layer", "Audio", [](){ return new CubeLayer; })

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnStratumBandChanged();

private:
    float EvaluateIntensity(float amplitude, float time);
    static float AxisPosition(int axis, float x, float y, float z,
                              float min_x, float max_x, float min_y, float max_y, float min_z, float max_z);
    float smoothstep(float edge0, float edge1, float x) const;

    AudioReactiveSettings3D audio_settings = MakeDefaultAudioReactiveSettings3D(20, 20000);
    float smoothed = 0.0f;
    float last_intensity_time = std::numeric_limits<float>::lowest();
    float layer_thickness = 0.12f;
    int layer_edge_shape = 0;  /* 0=Round, 1=Sharp, 2=Square */
    QComboBox* layer_edge_combo = nullptr;

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};
};

#endif
