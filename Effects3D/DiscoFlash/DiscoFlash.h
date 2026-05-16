// SPDX-License-Identifier: GPL-2.0-only

#ifndef DISCOFLASH_H
#define DISCOFLASH_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Audio/AudioInputManager.h"
#include "Effects3D/AudioReactiveCommon.h"
#include "EffectStratumBlend.h"
#include <vector>
#include <limits>
#include <random>

class DiscoFlash : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit DiscoFlash(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("DiscoFlash", "Disco Flash", "Audio", [](){ return new DiscoFlash; })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
private:
    void TickFlashes(float time);
    float ApplyDiscoFlashRoomHue(float hue_deg,
                                 float x,
                                 float y,
                                 float z,
                                 float time,
                                 const GridContext3D& grid,
                                 const Vector3D& origin,
                                 const Vector3D& rp) const;
    RGBColor SampleFlashField(float x,
                              float y,
                              float z,
                              float nx,
                              float ny,
                              float nz,
                              float time,
                              const EffectStratumBlend::BandBlendScalars& bb,
                              float stratum_mot01,
                              const GridContext3D& grid,
                              const Vector3D& origin,
                              const Vector3D& rp);

    struct Flash
    {
        float birth_time;
        float hue;
        float nx, ny, nz;
        float size;
    };

    AudioReactiveSettings3D audio_settings = MakeDefaultAudioReactiveSettings3D(20, 200);
    std::vector<Flash> flashes;

    float last_tick_time = std::numeric_limits<float>::lowest();
    float onset_smoothed = 0.0f;
    float onset_hold = 0.0f;

    float flash_decay = 3.5f;
    float flash_density = 0.35f;
    float flash_size = 0.25f;
    float onset_threshold = 0.5f;

    enum Mode { MODE_BEAT = 0, MODE_SPARKLE, MODE_COUNT };
    int flash_mode = MODE_BEAT;

    std::mt19937 rng{42};
};

#endif
