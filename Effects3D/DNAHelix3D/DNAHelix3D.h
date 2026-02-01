// SPDX-License-Identifier: GPL-2.0-only

#ifndef DNAHELIX3D_H
#define DNAHELIX3D_H

#include <QWidget>
#include <QSlider>
#include <QLabel>
#include "SpatialEffect3D.h"
#include "EffectRegisterer3D.h"

class DNAHelix3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit DNAHelix3D(QWidget* parent = nullptr);
    ~DNAHelix3D();

    // Auto-registration system
    EFFECT_REGISTERER_3D("DNAHelix3D", "3D DNA Helix", "3D Spatial", [](){return new DNAHelix3D;});

    static std::string const ClassName() { return "DNAHelix3D"; }
    static std::string const UIName() { return "3D DNA Helix"; }

    // Pure virtual implementations
    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColor(float x, float y, float z, float time) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    // Settings persistence
    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void OnDNAParameterChanged();

private:
    // DNA-specific controls
    QSlider*        radius_slider;
    QLabel*         radius_label;

    /*---------------------------------------------------------*\
    | DNA-specific parameters                                  |
    | (frequency, rainbow_mode, colors are in base class)     |
    \*---------------------------------------------------------*/
    unsigned int    helix_radius;
    float           progress;
};

#endif
