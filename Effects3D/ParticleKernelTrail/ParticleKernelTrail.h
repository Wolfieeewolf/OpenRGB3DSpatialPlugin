// SPDX-License-Identifier: GPL-2.0-only

#ifndef PARTICLEKERNELTRAIL_H
#define PARTICLEKERNELTRAIL_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include <vector>

class StripKernelColormapPanel;
class StratumBandPanel;

/** Particles drift in the volume; each smears strip-kernel phase for a soft temporal trail. */
class ParticleKernelTrail : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit ParticleKernelTrail(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("ParticleKernelTrail", "Particle Kernel Trail", "Spatial", []() {
        return new ParticleKernelTrail;
    });

    static std::string const ClassName() { return "ParticleKernelTrail"; }
    static std::string const UIName() { return "Particle Kernel Trail"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnStratumBandChanged();
    void SyncStripColormapFromPanel();

private:
    struct Particle
    {
        float x = 0.5f, y = 0.5f, z = 0.5f;
        float vx = 0.0f, vy = 0.0f, vz = 0.0f;
        float trail_p01 = 0.0f;
    };

    void EnsureParticles(int count, unsigned int seed);
    void TickParticles(float time, const GridContext3D& grid);

    int particle_count = 16;
    float particle_radius = 0.14f;
    float trail_lerp = 0.12f; /* higher = snappier, lower = longer smear */
    float last_tick_time = -1e9f;
    std::vector<Particle> particles;
    unsigned int particle_seed = 1u;

    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool pktrail_strip_cmap_on = true;
    int pktrail_strip_cmap_kernel = 0;
    float pktrail_strip_cmap_rep = 4.0f;
    int pktrail_strip_cmap_unfold = 0;
    float pktrail_strip_cmap_dir = 0.0f;
    int pktrail_strip_cmap_color_style = 0;

    StratumBandPanel* stratum_panel = nullptr;
    int stratum_layout_mode = 0;
    EffectStratumBlend::BandTuningPct stratum_tuning_{};
};

#endif
