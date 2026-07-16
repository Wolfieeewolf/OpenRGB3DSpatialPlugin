// SPDX-License-Identifier: GPL-2.0-only

#ifndef DNAHELIX_H
#define DNAHELIX_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"
#include "EffectStratumBlend.h"

class QComboBox;
class DNAHelix : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit DNAHelix(QWidget* parent = nullptr);
    ~DNAHelix();

    EFFECT_REGISTERER_3D("DNAHelix", "DNA Helix", "Spatial", [](){return new DNAHelix;});

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnDNAParameterChanged();
private:
    QSlider*   radius_slider = nullptr;
    QComboBox* shape_combo = nullptr;
    QSlider*   vertical_drift_slider = nullptr;
    QSlider*   ring_pulse_slider = nullptr;
    QComboBox* pulse_dir_combo = nullptr;
    QSlider*   plane_count_slider = nullptr;

    unsigned int    helix_radius = 180;
    float           progress = 0.0f;
    int             helix_shape_mode = 0;
    int             vertical_drift_pct = 0;
    int             ring_pulse_pct = 0;
    int             ring_pulse_dir = 0;
    int             plane_layers = 8;
};

#endif
