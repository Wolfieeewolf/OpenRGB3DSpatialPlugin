// SPDX-License-Identifier: GPL-2.0-only

#ifndef AUDIOPAINTBRUSH_H
#define AUDIOPAINTBRUSH_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Effects3D/AudioReactiveCommon.h"
#include "EffectStratumBlend.h"
#include "SpatialVolumeFieldAssist.h"
#include <limits>

/** Soft 3D Lissajous ribbons driven by bass / mid / treble (MoonLight PaintBrush inspired). */
class AudioPaintBrush : public SpatialEffect3D
{
    Q_OBJECT
public:
    explicit AudioPaintBrush(QWidget* parent = nullptr);

    EFFECT_REGISTERER_3D("AudioPaintBrush", "Audio Paintbrush", "Audio",
                         [](){ return new AudioPaintBrush; })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    AudioReactiveSettings3D audio_settings = MakeDefaultSpectrumAudioReactiveSettings3D();
    float thickness = 0.07f;
    float chaos = 0.45f;
    int ribbon_count = 3;
    float bass_s = 0.0f;
    float mid_s = 0.0f;
    float high_s = 0.0f;
    float last_tick = std::numeric_limits<float>::lowest();
    float time_boost = 0.0f;
    SpatialVolumeFieldAssist volume_assist_;
};

#endif
