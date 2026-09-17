// SPDX-License-Identifier: GPL-2.0-only

#ifndef NOTESPARKLE_H
#define NOTESPARKLE_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Effects3D/AudioReactiveCommon.h"
#include "EffectStratumBlend.h"
#include "SpatialVolumeFieldAssist.h"
#include <cstdint>
#include <limits>

/** High/note particle cloud — ColorChord hues; strip, zone, or room via stack bounds. */
class NoteSparkle : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit NoteSparkle(QWidget* parent = nullptr);
    ~NoteSparkle() override = default;

    EFFECT_REGISTERER_3D("NoteSparkle", "Note Sparkle", "Audio", []() { return new NoteSparkle; })

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;
    bool RequiresWorldSpaceCoordinates() const override { return false; }

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

protected:
    float EvaluateDrive(float amplitude, float time);

    AudioReactiveSettings3D audio_settings = MakeDefaultHighSparkleAudioReactiveSettings3D();
    float smoothed = 0.0f;
    float last_intensity_time = std::numeric_limits<float>::lowest();

    float particle_amount = 0.72f; /* 0..1 */
    float turbulence = 0.45f;      /* 0..1 */
    float hull_size = 0.32f;       /* relative shell radius */

    SpatialVolumeFieldAssist volume_assist_;
};

#endif
