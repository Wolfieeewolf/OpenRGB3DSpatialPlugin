// SPDX-License-Identifier: GPL-2.0-only

#ifndef PARTICLEFIELD_H
#define PARTICLEFIELD_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include "Shaders/SpatialVolumeFieldAssist.h"

class ParticleField : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit ParticleField(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("ParticleField", "Particle Field", "Spatial", []() { return new ParticleField; })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

    enum Mode : int
    {
        MODE_FLOAT = 0,
        MODE_SNOW = 1,
        MODE_EMBERS = 2,
        MODE_SPARKLE = 3,
        MODE_ATTRACT = 4,
        MODE_RAIN = 5,
        MODE_FIREWORKS = 6,
        MODE_COUNT
    };

    static const char* ModeName(int m);

private:
    void ApplyModeDefaults(int mode_id);
    void SyncCustomUiFromModel();

    static constexpr int kMaxGpuParticles = 48;

    int mode = MODE_FLOAT;
    int particle_count = 36;
    float particle_size = 0.72f;
    float thickness = 0.95f;
    float motion_amount = 1.0f;
    float noise_amount = 0.55f;
    float fill_amount = 1.15f;
    SpatialVolumeFieldAssist volume_assist_;
};

#endif
