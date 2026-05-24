// SPDX-License-Identifier: GPL-2.0-only

#ifndef DISCOFLASH_H
#define DISCOFLASH_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Audio/AudioInputManager.h"
#include "Effects3D/AudioReactiveCommon.h"
#include "EffectStratumBlend.h"
#include <cstdint>
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
    float EffectiveFlashRadius() const;
    void TickFlashes(float time);
    void SpawnPointFlashes(float time, float strength, uint32_t color_slot);
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
        float nx, ny, nz;
        float size;
        uint32_t color_slot = 0;
    };

    AudioReactiveSettings3D audio_settings = MakeDefaultBeatAudioReactiveSettings3D();
    std::vector<Flash> flashes;

    float last_tick_time = std::numeric_limits<float>::lowest();
    AudioPulseTriggerState pulse_trigger{};

    float flash_decay = 3.2f;
    float flash_density = 0.40f;
    float onset_threshold = 0.30f;

    enum Mode { MODE_BEAT = 0, MODE_SPARKLE, MODE_COUNT };
    int flash_mode = MODE_BEAT;

    std::mt19937 rng{42};
    uint32_t next_pulse_color_slot = 0;
};

#endif
