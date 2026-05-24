// SPDX-License-Identifier: GPL-2.0-only

#ifndef AUDIOPULSE_H
#define AUDIOPULSE_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Audio/AudioInputManager.h"
#include "Effects3D/AudioReactiveCommon.h"
#include "EffectStratumBlend.h"
#include <cstdint>
#include <limits>
#include <vector>

class AudioPulse : public SpatialEffect3D
{
    Q_OBJECT

public:
    AudioPulse(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("AudioPulse", "Audio Pulse", "Audio", [](){ return new AudioPulse; })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams&) override;

    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
private:
    struct PulseData
    {
        float birth_time = 0.0f;
        float strength   = 0.0f;
        uint32_t color_slot = 0;
    };

    static float Hash01(float x, float y, float z, unsigned int salt);

    void TickPulses(float time);
    float HeightFade01(float height_norm) const;
    BeatShellHit SampleBeatShell(float distance,
                                 float radius_basis,
                                 float time,
                                 const EffectStratumBlend::BandBlendScalars& bb,
                                 float stratum_mot01,
                                 float height_fade,
                                 float x,
                                 float y,
                                 float z) const;
    float ParticleDebrisAt(float x,
                           float y,
                           float z,
                           float burst_phase,
                           float distance,
                           float shell_radius,
                           int salt) const;

    AudioReactiveSettings3D audio_settings = MakeDefaultBeatAudioReactiveSettings3D();

    std::vector<PulseData> pulses;
    float onset_threshold = 0.28f;
    AudioPulseTriggerState pulse_trigger{};
    float last_tick_time = std::numeric_limits<float>::lowest();
    int particle_amount = 0;
    uint32_t next_pulse_color_slot = 0;
};

#endif
