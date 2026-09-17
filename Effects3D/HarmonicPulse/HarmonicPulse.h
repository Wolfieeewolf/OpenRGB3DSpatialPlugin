// SPDX-License-Identifier: GPL-2.0-only

#ifndef HARMONICPULSE_H
#define HARMONICPULSE_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Shaders/SpatialVolumeFieldAssist.h"

class QSlider;
class QComboBox;

class HarmonicPulse : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit HarmonicPulse(QWidget* parent = nullptr);
    ~HarmonicPulse() override;

    EFFECT_REGISTERER_3D("HarmonicPulse", "Harmonic Pulse", "Spatial", []() { return new HarmonicPulse; });

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
private:
    enum ColorMode {
        COLOR_MONO = 0,
        COLOR_DUO_SNAP,
        COLOR_DUO_BLEND,
        COLOR_MULTI,
        COLOR_COUNT
    };

    static const char* ColorModeName(int m);
    static RGBColor ScaleColor(RGBColor c, float bright);

    int color_mode = COLOR_DUO_SNAP;
    float zoom_wobble_strength = 0.55f;
    float flow_amount = 1.15f;
    float pulse_contrast = 0.85f;
    float spatial_amount = 0.55f;

    QComboBox* color_mode_combo = nullptr;
    QSlider* wobble_slider = nullptr;
    QSlider* flow_slider = nullptr;
    QSlider* contrast_slider = nullptr;
    QSlider* spatial_slider = nullptr;
    SpatialVolumeFieldAssist volume_assist_;
};

#endif
