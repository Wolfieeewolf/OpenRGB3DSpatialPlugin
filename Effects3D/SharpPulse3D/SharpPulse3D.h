// SPDX-License-Identifier: GPL-2.0-only

#ifndef SHARPPULSE3D_H
#define SHARPPULSE3D_H

#include "EffectRegisterer3D.h"
#include "SpatialEffect3D.h"

class StripKernelColormapPanel;

class SharpPulse3D : public SpatialEffect3D
{
    Q_OBJECT

public:
    explicit SharpPulse3D(QWidget* parent = nullptr);
    ~SharpPulse3D() override;

    EFFECT_REGISTERER_3D("SharpPulse3D", "Sharp Pulse 3D", "Spatial", []() { return new SharpPulse3D; });

    static std::string const ClassName() { return "SharpPulse3D"; }
    static std::string const UIName() { return "Sharp Pulse 3D"; }

    EffectInfo3D GetEffectInfo() override;
    void SetupCustomUI(QWidget* parent) override;
    void UpdateParams(SpatialEffectParams& params) override;
    RGBColor CalculateColorGrid(float x, float y, float z, float time, const GridContext3D& grid) override;

    nlohmann::json SaveSettings() const override;
    void LoadSettings(const nlohmann::json& settings) override;

private slots:
    void SyncStripColormapFromPanel();

private:
    StripKernelColormapPanel* strip_cmap_panel = nullptr;
    bool sharppulse_strip_cmap_on = false;
    int sharppulse_strip_cmap_kernel = 0;
    float sharppulse_strip_cmap_rep = 4.0f;
    int sharppulse_strip_cmap_unfold = 0;
    float sharppulse_strip_cmap_dir = 0.0f;
    int sharppulse_strip_cmap_color_style = 0;
};

#endif
