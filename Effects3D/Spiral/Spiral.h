// SPDX-License-Identifier: GPL-2.0-only

#ifndef SPIRAL_H
#define SPIRAL_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Shaders/SpatialVolumeFieldAssist.h"

class QSlider;
class QLabel;
class QWidget;
class QComboBox;
class Spiral : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit Spiral(QWidget* parent = nullptr);
    ~Spiral();

    EFFECT_REGISTERER_3D("Spiral", "Spiral", "Spatial", [](){return new Spiral;});

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnSpiralParameterChanged();
private:
    static constexpr int kSpiralPatternCount = 6;

    QSlider*   arms_slider = nullptr;
    QComboBox* pattern_combo = nullptr;
    QSlider*   gap_slider = nullptr;
    QSlider*   coil_slider = nullptr;
    QSlider*   height_coil_slider = nullptr;

    unsigned int    num_arms;
    int             pattern_type;
    unsigned int    gap_size;
    unsigned int    coil_amount = 25;       // 0 = straight spin rays, 100 = tight spiral
    unsigned int    height_coil_amount = 15; // 0 = flat spin, 100 = strong vertical helix
    float           progress;
    SpatialVolumeFieldAssist volume_assist_;
};

#endif
