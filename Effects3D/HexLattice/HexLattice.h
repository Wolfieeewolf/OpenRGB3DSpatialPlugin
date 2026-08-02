// SPDX-License-Identifier: GPL-2.0-only

#ifndef HEXLATTICE_H
#define HEXLATTICE_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "Shaders/SpatialVolumeFieldAssist.h"

class QSlider;
class QComboBox;

class HexLattice : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit HexLattice(QWidget* parent = nullptr);
    ~HexLattice() override;

    EFFECT_REGISTERER_3D("HexLattice", "Hex Lattice", "Spatial", []() { return new HexLattice; });

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void PrepareGpuFields(std::uint64_t render_sequence, float time_sec, const GridContext3D& grid) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
private:
    static RGBColor Hsv01ToBgr(float h, float s, float v);
    void EvaluateHexFieldCpu(float nx, float ny, float nz, float flow_t, float hue_t,
                             float detail_norm, float* out_v, float* out_h01) const;

    QSlider* breathing_amount_slider = nullptr;
    QSlider* pulse_amount_slider = nullptr;
    QComboBox* flow_mode_combo = nullptr;
    QSlider* turbulence_amount_slider = nullptr;
    float breathing_amount = 0.35f;
    float pulse_amount = 0.25f;
    int flow_mode = 0; // Calm
    float turbulence_amount = 0.15f;
    SpatialVolumeFieldAssist volume_assist_;
};

#endif
