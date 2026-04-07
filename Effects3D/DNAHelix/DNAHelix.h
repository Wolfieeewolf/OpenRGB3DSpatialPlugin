// SPDX-License-Identifier: GPL-2.0-only

#ifndef DNAHELIX_H
#define DNAHELIX_H

#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class DNAHelix : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit DNAHelix(QWidget* parent = nullptr);
    ~DNAHelix();

    EFFECT_REGISTERER_3D("DNAHelix", "DNA Helix", "Spatial", [](){return new DNAHelix;});

    static std::string const ClassName() { return "DNAHelix"; }
    static std::string const UIName() { return "DNA Helix"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnDNAParameterChanged();

private:
    QSlider*        radius_slider;
    QLabel*         radius_label;

    unsigned int    helix_radius;
    float           progress;
};

#endif
