// SPDX-License-Identifier: GPL-2.0-only

#ifndef DNAHELIX_H
#define DNAHELIX_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"
#include "Shaders/SpatialVolumeFieldAssist.h"

class QComboBox;
class QSlider;

class DNAHelix : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit DNAHelix(QWidget* parent = nullptr);
    ~DNAHelix() override;

    EFFECT_REGISTERER_3D("DNAHelix", "DNA Helix", "Spatial", []() { return new DNAHelix; });

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
private:
    enum Shape {
        SHAPE_HELIX = 0,
        SHAPE_ROPE,
        SHAPE_RIBBONS,
        SHAPE_LADDER,
        SHAPE_COUNT
    };

    static const char* ShapeName(int s);

    int helix_shape_mode = SHAPE_HELIX;
    float helix_radius_pct = 55.0f;
    float twist_amount = 2.4f;
    float strand_thickness_pct = 28.0f;
    float rung_amount_pct = 55.0f;

    QComboBox* shape_combo = nullptr;
    QSlider* radius_slider = nullptr;
    QSlider* twist_slider = nullptr;
    QSlider* thickness_slider = nullptr;
    QSlider* rung_slider = nullptr;
    SpatialVolumeFieldAssist volume_assist_;
};

#endif
