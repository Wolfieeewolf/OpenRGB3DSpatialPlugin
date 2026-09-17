// SPDX-License-Identifier: GPL-2.0-only

#ifndef PLASMA_H
#define PLASMA_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include "Shaders/SpatialVolumeFieldAssist.h"

class QComboBox;

class Plasma : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Plasma(QWidget* parent = nullptr);
    ~Plasma();

    EFFECT_REGISTERER_3D("Plasma", "Plasma", "Spatial", [](){return new Plasma;});

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnPlasmaParameterChanged();

private:
    QComboBox* pattern_combo = nullptr;
    int pattern_type = 0;
    float progress = 0.0f;
    SpatialVolumeFieldAssist volume_assist_;
};

#endif
