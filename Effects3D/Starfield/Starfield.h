// SPDX-License-Identifier: GPL-2.0-only

#ifndef STARFIELD_H
#define STARFIELD_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include "Shaders/SpatialVolumeFieldAssist.h"

class Starfield : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit Starfield(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("Starfield", "Space", "Spatial", [](){ return new Starfield; })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    enum Mode {
        MODE_STARS = 0,
        MODE_TWINKLE,
        MODE_WARP,
        MODE_HYPERDRIVE,
        MODE_BLACKHOLE,
        MODE_WORMHOLE,
        MODE_COUNT
    };
    static constexpr int kMaxGpuParticles = 48;
    static constexpr int kMaxCpuParticles = 36;
    static const char* ModeName(int m);

    struct ViewSample
    {
        float vx = 0.0f;
        float vy = 0.0f;
        float vz = 0.0f;
        float radial = 0.0f;
        float ang = 0.0f;
    };

    struct EvalContext
    {
        Vector3D origin{};
        Vector3D rp{};
        ViewSample view{};
        EffectGridAxisHalfExtents e{};
        float h_scale = 1.0f;
        float progress = 0.0f;
        float speed = 1.0f;
        float size_m = 1.0f;
        float detail = 1.0f;
        float fill = 1.0f;
        float thickness = 0.08f;
        float drift = 0.15f;
        float twinkle = 0.25f;
        float color_cycle = 0.0f;
        float strip_p01 = 0.0f;
        bool strat_on = false;
        EffectStratumBlend::BandBlendScalars bb{};
        float stratum_mot01 = 0.0f;
        float time = 0.0f;
        const GridContext3D* grid = nullptr;
        int particle_count = 32;
    };

    ViewSample MakeViewSample(const Vector3D& rp, const Vector3D& origin, const EffectGridAxisHalfExtents& e, float fill) const;
    RGBColor ResolveSpaceColor(const EvalContext& ctx, float pos01, float hue_shift) const;
    RGBColor FinishSample(const EvalContext& ctx, float intensity, float palette01, float hotness, int mode_i) const;
    RGBColor AccumParticles(const EvalContext& ctx, int mode_i) const;
    RGBColor EvalBlackhole(const EvalContext& ctx) const;
    RGBColor EvalWormhole(const EvalContext& ctx) const;

    int mode = MODE_STARS;
    int num_stars = 32;
    float star_size = 0.10f;
    float drift_amount = 0.12f;
    float twinkle_speed = 0.45f;
    float fill_amount = 1.0f;
    SpatialVolumeFieldAssist volume_assist_;
};

#endif
