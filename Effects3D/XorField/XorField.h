// SPDX-License-Identifier: GPL-2.0-only

#ifndef XORFIELD_H
#define XORFIELD_H

#include "EffectRegisterer3D.h"
#include "SpatialEffect3D.h"

class QSlider;
class XorField : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit XorField(QWidget* parent = nullptr);
    ~XorField() override;

    EFFECT_REGISTERER_3D("XorField", "Xor Field", "Spatial", []() { return new XorField; });

    static std::string const ClassName() { return "XorField"; }
    static std::string const UIName() { return "Xor Field"; }

    EffectInfo3D GetEffectInfo() const override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private:
    float direction_alternate = 1.0f;
    float cell_scale = 5.0f;
    float wave_drive = 1.0f;

    QSlider* alt_slider = nullptr;
    QSlider* cell_slider = nullptr;
    QSlider* drive_slider = nullptr;
private slots:
};

#endif
